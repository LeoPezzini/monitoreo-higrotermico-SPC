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
| Retención RTC sin alimentación | ⏳ Pendiente LIR2032 |
| microSD | ✅ HW-203 con microSD de 2 GB; escritura, relectura y CSV verificados |
| Cadena 4 sensores → 2 PCA → RTC → SD | ✅ Validada; CSV timestamp-eado generado |
| Wi-Fi ESP32 | ✅ 2.4 GHz validado |
| HTTP POST / JSON | ✅ Medición real NSHT30 enviada a servidor de prueba; HTTP 200 y JSON recibido correctamente |
| CRC-8 NSHT30 | ⏳ Bytes leídos, validación aún no implementada |
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

### Transmisión HTTP

Se validó:

```text
NSHT30 -> PCA -> ESP32 -> Wi-Fi -> HTTP POST -> servidor de prueba
```

Una medición real se serializó como JSON, se envió mediante POST y el servidor respondió HTTP 200 devolviendo correctamente el contenido recibido. El sketch de prueba no contiene credenciales reales.

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
3. Incorporar validación CRC-8 de temperatura y humedad del NSHT30.
4. Definir manejo explícito de sensor sin respuesta, CRC inválido y fallas de almacenamiento/comunicación.
5. Verificar retención del RTC con batería LIR2032.
6. Escalar la adquisición desde 4 hasta los 14 sensores previstos.
7. Realizar contraste/caracterización de los NSHT30 con el instrumento de referencia.
8. Caracterizar I2C con las longitudes y cableado reales de la instalación.
9. Implementar buffer de pendientes, confirmación, retransmisión y deduplicación según el backend definitivo.
10. Probar Wi-Fi y acceso al servidor desde la casilla de ensayo.
11. Realizar pruebas de fallas, ensayo prolongado y montaje definitivo.
