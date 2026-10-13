#include <Wire.h>

// Prueba de CRC-8 de cuatro NSHT30 conectados a dos PCA9548A.
// Validada en banco el 2026-10-13. No usa Wi-Fi, RTC ni microSD.
#define SDA_SENSORES 21
#define SCL_SENSORES 22
#define PCA1_ADDR 0x70
#define PCA2_ADDR 0x71
#define NSHT_ADDR 0x44

bool seleccionarCanal(uint8_t direccionPCA, uint8_t canal) {
  if (canal > 7) return false;
  uint8_t otroPCA = (direccionPCA == PCA1_ADDR) ? PCA2_ADDR : PCA1_ADDR;
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

void probarSensor(const char *nombre, uint8_t direccionPCA, uint8_t canal) {
  Serial.println();
  Serial.print("Sensor: ");
  Serial.println(nombre);

  if (!seleccionarCanal(direccionPCA, canal)) {
    Serial.println("ERROR: seleccion del PCA");
    return;
  }

  Wire.beginTransmission(NSHT_ADDR);
  Wire.write(0x2C);
  Wire.write(0x06);
  uint8_t errorI2C = Wire.endTransmission();
  if (errorI2C != 0) {
    Serial.print("ERROR I2C del sensor. Codigo: ");
    Serial.println(errorI2C);
    return;
  }

  delay(20);
  uint8_t recibidos = Wire.requestFrom((uint8_t)NSHT_ADDR, (uint8_t)6);
  if (recibidos != 6) {
    Serial.print("ERROR: bytes recibidos = ");
    Serial.println(recibidos);
    return;
  }

  uint8_t datos[6];
  for (uint8_t i = 0; i < 6; i++) datos[i] = Wire.read();

  bool tempOK = calcularCRC(&datos[0], 2) == datos[2];
  bool humOK = calcularCRC(&datos[3], 2) == datos[5];

  Serial.print("CRC temperatura: ");
  Serial.println(tempOK ? "OK" : "ERROR");
  Serial.print("CRC humedad:     ");
  Serial.println(humOK ? "OK" : "ERROR");

  if (!tempOK || !humOK) {
    Serial.println("MEDICION DESCARTADA");
    return;
  }

  uint16_t rawTemp = ((uint16_t)datos[0] << 8) | datos[1];
  uint16_t rawHum = ((uint16_t)datos[3] << 8) | datos[4];
  float temperatura = -45.0 + 175.0 * rawTemp / 65535.0;
  float humedad = 100.0 * rawHum / 65535.0;

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.println(" C");
  Serial.print("Humedad: ");
  Serial.print(humedad, 2);
  Serial.println(" %");
  Serial.println("MEDICION VALIDA");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(SDA_SENSORES, SCL_SENSORES);
  Wire.setClock(100000);
  Serial.println();
  Serial.println("==============================");
  Serial.println(" PRUEBA CRC-8 NSHT30");
  Serial.println(" 2 PCA9548A + 4 sensores");
  Serial.println("==============================");
}

void loop() {
  probarSensor("S01", PCA1_ADDR, 0);
  probarSensor("S02", PCA1_ADDR, 1);
  probarSensor("S03", PCA2_ADDR, 0);
  probarSensor("S04", PCA2_ADDR, 1);
  Serial.println();
  Serial.println("==============================");
  Serial.println(" FIN DEL CICLO");
  Serial.println("==============================");
  delay(5000);
}
