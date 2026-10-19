# Prueba de integración local: cola, ACK y reenvío — 2026-10-19

## Alcance y configuración

Prueba de banco con ESP32, cuatro NSHT30 (S01–S04), dos PCA9548A (0x70 y 0x71), RTC DS3231, microSD, Wi-Fi, InfluxDB 2.7 local y dashboard Grafana. Frecuencia aproximada de 30 segundos, hasta 20 ciclos por ejecución. Las mediciones válidas se guardan en CSV, Line Protocol histórico y una cola persistente.

Archivos SD usados por el sketch recibido:
- `/mediciones_http.csv`: histórico CSV
- `/mediciones_http.lp`: histórico Line Protocol, **no** la cola
- `/pendientes_v1.lp`: cola append-only de Line Protocol
- `/confirmados_v1.log`: diario de ACK en formato `N,~N`

El firmware usa `secrets.h` local (no publicar), espera HTTP 204 de InfluxDB antes de escribir ACK y solo entonces avanza el cursor. Limita los reenvíos a ocho registros por pasada. Tras los 20 ciclos sigue comprobando pendientes cada 30 segundos.

## Evidencia observada

- Una ejecución recuperó ACK persistente 4 y confirmó secuencialmente registros #5–#16.
- Ante fallos HTTP -1, el registro #17 permaneció pendiente durante varios ciclos y luego se confirmaron #17–#24.
- En otra ejecución se recuperó ACK 84; #85–#88 fueron aceptados (204), mientras #89 permaneció pendiente con HTTP -1.
- Después de un reinicio se recuperó ACK 88; #89 siguió pendiente y finalmente fueron aceptados #89–#96 con HTTP 204.
- Grafana mostró las cuatro curvas de temperatura y las cuatro de humedad relativa, con continuidad de las mediciones visibles tras la recuperación.
- El usuario confirmó una ejecución de 20 ciclos completa. Los extractos de monitor compartidos no permiten auditar todas las entradas de esa ejecución ni probar ausencia global de duplicados.

## Interpretación y limitaciones

HTTP 204 confirma aceptación de escritura por InfluxDB; el diario local marca el avance únicamente si pudo escribir ACK. Un corte entre ambas acciones puede causar reenvío del mismo punto. En InfluxDB, la coincidencia de measurement, tags y timestamp puede sobrescribir o fusionar campos; se debe verificar la semántica de idempotencia del esquema final. No afirmar entrega exactamente-una-vez.

El sketch no compacta la cola ni el diario; `recuperarACK()` recorre todo el log y `reenviarPendientes()` recorre la cola desde el principio en cada pasada. Antes de producción se requiere estrategia de tamaño, integridad y recuperación de SD, identificación de fallas, estabilidad prolongada, configuración horaria y protección de credenciales.

## Para migrar con la persona responsable del servidor

Confirmar red y acceso desde ESP32, endpoint de escritura, organización y bucket, token de alcance mínimo, retención, esquema de measurement/tags/fields, precisión temporal en segundos, datasource de Grafana, política de deduplicación, respaldo y recuperación. La prueba local no modifica ni valida por sí misma el servidor del laboratorio.

## Fuente del firmware

Archivo entregado por el usuario: `integracion_4sensores_http_reenvio(1).ino` (295 líneas), revisado el 19/10/2026. Incluye un endpoint privado local que deberá parametrizarse antes de publicarlo como ejemplo portable. No contiene el archivo `secrets.h`.
