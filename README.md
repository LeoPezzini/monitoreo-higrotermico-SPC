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

## Arquitectura prevista

```text
NSHT30 x 14 -> PCA9548A x 2 -> ESP32 -> Wi-Fi -> servidor / BD -> Grafana
                                  |-> RTC
                                  |-> microSD (buffer local)
```

La transmisión por Wi-Fi mediante HTTP POST y JSON ya fue validada en banco con una medición real. HTTP queda como alternativa preferente si la infraestructura existente del grupo dispone de una API compatible. El backend y mecanismo definitivos se decidirán después del relevamiento de la instalación existente.

## Hardware disponible

- ESP32 DevKit / ESP-WROOM-32
- 20 x NSHT30 disponibles (14 previstos actualmente para la instalación; 6 de reserva)
- 2 x PCA9548A nuevos verificados y funcionando simultáneamente en `0x70` y `0x71`
- 1 x TCA9548A previamente verificado, disponible como repuesto
- 1 x módulo TCA/PCA anterior no funcional
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
| Buffer / ACK / retransmisión | ⏳ Pendiente |
| Backend / Grafana real | ⏳ Pendiente de relevamiento |
| Instalación final | ⏳ Pendiente |

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

Un registro no se considerará entregado hasta contar con confirmación suficiente de su persistencia en el sistema remoto. El mecanismo definitivo de confirmación, reintento y deduplicación se definirá junto con el protocolo de comunicación y el backend.

## Próximos pasos

1. Relevar con el grupo de investigación la infraestructura existente: datasource de Grafana, base de datos, forma de ingreso de datos, red disponible y responsable técnico.
2. Confirmar si existe una API HTTP compatible; si existe, relevar endpoint, JSON esperado, autenticación y respuesta que confirma persistencia.
3. Coordinar con Fede una prueba HTTP POST a InfluxDB 2.x en red local: IP/hostname, puerto, organización, bucket de pruebas y token de solo escritura; usar `precision=s`.
4. Definir manejo explícito de sensor sin respuesta, CRC inválido y fallas de almacenamiento/comunicación.
5. Realizar prueba prolongada de retención del RTC con CR2032 y verificar precisión temporal.
6. Escalar la adquisición desde 4 hasta los 14 sensores previstos.
7. Realizar contraste/caracterización de los NSHT30 con el instrumento de referencia.
8. Caracterizar I2C con las longitudes y cableado reales de la instalación.
9. Implementar buffer de pendientes, confirmación, retransmisión y deduplicación según el backend definitivo.
10. Probar Wi-Fi y acceso al servidor desde la casilla de ensayo.
11. Realizar pruebas de fallas, ensayo prolongado y montaje definitivo.
