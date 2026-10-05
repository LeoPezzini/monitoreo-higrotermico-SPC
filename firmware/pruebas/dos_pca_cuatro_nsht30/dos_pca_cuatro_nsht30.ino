#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define PCA1_ADDR 0x70
#define PCA2_ADDR 0x71
#define NSHT_ADDR 0x44

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

void probarSensor(const char* nombre, uint8_t direccionPCA, uint8_t canal) {
  Serial.print(nombre);
  Serial.print(" | PCA 0x");
  Serial.print(direccionPCA, HEX);
  Serial.print(" | CH");
  Serial.print(canal);
  Serial.print(" | ");

  if (!seleccionarCanal(direccionPCA, canal)) {
    Serial.println("ERROR PCA");
    return;
  }

  float temperatura, humedad;
  if (!leerNSHT30(temperatura, humedad)) {
    Serial.println("ERROR SENSOR");
    return;
  }

  Serial.print("T = ");
  Serial.print(temperatura, 2);
  Serial.print(" C | HR = ");
  Serial.print(humedad, 2);
  Serial.println(" %");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("\n==========================================");
  Serial.println(" PRUEBA 2 PCA9548A + 4 NSHT30");
  Serial.println("==========================================");
  probarSensor("S01", PCA1_ADDR, 0);
  probarSensor("S02", PCA1_ADDR, 1);
  probarSensor("S03", PCA2_ADDR, 0);
  probarSensor("S04", PCA2_ADDR, 1);
  Serial.println("==========================================");
  Serial.println(" PRUEBA FINALIZADA");
  Serial.println("==========================================");
}
void loop() {}
