#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"

// PRUEBA DE BANCO: 4 NSHT30 + CRC + RTC + SD + HTTP a InfluxDB 2.7.
// Un ciclo cada 30 segundos, durante 10 minutos (20 ciclos).
// Requiere secrets.h local. NO subir ese archivo con credenciales.
// NO implementa reenvio ni confirmacion persistente de pendientes.
#define SDA_SENSORES 21
#define SCL_SENSORES 22
#define SDA_RTC 16
#define SCL_RTC 17
#define SD_CS 26
#define SD_MOSI 13
#define SD_SCK 14
#define SD_MISO 27
#define PCA1_ADDR 0x70
#define PCA2_ADDR 0x71
#define NSHT_ADDR 0x44

const uint32_t DESFASE_UTC = 3UL * 3600UL; // RTC en hora de Argentina, UTC-3
const unsigned long INTERVALO_MS = 30000UL;
const uint8_t MAX_CICLOS = 20;
const char *ARCHIVO_CSV = "/mediciones_http.csv";
const char *ARCHIVO_LP = "/mediciones_http.lp"; // Historico; NO es cola de pendientes
const char *ARCHIVO_ESTADOS = "/envios_http.csv";
const char *INFLUX_URL =
  "http://192.168.1.40:8086/api/v2/write"
  "?org=monitoreo-higrotermico&bucket=sensores-prueba&precision=s";

TwoWire I2C_RTC = TwoWire(1);
RTC_DS3231 rtc;
SPIClass spiSD(VSPI);
bool listo = false;
unsigned long ultimoCiclo = 0;
uint8_t ciclos = 0;

struct Sensor { const char *nombre; uint8_t pca; uint8_t canal; };
Sensor sensores[] = {
  {"S01", PCA1_ADDR, 0}, {"S02", PCA1_ADDR, 1},
  {"S03", PCA2_ADDR, 0}, {"S04", PCA2_ADDR, 1}
};

bool seleccionarCanal(uint8_t pca, uint8_t canal) {
  uint8_t otro = pca == PCA1_ADDR ? PCA2_ADDR : PCA1_ADDR;
  Wire.beginTransmission(otro);
  Wire.write(0);
  if (Wire.endTransmission() != 0) return false;
  Wire.beginTransmission(pca);
  Wire.write(1 << canal);
  return Wire.endTransmission() == 0;
}

uint8_t calcularCRC(const uint8_t *datos, uint8_t longitud) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < longitud; i++) {
    crc ^= datos[i];
    for (uint8_t b = 0; b < 8; b++)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

bool leerNSHT30(float &temperatura, float &humedad) {
  Wire.beginTransmission(NSHT_ADDR);
  Wire.write(0x2C);
  Wire.write(0x06);
  uint8_t error = Wire.endTransmission();
  if (error) {
    Serial.printf("ERROR I2C: %u\n", error);
    return false;
  }
  delay(20);
  if (Wire.requestFrom((uint8_t)NSHT_ADDR, (uint8_t)6) != 6) {
    Serial.println("ERROR: lectura incompleta");
    return false;
  }
  uint8_t d[6];
  for (uint8_t i = 0; i < 6; i++) d[i] = Wire.read();
  if (calcularCRC(d, 2) != d[2] || calcularCRC(d + 3, 2) != d[5]) {
    Serial.println("ERROR CRC: medicion descartada");
    return false;
  }
  uint16_t rawT = ((uint16_t)d[0] << 8) | d[1];
  uint16_t rawH = ((uint16_t)d[3] << 8) | d[4];
  temperatura = -45.0f + 175.0f * rawT / 65535.0f;
  humedad = 100.0f * rawH / 65535.0f;
  return true;
}

String fechaLocal(const DateTime &f) {
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
           f.year(), f.month(), f.day(), f.hour(), f.minute(), f.second());
  return String(buf);
}

bool guardarLinea(const char *ruta, const String &linea) {
  File f = SD.open(ruta, FILE_APPEND);
  if (!f) return false;
  size_t n = f.print(linea);
  n += f.print('\n');
  f.close();
  return n == linea.length() + 1;
}

bool enviarHTTP(const String &linea, int &codigo) {
  codigo = -1;
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    Serial.println("Wi-Fi desconectado; registro solo local");
    return false;
  }
  HTTPClient http;
  if (!http.begin(INFLUX_URL)) return false;
  http.setTimeout(5000);
  http.addHeader("Authorization", String("Token ") + INFLUX_TOKEN);
  http.addHeader("Content-Type", "text/plain; charset=utf-8");
  codigo = http.POST(linea);
  if (codigo > 0 && codigo != 204) {
    Serial.print("Respuesta InfluxDB: ");
    Serial.println(http.getString());
  }
  http.end();
  return codigo == 204;
}

void ciclo() {
  DateTime ahora = rtc.now();
  String fecha = fechaLocal(ahora);
  uint32_t unixUTC = ahora.unixtime() + DESFASE_UTC;
  Serial.printf("\n=== CICLO %u/%u | %s | UTC %lu ===\n",
                ciclos + 1, MAX_CICLOS, fecha.c_str(), (unsigned long)unixUTC);

  for (const Sensor &s : sensores) {
    Serial.printf("\nSensor %s\n", s.nombre);
    if (!seleccionarCanal(s.pca, s.canal)) {
      Serial.println("ERROR PCA; sensor omitido");
      continue;
    }
    float t, h;
    if (!leerNSHT30(t, h)) {
      Serial.println("MEDICION INVALIDA; sensor omitido");
      continue;
    }
    String lp = "higrotermico,sensor=" + String(s.nombre) +
                " temperature_c=" + String(t, 2) +
                ",humidity_rh=" + String(h, 2) +
                " " + String(unixUTC);
    String csv = fecha + "," + s.nombre + "," +
                 String(t, 2) + "," + String(h, 2) + ",OK";
    bool csvOK = guardarLinea(ARCHIVO_CSV, csv);
    bool lpOK = guardarLinea(ARCHIVO_LP, lp);
    Serial.printf("T=%.2f C | HR=%.2f %% | CSV=%s | LP=%s\n",
                  t, h, csvOK ? "OK" : "ERROR", lpOK ? "OK" : "ERROR");
    if (!csvOK || !lpOK) {
      Serial.println("ERROR SD: no se envia sin respaldo local completo");
      continue;
    }
    int codigo;
    bool aceptado = enviarHTTP(lp, codigo);
    Serial.printf("HTTP: %d | %s\n", codigo, aceptado ? "ACEPTADO" : "NO CONFIRMADO");
    String estado = fecha + "," + s.nombre + "," +
                    String(unixUTC) + "," + String(codigo) + "," +
                    (aceptado ? "ACEPTADO" : "NO_CONFIRMADO");
    if (!guardarLinea(ARCHIVO_ESTADOS, estado))
      Serial.println("ERROR guardando estado HTTP en SD");
  }
  ciclos++;
  Serial.println("FIN DEL CICLO");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nPRUEBA INTEGRADA 4 NSHT30 -> SD + INFLUXDB");
  Wire.begin(SDA_SENSORES, SCL_SENSORES);
  Wire.setClock(100000);
  I2C_RTC.begin(SDA_RTC, SCL_RTC, 100000);
  if (!rtc.begin(&I2C_RTC) || rtc.lostPower()) {
    Serial.println("ERROR RTC o fecha invalida; verificar y ajustar por separado");
    return;
  }
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("ERROR microSD");
    return;
  }
  if (!SD.exists(ARCHIVO_CSV))
    guardarLinea(ARCHIVO_CSV, "timestamp_local,sensor,temperatura_C,humedad_RH,estado");
  if (!SD.exists(ARCHIVO_ESTADOS))
    guardarLinea(ARCHIVO_ESTADOS, "timestamp_local,sensor,unix_utc,http_codigo,estado");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000UL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi OK. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi no disponible: se conserva registro local");
  }
  listo = true;
  ciclo();
  ultimoCiclo = millis();
}

void loop() {
  if (!listo || ciclos >= MAX_CICLOS) return;
  if (millis() - ultimoCiclo >= INTERVALO_MS) {
    ultimoCiclo = millis();
    ciclo();
  }
}
