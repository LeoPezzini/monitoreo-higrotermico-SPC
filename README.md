# Monitoreo higrotérmico — Sistemas para Control

Sistema de adquisición, almacenamiento y transmisión de temperatura y humedad para el seguimiento de eco-materiales aislantes en una casilla de ensayo.

## Objetivo

Desarrollar un sistema autónomo y confiable capaz de:

- adquirir temperatura y humedad de múltiples sensores NSHT30;
- identificar cada medición por sensor y ubicación;
- asociar fecha y hora mediante RTC;
- conservar localmente las mediciones en microSD;
- transmitir los registros mediante Wi-Fi;
- integrarse con la infraestructura de base de datos y Grafana del grupo de investigación;
- conservar registros pendientes ante pérdidas de conectividad y retransmitirlos al recuperar la comunicación;
- detectar sensores sin respuesta y otras fallas previsibles.

## Arquitectura integrada (cuatro sensores validados)

```text
NSHT30 (4 en banco; ~11 previstos, a confirmar) -> PCA9548A x 2 -> ESP32
                                                               |-> RTC DS3231M
                                                               |-> microSD (CSV, LP, cola y ACK)
                                                               +-> Wi-Fi / HTTP POST :8080
                                                                   -> receptor Python en Docker
                                                                   -> InfluxDB 2.7 :8086 -> Grafana
```

[Arquitectura técnica, conexiones y límites de validación](docs/arquitectura/arquitectura-sistema.md).

La arquitectura HTTP quedó validada con un receptor Python en Docker que reenvía Line Protocol a InfluxDB 2.7. El 22/10/2026 se verificó el envío directo desde el ESP32 al servidor Ubuntu del laboratorio, con visualización en Grafana. El despliegue permanente y el escalado siguen pendientes.

## Hardware disponible

- ESP32 DevKit / ESP-WROOM-32
- 20 x NSHT30 disponibles (estimación actual: ~11 para instalación y ~9 de reserva; cantidad y posiciones por confirmar)
- 2 x PCA9548A nuevos verificados y funcionando simultáneamente en `0x70` y `0x71`
- RTC HW-084 / DS3231M
- módulo microSD HW-203
- placas preperforadas, fuente y elementos de montaje

## Estado actual

| Bloque | Estado |
| --- | --- |
| ESP32 | ✅ Verificado |
| NSHT30 `0x44` | ✅ 20 sensores comprobados individualmente |
| Multiplexación | ✅ 2 PCA9548A simultáneos: `0x70` y `0x71` |
| Adquisición multisensor | ✅ 4 NSHT30 simultáneos distribuidos en 2 PCA |
| RTC | ✅ DS3231M detectado en `0x68`; lectura y timestamp verificados |
| Retención RTC sin alimentación | ✅ CR2032 no recargable, circuito de carga deshabilitado; corte breve validado 2026-10-14 |
| microSD | ✅ HW-203 con microSD de 2 GB; escritura, relectura y CSV verificados |
| Cadena 4 sensores → 2 PCA → RTC → SD | ✅ Validada; CSV timestamp-eado generado |
| Wi-Fi ESP32 | ✅ 2.4 GHz validado |
| HTTP POST / JSON | ✅ Medición real NSHT30 enviada a servidor de prueba; HTTP 200 y JSON recibido correctamente |
| CRC-8 NSHT30 | ✅ Validación de temperatura y humedad integrada con 4 sensores, RTC y microSD |
| Line Protocol InfluxDB y timestamp Unix UTC | ✅ 4 sensores, generación y relectura desde microSD validadas el 2026-10-14 |
| Buffer / ACK / retransmisión | ✅ Validado en banco: cola SD, ACK persistente, reintento HTTP 204 y recuperación tras reinicio (19/10/2026) |
| InfluxDB / Grafana local | ✅ Cuatro sensores y ocho series verificadas en el banco |\n| Receptor Docker (notebook) | ✅ POST/204, caída del receptor y recuperación de cola tras reinicio del ESP32 (21/10/2026) |\n| Receptor Docker (Ubuntu laboratorio) | ✅ Imagen construida, red `iot_net`, POST/204 desde ESP32 y visualización en Grafana (22/10/2026) |

## Pruebas validadas destacadas

### Dos PCA y cuatro sensores

Configuración de banco validada:

```text
ESP32 I2C GPIO21/22
 |
 +-- PCA 0x70
 |    +-- CH0 -> S01
 |    +-- CH1 -> S02
 |
 +-- PCA 0x71
      +-- CH0 -> S03
      +-- CH1 -> S04
```

Los dos PCA nuevos fueron probados primero individualmente y luego simultáneamente. El segundo utiliza A0 en nivel alto para obtener la dirección `0x71`.

### Integración RTC + microSD

El 2026-10-05 se validó un ciclo completo de cuatro sensores con timestamp común y almacenamiento CSV. Para esta prueba, al no estar instalada todavía la batería del RTC, el sketch ajusta temporalmente el DS3231 a `__DATE__` / `__TIME__` al arrancar. Esa línea no debe permanecer en el firmware definitivo.

Ejemplo validado:

```csv
timestamp,sensor,temperatura_C,humedad_RH,estado
2026-10-05T19:21:05,S01,26.27,28.32,OK
2026-10-05T19:21:05,S02,26.32,35.81,OK
2026-10-05T19:21:05,S03,26.39,40.40,OK
2026-10-05T19:21:05,S04,26.29,35.81,OK
```

### Integración CRC + RTC respaldado + microSD — 2026-10-14

Se validó en banco la integración de los cuatro NSHT30 con CRC-8 de temperatura y humedad, RTC y escritura reportada en microSD. Se modificó el circuito de carga del módulo RTC para usar una CR2032 no recargable y se comprobó que el reloj continuó avanzando durante un corte breve, con el ajuste automático desactivado. El archivo CSV no se releyó en esta prueba. La hora del RTC está en hora local de Argentina y requerirá conversión a UTC antes de generar timestamps Unix para InfluxDB.

- [Sketch integrado con CRC, RTC y SD](firmware/pruebas/integracion_4sensores_crc_rtc_sd/integracion_4sensores_crc_rtc_sd.ino)
- [Registro de prueba y modificación del RTC](docs/pruebas/2026-10-14_integracion_crc_rtc_sd_cr2032.md)

### Integración local completa con Line Protocol — 2026-10-14

Se validaron 41 ciclos y 164 registros por archivo con cuatro NSHT30, CRC-8, RTC y microSD. El firmware genera CSV y Line Protocol con timestamp Unix UTC en segundos, reabre los archivos y muestra su contenido por serie. No hay transmisión ni gestión de ACK implementadas todavía.

- [Sketch de integración local](firmware/pruebas/integracion_local_4sensores_influx_lp/integracion_local_4sensores_influx_lp.ino)
- [Registro de validación](docs/pruebas/2026-10-14_integracion_local_influx_lp.md)

### Transmisión HTTP

Se validó:

```text
NSHT30 -> PCA -> ESP32 -> Wi-Fi -> HTTP POST -> servidor de prueba
```

Una medición real se serializó como JSON, se envió mediante POST y el servidor respondió HTTP 200 devolviendo correctamente el contenido recibido. El sketch de prueba no contiene credenciales reales.

### Verificación CRC-8 — 2026-10-13

Se probó un sketch independiente con los cuatro NSHT30 conectados a dos PCA9548A. En un ciclo completo los cuatro sensores devolvieron CRC-8 correcto tanto para temperatura como para humedad relativa. S02 presentó inicialmente error I2C código 4 y volvió a responder después de revisar conexiones; posible contacto intermitente no confirmado. No se modificó el firmware anterior de integración.

- [Sketch de prueba CRC-8](firmware/pruebas/nsht30_crc8_4sensores/nsht30_crc8_4sensores.ino)
- [Resultados y observaciones](docs/pruebas/2026-10-13_crc8_4sensores.md)

### InfluxDB local y HTTP desde ESP32 — 2026-10-15

En Windows 10 se inicio InfluxDB v2.7.0 sin Docker. Se verificaron escritura manual, consulta Flux y timestamp Unix en segundos. Python obtuvo HTTP 204 y el ESP32 realizo GET /health (200) y POST de una medicion TEST_ESP32 (204). Data Explorer recupero temperatura 25.5, humedad 55.2 y timestamp 2026-10-15T12:10:02Z.

Se preparo una **nueva prueba integrada, aun no ejecutada**, de cuatro NSHT30 + CRC + RTC + microSD + HTTP POST. Realiza 20 ciclos aproximados cada 30 segundos (hasta 80 registros), con CSV y LP historicos locales, y log de respuestas HTTP. No implementa aun reenvio ni deduplicacion. Credenciales exclusivamente en secrets.h local, nunca en GitHub.

- [Sketch integrado para probar](firmware/pruebas/integracion_4sensores_http_influx/integracion_4sensores_http_influx.ino)
- [Plantilla de credenciales sin secretos](firmware/pruebas/integracion_4sensores_http_influx/secrets.example.h)
- [Registro y protocolo de prueba](docs/pruebas/2026-10-15_esp32_influx_http_y_plan_integracion.md)

## Estrategia de confiabilidad

```text
medir -> validar -> timestamp -> guardar localmente -> transmitir
                                      |
                              sin confirmación
                                      |
                              mantener pendiente
                                      |
                                 retransmitir
```

Un registro no se considerará entregado hasta contar con confirmación suficiente de su persistencia en el sistema remoto. La cola y el ACK persistentes están implementados y validados en banco; no hay garantía demostrada de entrega exactamente una vez. Quedan pendientes deduplicación, compactación, ensayos de corrupción y operación prolongada. El firmware histórico publicado corresponde a conexión directa con InfluxDB y no debe confundirse con el binario probado contra el receptor Docker.

### Cola persistente y visualización local — 2026-10-19

Se verificó un firmware de banco con cuatro NSHT30, CRC-8, RTC, CSV y Line Protocol en microSD, cola append-only, diario de ACK persistente y reenvío a InfluxDB 2.7 local. El equipo obtuvo HTTP 204 en escrituras aceptadas, conservó el registro #89 pendiente durante fallos HTTP -1 y posteriormente lo retransmitió junto con otros pendientes (#89 a #96). Tras reiniciar, recuperó el cursor de confirmación de la microSD. Grafana mostró las cuatro series de temperatura y humedad relativa.

**Alcance:** esta prueba no demuestra deduplicación exactamente-una-vez en todas las condiciones: una caída entre el HTTP 204 y la escritura local del ACK puede provocar reenvío. La cola y el diario crecen sin compactación; se requiere una política de retención y recuperación ante archivos dañados. El endpoint y credenciales corresponden al banco local, no al servidor del laboratorio. La versión exacta compilada para la prueba con receptor Docker debe archivarse por separado sin secretos.

- [Registro de prueba del 19/10 y observaciones](docs/pruebas/2026-10-19_cola_ack_reenvio_grafana.md)

## Receptor Docker y validación en el laboratorio — 21 y 22/10/2026

Se implementó un receptor HTTP Python en Docker (puerto 8080) que recibe Line Protocol del ESP32, añade el token desde el entorno del servidor y reenvía a InfluxDB 2.7. Se validaron la escritura HTTP 204, la cola microSD y el ACK persistente ante caída del receptor y reinicio del ESP32. El 22/10 se instaló el receptor en el servidor Ubuntu del laboratorio, conectado a la red Docker `iot_net`, y se observaron solicitudes reales del ESP32 con HTTP 204 y datos en Grafana.

Una consulta sobre un intervalo de 15 minutos devolvió 26 registros de temperatura por cada sensor S01–S04 y 26 de humedad por cada uno. **Esto no demuestra ausencia total de duplicados** ni entrega exactamente una vez. El receptor de prueba tampoco autentica al cliente ni realiza almacenamiento local.

- [Registro de ensayos y limitaciones](docs/pruebas/2026-10-21_22_receptor_docker_recuperacion_y_migracion.md)
- [Código y procedimiento del receptor Docker](servidor/receptor-spc/README.md)

## Próximos pasos

1. Confirmar número y posición final de sensores (estimación actual: ~11) con la responsable del ensayo.
2. Ensayar cable blindado real, longitud, I²C, CRC y protección de módulos; escalar de 4 sensores al número confirmado.
3. Probar UPS de 5 V, regulador 3,3 V, alimentación, autonomía y cortes; documentar conexiones en KiCad.
4. Identificar y caracterizar los 20 NSHT30 frente a referencia, con repuestos trazables.
5. Archivar y comentar el firmware exacto para receptor Docker, sin credenciales de InfluxDB en el ESP32; definir funcionamiento continuo y diagnóstico de fallas.
6. Verificar SD llena/dañada, compactación de cola/ACK, duplicados, reinicios y funcionamiento prolongado.
7. Coordinar con Fede despliegue estable, control de acceso al puerto 8080 y dirección del servidor; no modificar otros contenedores.
8. Documentar con fotos, capturas, logs y pruebas reproducibles el montaje y la validación final.

## Documentación principal

- [Arquitectura técnica actualizada](docs/arquitectura/arquitectura-sistema.md)
- [Auditoría de repositorio y brechas pendientes (28/10)](docs/auditoria-repositorio-2026-10-28.md)
- [Registro de decisiones DEC-001 a DEC-019](docs/decisiones/decision-log.md)
- [Validación Docker, recuperación y Ubuntu (21–22/10)](docs/pruebas/2026-10-21_22_receptor_docker_recuperacion_y_migracion.md)
- [Operación del receptor Python](servidor/receptor-spc/README.md)
