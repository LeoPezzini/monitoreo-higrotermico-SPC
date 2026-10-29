# Prueba funcional OLED SSD1306 y microSD — 29/10/2026

## Objetivo
Comprobar la inicialización y visualización en una pantalla OLED SSD1306 de 0,91 pulgadas (128×32, I²C) y la escritura y lectura de una tarjeta microSD de 16 GB con un ESP32 DevKit V1 de 30 pines, sin modificar el firmware validado en el laboratorio.

## Configuración ensayada
- OLED: GND a GND, VCC a 3,3 V, SDA a GPIO21 y SCL a GPIO22; dirección I²C 0x3C.
- MicroSD HW-203, interfaz SPI según el firmware de laboratorio: CS GPIO26, MOSI GPIO13, SCK GPIO14 y MISO GPIO27.
- Se usó un sketch independiente con bibliotecas Adafruit SSD1306, Adafruit GFX, Wire, SPI y SD.
- La tarjeta de 16 GB fue reconocida por el ESP32; el programa informó 15000 MB.

## Procedimiento y evidencias
1. Se cargó una prueba OLED que mostró «SISTEMA SPC», «Monitoreo» y «OLED funcionando!». El monitor serie informó «Iniciando prueba OLED...» y «Pantalla inicializada correctamente».
2. Se cargó una prueba combinada OLED + microSD que inicializó la tarjeta, abrió `/prueba_spc.txt`, escribió «Prueba SPC: escritura y lectura OK», volvió a leer el archivo y comparó el contenido.
3. El monitor serie mostró:
   ```text
   === PRUEBA MICROSD SPC ===
   MicroSD inicializada.
   Capacidad: 15000 MB
   Contenido leido: Prueba SPC: escritura y lectura OK
   RESULTADO: PRUEBA EXITOSA
   ```
4. La pantalla mostró «OLED: OK» y «MICROSD: OK».

## Resultado
Prueba funcional inicial exitosa de pantalla OLED y almacenamiento microSD en un mismo montaje. La comprobación de escritura y lectura es puntual: no demuestra todavía estabilidad a largo plazo, resistencia a cortes de alimentación ni integridad bajo funcionamiento continuo.

## Incidencia a investigar
Durante la manipulación de cables, antes de cargar el sketch combinado, la pantalla se apagó momentáneamente. Volvió a funcionar y superó la prueba posterior. Posible falso contacto de alimentación o jumpers; causa no confirmada. Revisar conexiones y realizar ensayo de estabilidad.

## Evidencia fotográfica pendiente de incorporar
- Fotografía de la OLED con el mensaje de la primera prueba (tomada por el equipo).
- Captura del monitor serie con resultado exitoso de la prueba microSD (tomada por el equipo).
- Fotografía general del montaje, mostrando las conexiones de la OLED y el módulo microSD.

## Próximos pasos
- Conservar el firmware validado de laboratorio sin modificaciones.
- Guardar el sketch de diagnóstico como prueba independiente en el repositorio.
- Ensayar estabilidad de conexiones, montaje definitivo y alimentación con UPS/regulador.
- Integrar visualización de mediciones y estado de almacenamiento/reenvío sin bloquear la adquisición.
