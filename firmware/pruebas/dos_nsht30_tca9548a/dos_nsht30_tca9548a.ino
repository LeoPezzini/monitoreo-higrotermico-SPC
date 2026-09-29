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
  Serial.println("=== PRUEBA 2 NSHT30 + TCA9548A ===");
  Serial.println("Sensor 1 -> canal 0");
  Serial.println("Sensor 2 -> canal 1");
  Serial.println();
}

void loop() {
  float temp1, hum1;
  float temp2, hum2;

  if (seleccionarCanal(0)) {
    if (leerNSHT30(temp1, hum1)) {
      Serial.print("Sensor 1 | T = ");
      Serial.print(temp1, 2);
      Serial.print(" C | HR = ");
      Serial.print(hum1, 2);
      Serial.println(" %");
    } else {
      Serial.println("Sensor 1 | ERROR DE LECTURA");
    }
  } else {
    Serial.println("Sensor 1 | ERROR TCA CANAL 0");
  }

  if (seleccionarCanal(1)) {
    if (leerNSHT30(temp2, hum2)) {
      Serial.print("Sensor 2 | T = ");
      Serial.print(temp2, 2);
      Serial.print(" C | HR = ");
      Serial.print(hum2, 2);
      Serial.println(" %");
    } else {
      Serial.println("Sensor 2 | ERROR DE LECTURA");
    }
  } else {
    Serial.println("Sensor 2 | ERROR TCA CANAL 1");
  }

  Serial.println("-----------------------------");
  delay(2000);
}
