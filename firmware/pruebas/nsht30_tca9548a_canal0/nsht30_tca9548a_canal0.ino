#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define TCA_ADDR  0x70
#define NSHT_ADDR 0x44

bool seleccionarCanal(uint8_t canal) {
  if (canal > 7) return false;

  Wire.beginTransmission(TCA_ADDR);
  Wire.write(1 << canal);
  return Wire.endTransmission() == 0;
}

bool existeI2C(uint8_t direccion) {
  Wire.beginTransmission(direccion);
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

  uint16_t rawTemp = (Wire.read() << 8) | Wire.read();
  uint8_t crcTemp = Wire.read();

  uint16_t rawHum = (Wire.read() << 8) | Wire.read();
  uint8_t crcHum = Wire.read();

  // CRC recibido pero todavía no validado en esta prueba.
  (void)crcTemp;
  (void)crcHum;

  temperatura = -45.0 + 175.0 * rawTemp / 65535.0;
  humedad = 100.0 * rawHum / 65535.0;

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println();
  Serial.println("=== PRUEBA TCA9548A + NSHT30 ===");

  Serial.print("Buscando TCA9548A en 0x70... ");
  if (!existeI2C(TCA_ADDR)) {
    Serial.println("NO ENCONTRADO");
    return;
  }
  Serial.println("OK");

  Serial.print("Seleccionando canal 0... ");
  if (!seleccionarCanal(0)) {
    Serial.println("ERROR");
    return;
  }
  Serial.println("OK");

  Serial.print("Buscando NSHT30 en 0x44... ");
  if (!existeI2C(NSHT_ADDR)) {
    Serial.println("NO ENCONTRADO");
    return;
  }
  Serial.println("OK");
}

void loop() {
  if (!seleccionarCanal(0)) {
    Serial.println("ERROR TCA");
    delay(2000);
    return;
  }

  float temperatura;
  float humedad;

  if (leerNSHT30(temperatura, humedad)) {
    Serial.print("T = ");
    Serial.print(temperatura, 2);
    Serial.print(" C    HR = ");
    Serial.print(humedad, 2);
    Serial.println(" %");
  } else {
    Serial.println("ERROR leyendo NSHT30");
  }

  delay(2000);
}
