#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define NSHT30_ADDR 0x44

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("\n=== Prueba NSHT30 directo ===");
}

void loop() {
  Wire.beginTransmission(NSHT30_ADDR);
  Wire.write(0x2C);
  Wire.write(0x06);

  if (Wire.endTransmission() != 0) {
    Serial.println("Error comunicando con NSHT30");
    delay(2000);
    return;
  }

  delay(20);

  Wire.requestFrom(NSHT30_ADDR, 6);

  if (Wire.available() != 6) {
    Serial.println("Error: no se recibieron 6 bytes");
    delay(2000);
    return;
  }

  uint16_t rawTemp = (Wire.read() << 8) | Wire.read();
  uint8_t crcTemp = Wire.read();
  uint16_t rawHum = (Wire.read() << 8) | Wire.read();
  uint8_t crcHum = Wire.read();

  float temperatura = -45.0 + 175.0 * (rawTemp / 65535.0);
  float humedad = 100.0 * (rawHum / 65535.0);

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.print(" C    Humedad: ");
  Serial.print(humedad, 2);
  Serial.println(" %RH");

  delay(2000);
}
