# Firmware de producción

Fuente `SPC_JASON_PRODUCCION_10min_FINAL.ino`, recibida en el paquete del 18/11/2026 y confirmada por Leonardo el 23/11 como instalada sin cambios. SHA256 del archivo original y del publicado `b0cc638cfd38411b9597b2e0359ea510c7b7fcc0fb94d938e4e358deb452e74f`.

## Preparación

Abrir el sketch en Arduino IDE desde esta carpeta. Copiar `secrets.example.h` como `secrets.h` y completar localmente Wi-Fi y URL `http://IP_SERVIDOR:8080/api/v2/write`. El archivo de secretos queda fuera de Git. Las dependencias del código son Arduino-ESP32, Wire, SPI, SD, WiFi, HTTPClient, RTClib, Adafruit GFX y Adafruit SSD1306. Las versiones exactas del core y librerías usadas para la carga original aún deben registrarse.

El ajuste normal es `AJUSTAR_RTC_UNA_VEZ=false`. El RTC contiene hora local argentina; el firmware suma 10800 segundos al construir el tiempo UTC. Al arrancar sincroniza también la fecha FAT del ESP32 desde el RTC y registra el motivo de reinicio.

## Parámetros

| Parámetro | Valor |
| --- | --- |
| Muestreo | Primer ciclo al arrancar y luego cada 600000 ms |
| Intento fallido | Espera nominal 10000 ms, además del tiempo de operación |
| Entrega exitosa | Pausa 100 ms antes del próximo registro |
| Timeout HTTP | 2000 ms |
| Margen antes de medir | 2500 ms sin iniciar una nueva transacción HTTP |
| OLED | 15 s ambientes, 5 s estado, fallas con prioridad |
| Eventos | Umbral 10 MiB; activo y dos rotaciones |

El programa intenta como máximo una transmisión por pasada. Los pendientes se procesan en orden; solo un 204 permite persistir el ACK. Un 409 o 422 requiere diagnóstico y mantiene la cabecera de la cola pendiente.

## Almacenamiento y recuperación

Consultar el [inventario de archivos](../../README.md#persistencia-en-microsd). El log incluye `BOOT`, `WIFI_UP`, `WIFI_DOWN`, `SENSOR_ERROR`, `SENSOR_RECUPERADO`, `HTTP_ERROR`, `HTTP_RECUPERADO`, `COLA_VACIA` y eventos de RTC/SD cuando es posible escribirlos. Las repeticiones de algunas fallas se condensan en cambios de estado. Los detalles de error de transacción se observan además por serie.

El arranque inspecciona la cola completa y el ACK. Si la cola termina incompleta o el ACK excede el número de filas, se detiene la adquisición hasta revisar una copia de los datos. Un fallo de CSV activa error de almacenamiento y bloquea el envío; un fallo al agregar a la cola deshabilita la SD y detiene la adquisición. El firmware requiere RTC y SD válidos para iniciar el ciclo. La ausencia de OLED permite continuar con diagnóstico por serie.

CSV, cola y ACK son append-only y carecen de compactación automática. El borrado de la cola reinicia la secuencia de ID y puede colisionar con registros remotos del mismo equipo y ubicación. Toda intervención debe preservar o migrar coherentemente el espacio de identidades y sus respaldos.

## Programación y alimentación

Aplicar el [procedimiento del jumper](../../docs/operacion/guia-fede.md#jumper-y-conexión-usb). El conector físico debe identificarse en la preperforada con Leonardo; el esquema ilustrativo de presentación contiene conexiones simplificadas.

Ahorro de energía, deep-sleep y espera de varios minutos durante una caída permanecen propuestos. Esta fuente se conserva tal como fue recibida; la revisión documental no constituye una nueva compilación ni una prueba sobre el equipo instalado.
