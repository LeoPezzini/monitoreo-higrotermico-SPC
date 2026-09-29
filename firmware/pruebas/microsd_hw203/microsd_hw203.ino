#include <SPI.h>
#include <SD.h>

#define SD_MOSI 13
#define SD_MISO 27
#define SD_SCK  14
#define SD_CS   26

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== PRUEBA MICROSD HW-203 ===");

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  Serial.print("Inicializando microSD... ");

  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("ERROR");
    Serial.println("No se pudo inicializar la tarjeta.");
    return;
  }

  Serial.println("OK");

  File archivo = SD.open("/prueba.txt", FILE_WRITE);

  if (!archivo) {
    Serial.println("ERROR: no se pudo abrir prueba.txt");
    return;
  }

  archivo.println("Prueba microSD OK");
  archivo.close();

  Serial.println("Archivo escrito correctamente.");

  archivo = SD.open("/prueba.txt");

  if (!archivo) {
    Serial.println("ERROR: no se pudo volver a abrir prueba.txt");
    return;
  }

  Serial.println();
  Serial.println("Contenido de prueba.txt:");
  Serial.println("------------------------");

  while (archivo.available()) {
    Serial.write(archivo.read());
  }

  archivo.close();

  Serial.println("------------------------");
  Serial.println("PRUEBA FINALIZADA");
}

void loop() {
}
