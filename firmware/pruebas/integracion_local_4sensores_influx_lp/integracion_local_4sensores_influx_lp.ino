#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

// Prueba local validada 2026-10-14: cuatro NSHT30, CRC-8,
// RTC en hora argentina (UTC-3), CSV y Line Protocol en microSD.
// Sin Wi-Fi, sin envio y sin confirmacion de entrega remota.
// No modifica la hora del RTC.
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

const uint32_t DESFASE_UTC = 3UL * 3600UL;
const unsigned long INTERVALO_MS = 30000UL;
const char *ARCHIVO_CSV = "/mediciones_v2.csv";
const char *ARCHIVO_LP = "/influx_pendiente.lp";

TwoWire I2C_RTC = TwoWire(1);
RTC_DS3231 rtc;
SPIClass spiSD(VSPI);
bool rtcDisponible = false;
bool sdDisponible = false;
unsigned long ultimoCiclo = 0;

struct Sensor {
  const char *nombre;
  uint8_t pca;
  uint8_t canal;
};
Sensor sensores[] = {
  {"S01", PCA1_ADDR, 0},
  {"S02", PCA1_ADDR, 1},
  {"S03", PCA2_ADDR, 0},
  {"S04", PCA2_ADDR, 1}
};
const uint8_t NUM_SENSORES = sizeof(sensores) / sizeof(sensores[0]);

bool seleccionarCanal(uint8_t pca, uint8_t canal) {
  if (canal > 7) return false;
  uint8_t otroPCA = pca == PCA1_ADDR ? PCA2_ADDR : PCA1_ADDR;
  Wire.beginTransmission(otroPCA);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  Wire.beginTransmission(pca);
  Wire.write(1 << canal);
  return Wire.endTransmission() == 0;
}

uint8_t calcularCRC(const uint8_t *datos, uint8_t longitud) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < longitud; i++) {
    crc ^= datos[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x80) crc = (crc << 1) ^ 0x31;
      else crc <<= 1;
    }
  }
  return crc;
}

bool leerNSHT30(float &temperatura, float &humedad) {
  Wire.beginTransmission(NSHT_ADDR);
  Wire.write(0x2C);
  Wire.write(0x06);
  uint8_t error = Wire.endTransmission();
  if (error != 0) {
    Serial.print("ERROR I2C: ");
    Serial.println(error);
    return false;
  }
  delay(20);
  uint8_t recibidos = Wire.requestFrom((uint8_t)NSHT_ADDR, (uint8_t)6);
  if (recibidos != 6) {
    Serial.println("ERROR: lectura incompleta");
    return false;
  }
  uint8_t datos[6];
  for (uint8_t i = 0; i < 6; i++) datos[i] = Wire.read();
  bool crcT = calcularCRC(&datos[0], 2) == datos[2];
  bool crcH = calcularCRC(&datos[3], 2) == datos[5];
  if (!crcT || !crcH) {
    Serial.println("ERROR CRC: medicion descartada");
    return false;
  }
  uint16_t rawT = ((uint16_t)datos[0] << 8) | datos[1];
  uint16_t rawH = ((uint16_t)datos[3] << 8) | datos[4];
  temperatura = -45.0 + 175.0 * rawT / 65535.0;
  humedad = 100.0 * rawH / 65535.0;
  return true;
}

String fechaLocal(DateTime fecha) {
  char buffer[25];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d",
           fecha.year(), fecha.month(), fecha.day(),
           fecha.hour(), fecha.minute(), fecha.second());
  return String(buffer);
}

String generarLineProtocol(const char *sensor, float temperatura,
                           float humedad, uint32_t timestampUTC) {
  String linea = "higrotermico,sensor=";
  linea += sensor;
  linea += " temperature_c=";
  linea += String(temperatura, 2);
  linea += ",humidity_rh=";
  linea += String(humedad, 2);
  linea += " ";
  linea += String(timestampUTC);
  return linea;
}

bool agregarLinea(const char *ruta, const String &linea) {
  File archivo = SD.open(ruta, FILE_APPEND);
  if (!archivo) {
    Serial.print("ERROR abriendo ");
    Serial.println(ruta);
    return false;
  }
  size_t escritos = archivo.println(linea);
  archivo.close();
  return escritos == linea.length() + 2; // println: CRLF en ESP32
}

void mostrarArchivo(const char *ruta) {
  Serial.println();
  Serial.println("==============================");
  Serial.print("LEYENDO ARCHIVO: ");
  Serial.println(ruta);
  Serial.println("==============================");
  File archivo = SD.open(ruta, FILE_READ);
  if (!archivo) {
    Serial.println("ERROR: no se pudo abrir");
    return;
  }
  while (archivo.available()) Serial.write(archivo.read());
  archivo.close();
  Serial.println();
  Serial.println("FIN DEL ARCHIVO");
  Serial.println("==============================");
}

void realizarCiclo() {
  if (!rtcDisponible || !sdDisponible) {
    Serial.println("ERROR: RTC o SD no disponible");
    return;
  }
  DateTime ahora = rtc.now();
  String fecha = fechaLocal(ahora);
  uint32_t timestampUTC = ahora.unixtime() + DESFASE_UTC;

  Serial.println();
  Serial.println("==============================");
  Serial.print("Fecha local: ");
  Serial.println(fecha);
  Serial.print("Timestamp Unix UTC: ");
  Serial.println(timestampUTC);
  Serial.println("==============================");

  for (uint8_t i = 0; i < NUM_SENSORES; i++) {
    Sensor &s = sensores[i];
    Serial.println();
    Serial.print("Sensor: ");
    Serial.println(s.nombre);
    if (!seleccionarCanal(s.pca, s.canal)) {
      Serial.println("ERROR seleccionando PCA");
      continue;
    }
    float temperatura, humedad;
    if (!leerNSHT30(temperatura, humedad)) {
      Serial.println("MEDICION INVALIDA");
      continue;
    }
    Serial.print("Temperatura: ");
    Serial.println(temperatura, 2);
    Serial.print("Humedad: ");
    Serial.println(humedad, 2);
    String lineaLP = generarLineProtocol(s.nombre, temperatura, humedad, timestampUTC);
    String lineaCSV = fecha + "," + String(s.nombre) + "," +
                      String(temperatura, 2) + "," +
                      String(humedad, 2) + ",OK";
    Serial.println("Line Protocol:");
    Serial.println(lineaLP);
    bool csvOK = agregarLinea(ARCHIVO_CSV, lineaCSV);
    bool lpOK = agregarLinea(ARCHIVO_LP, lineaLP);
    Serial.print("CSV: ");
    Serial.println(csvOK ? "GUARDADO" : "ERROR");
    Serial.print("LP: ");
    Serial.println(lpOK ? "GUARDADO" : "ERROR");
  }
  Serial.println();
  Serial.println("==============================");
  Serial.println("FIN DEL CICLO");
  Serial.println("==============================");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("==============================");
  Serial.println(" INTEGRACION LOCAL HIGROTERMICA");
  Serial.println("==============================");
  Wire.begin(SDA_SENSORES, SCL_SENSORES);
  Wire.setClock(100000);
  I2C_RTC.begin(SDA_RTC, SCL_RTC, 100000);
  rtcDisponible = rtc.begin(&I2C_RTC);
  if (!rtcDisponible) {
    Serial.println("ERROR: RTC no encontrado");
    return;
  }
  if (rtc.lostPower()) {
    Serial.println("AVISO: RTC informa perdida de energia");
    Serial.println("Verificar fecha y hora antes de medir");
    return;
  }
  Serial.println("RTC encontrado");
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdDisponible = SD.begin(SD_CS, spiSD);
  if (!sdDisponible) {
    Serial.println("ERROR: SD no inicializada");
    return;
  }
  Serial.println("SD inicializada");
  if (!SD.exists(ARCHIVO_CSV)) {
    File archivo = SD.open(ARCHIVO_CSV, FILE_WRITE);
    if (archivo) {
      archivo.println("timestamp_local,sensor,temperatura_C,humedad_RH,estado");
      archivo.close();
    }
  }
  realizarCiclo();
  mostrarArchivo(ARCHIVO_CSV);
  mostrarArchivo(ARCHIVO_LP);
  ultimoCiclo = millis();
}

void loop() {
  if (!rtcDisponible || !sdDisponible) {
    delay(1000);
    return;
  }
  if (millis() - ultimoCiclo >= INTERVALO_MS) {
    ultimoCiclo = millis();
    realizarCiclo();
  }
}
