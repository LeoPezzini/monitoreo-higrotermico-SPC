#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

// Prueba integrada validada en banco el 14/10/2026.
// Un ciclo de cuatro sensores al arrancar; luego muestra hora cada 5 s.
// Ajustar el RTC una sola vez cambiando esta constante a true.
// IMPORTANTE: volver a false y cargar de nuevo para probar la CR2032.
const bool AJUSTAR_RTC = false;

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

TwoWire I2C_RTC = TwoWire(1);
RTC_DS3231 rtc;
SPIClass spiSD(VSPI);
bool rtcDisponible = false;
bool sdDisponible = false;

bool seleccionarCanal(uint8_t direccionPCA, uint8_t canal) {
  if (canal > 7) return false;
  uint8_t otroPCA = direccionPCA == PCA1_ADDR ? PCA2_ADDR : PCA1_ADDR;
  Wire.beginTransmission(otroPCA);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  Wire.beginTransmission(direccionPCA);
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
  uint8_t errorI2C = Wire.endTransmission();
  if (errorI2C != 0) {
    Serial.print("ERROR I2C. Codigo: ");
    Serial.println(errorI2C);
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
  bool crcTempOK = calcularCRC(&datos[0], 2) == datos[2];
  bool crcHumOK = calcularCRC(&datos[3], 2) == datos[5];
  Serial.print("CRC temperatura: ");
  Serial.println(crcTempOK ? "OK" : "ERROR");
  Serial.print("CRC humedad: ");
  Serial.println(crcHumOK ? "OK" : "ERROR");
  if (!crcTempOK || !crcHumOK) {
    Serial.println("MEDICION DESCARTADA");
    return false;
  }
  uint16_t rawTemp = ((uint16_t)datos[0] << 8) | datos[1];
  uint16_t rawHum = ((uint16_t)datos[3] << 8) | datos[4];
  temperatura = -45.0 + 175.0 * rawTemp / 65535.0;
  humedad = 100.0 * rawHum / 65535.0;
  return true;
}

String obtenerTimestamp() {
  DateTime ahora = rtc.now();
  char buffer[25];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d",
           ahora.year(), ahora.month(), ahora.day(),
           ahora.hour(), ahora.minute(), ahora.second());
  return String(buffer);
}

void guardarSensor(const char *nombre, uint8_t direccionPCA,
                   uint8_t canal, const String &timestamp) {
  Serial.println();
  Serial.print("Sensor: ");
  Serial.println(nombre);
  if (!seleccionarCanal(direccionPCA, canal)) {
    Serial.println("ERROR: seleccion del PCA");
    return;
  }
  float temperatura, humedad;
  if (!leerNSHT30(temperatura, humedad)) {
    Serial.println("ERROR: medicion no valida");
    return;
  }
  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.println(" C");
  Serial.print("Humedad: ");
  Serial.print(humedad, 2);
  Serial.println(" %");
  Serial.println("MEDICION VALIDA");
  if (!sdDisponible) {
    Serial.println("ERROR: microSD no disponible");
    return;
  }
  File archivo = SD.open("/mediciones_4sensores.csv", FILE_APPEND);
  if (!archivo) {
    Serial.println("ERROR: apertura del CSV");
    return;
  }
  archivo.print(timestamp); archivo.print(",");
  archivo.print(nombre); archivo.print(",");
  archivo.print(temperatura, 2); archivo.print(",");
  archivo.print(humedad, 2); archivo.print(",");
  archivo.println("OK");
  archivo.close();
  Serial.println("Registro guardado en microSD");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("==============================");
  Serial.println(" 4 NSHT30 + CRC + RTC + SD");
  Serial.println("==============================");
  Wire.begin(SDA_SENSORES, SCL_SENSORES);
  Wire.setClock(100000);
  Serial.println("I2C sensores iniciado");
  I2C_RTC.begin(SDA_RTC, SCL_RTC, 100000);
  rtcDisponible = rtc.begin(&I2C_RTC);
  if (!rtcDisponible) {
    Serial.println("ERROR: RTC no encontrado");
  } else {
    Serial.println("RTC encontrado");
    if (AJUSTAR_RTC) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.println("RTC ajustado a fecha de compilacion");
    } else {
      Serial.println("RTC SIN reajuste automatico");
    }
    if (rtc.lostPower()) {
      Serial.println("AVISO: RTC informa perdida de alimentacion");
    }
    Serial.print("Hora actual RTC: ");
    Serial.println(obtenerTimestamp());
  }
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdDisponible = SD.begin(SD_CS, spiSD);
  if (!sdDisponible) {
    Serial.println("ERROR: microSD no inicializada");
  } else {
    Serial.println("microSD inicializada");
    if (!SD.exists("/mediciones_4sensores.csv")) {
      File archivo = SD.open("/mediciones_4sensores.csv", FILE_WRITE);
      if (archivo) {
        archivo.println("timestamp,sensor,temperatura_C,humedad_RH,estado");
        archivo.close();
      }
    }
  }
  if (!rtcDisponible) {
    Serial.println("Sin RTC: no se guardan mediciones");
    return;
  }
  String timestamp = obtenerTimestamp();
  Serial.println();
  Serial.print("Ciclo: ");
  Serial.println(timestamp);
  guardarSensor("S01", PCA1_ADDR, 0, timestamp);
  guardarSensor("S02", PCA1_ADDR, 1, timestamp);
  guardarSensor("S03", PCA2_ADDR, 0, timestamp);
  guardarSensor("S04", PCA2_ADDR, 1, timestamp);
  Serial.println();
  Serial.println("==============================");
  Serial.println(" FIN DEL CICLO");
  Serial.println("==============================");
}

void loop() {
  if (rtcDisponible) {
    Serial.print("Hora RTC: ");
    Serial.println(obtenerTimestamp());
  }
  delay(5000);
}
