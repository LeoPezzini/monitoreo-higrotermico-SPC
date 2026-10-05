#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

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

bool seleccionarCanal(uint8_t direccionPCA, uint8_t canal) {
  if (canal > 7) return false;
  Wire.beginTransmission(direccionPCA);
  Wire.write(1 << canal);
  return Wire.endTransmission() == 0;
}

bool leerNSHT30(float &temperatura, float &humedad) {
  Wire.beginTransmission(NSHT_ADDR);
  Wire.write(0x2C);
  Wire.write(0x06);
  if (Wire.endTransmission() != 0) return false;
  delay(20);
  Wire.requestFrom(NSHT_ADDR, 6);
  if (Wire.available() != 6) return false;

  uint16_t rawTemp = ((uint16_t)Wire.read() << 8) | Wire.read();
  uint8_t crcTemp = Wire.read();
  uint16_t rawHum = ((uint16_t)Wire.read() << 8) | Wire.read();
  uint8_t crcHum = Wire.read();
  (void)crcTemp;
  (void)crcHum;

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

void guardarSensor(const char* nombre, uint8_t direccionPCA, uint8_t canal,
                   const String &timestamp) {
  float temperatura, humedad;
  Serial.print(nombre);
  Serial.print(" | ");

  if (!seleccionarCanal(direccionPCA, canal)) {
    Serial.println("ERROR PCA");
    return;
  }
  if (!leerNSHT30(temperatura, humedad)) {
    Serial.println("ERROR SENSOR");
    return;
  }

  Serial.print(timestamp);
  Serial.print(" | T = ");
  Serial.print(temperatura, 2);
  Serial.print(" C | HR = ");
  Serial.print(humedad, 2);
  Serial.println(" %");

  File archivo = SD.open("/mediciones_4sensores.csv", FILE_APPEND);
  if (!archivo) {
    Serial.println("ERROR abriendo archivo SD");
    return;
  }
  archivo.print(timestamp); archivo.print(",");
  archivo.print(nombre); archivo.print(",");
  archivo.print(temperatura, 2); archivo.print(",");
  archivo.print(humedad, 2); archivo.print(",");
  archivo.println("OK");
  archivo.close();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println(" 2 PCA + 4 NSHT30 + RTC + microSD");
  Serial.println("==========================================");

  Wire.begin(SDA_SENSORES, SCL_SENSORES);
  Serial.println("I2C sensores iniciado.");

  I2C_RTC.begin(SDA_RTC, SCL_RTC, 100000);
  if (!rtc.begin(&I2C_RTC)) {
    Serial.println("ERROR: RTC no encontrado.");
    return;
  }
  Serial.println("RTC encontrado.");

  // SOLO PARA ESTA PRUEBA SIN BATERIA:
  // reajusta el RTC a fecha/hora de compilacion en cada arranque.
  // Quitar esta linea en el firmware definitivo.
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  Serial.println("RTC ajustado a fecha/hora de compilacion.");

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("ERROR: microSD no inicializada.");
    return;
  }
  Serial.println("microSD inicializada.");

  if (!SD.exists("/mediciones_4sensores.csv")) {
    File archivo = SD.open("/mediciones_4sensores.csv", FILE_WRITE);
    if (archivo) {
      archivo.println("timestamp,sensor,temperatura_C,humedad_RH,estado");
      archivo.close();
      Serial.println("Archivo CSV creado.");
    }
  }

  String timestamp = obtenerTimestamp();
  Serial.print("\nCiclo de medicion: ");
  Serial.println(timestamp);

  guardarSensor("S01", PCA1_ADDR, 0, timestamp);
  guardarSensor("S02", PCA1_ADDR, 1, timestamp);
  guardarSensor("S03", PCA2_ADDR, 0, timestamp);
  guardarSensor("S04", PCA2_ADDR, 1, timestamp);

  Serial.println("\n==========================================");
  Serial.println(" CICLO GUARDADO");
  Serial.println("==========================================");
}
void loop() {}
