#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>

const char* ssid = "TU_RED_2_4_GHZ";
const char* password = "TU_PASSWORD";

#define SDA_PIN 21
#define SCL_PIN 22
#define PCA_ADDR 0x70
#define NSHT_ADDR 0x44

bool seleccionarCanal(uint8_t canal) {
  if (canal > 7) return false;
  Wire.beginTransmission(PCA_ADDR);
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(SDA_PIN, SCL_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando al WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi conectado. IP: ");
  Serial.println(WiFi.localIP());

  float temperatura, humedad;
  if (!seleccionarCanal(0) || !leerNSHT30(temperatura, humedad)) {
    Serial.println("ERROR leyendo S01");
    return;
  }

  String datos = "{";
  datos += "\"sensor\":\"S01\",";
  datos += "\"temperatura\":" + String(temperatura, 2) + ",";
  datos += "\"humedad\":" + String(humedad, 2);
  datos += "}";

  Serial.println("JSON generado:");
  Serial.println(datos);

  HTTPClient http;
  http.begin("http://httpbin.org/post");
  http.addHeader("Content-Type", "application/json");
  int codigoHTTP = http.POST(datos);

  Serial.print("Codigo HTTP: ");
  Serial.println(codigoHTTP);
  if (codigoHTTP > 0) Serial.println(http.getString());
  http.end();
}
void loop() {}
