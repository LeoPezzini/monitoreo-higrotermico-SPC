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
NSHT30 x N -> TCA9548A x 2 -> ESP32 -> Wi-Fi -> servidor / BD -> Grafana
                                  |-> RTC
                                  |-> microSD (buffer local)
```

El protocolo de comunicación y el backend definitivos se definirán luego de relevar la infraestructura actualmente utilizada por el grupo de investigación.

## Hardware disponible

- ESP32 DevKit / ESP-WROOM-32
- 20 x NSHT30 disponibles (14 previstos actualmente para la instalación; 6 de reserva)
- TCA9548A de 8 canales: 1 verificado, 1 módulo no responde; 2 reemplazos/repuestos pedidos
- RTC HW-084 / DS3231
- módulo microSD HW-203
- placas preperforadas, fuente y elementos de montaje

## Estado actual

| Bloque | Estado |
| --- | --- |
| ESP32 | ✅ Verificado |
| Scanner I2C | ✅ Verificado |
| NSHT30 detectado en `0x44` | ✅ Verificado |
| Lectura directa de temperatura y humedad | ✅ Verificado |
| NSHT30 mediante TCA9548A | ✅ Verificado (canal 0) |
| RTC | 🧪 DS3231M detectado en 0x68 y lectura de fecha/hora verificada |
| microSD | ✅ HW-203: escritura/relectura y CSV integrado verificados con microSD de 2 GB |
| Adquisición multisensor | 🧪 2 sensores verificados en CH0/CH1; 20 NSHT30 comprobados individualmente |
| Cadena sensor → TCA → RTC → SD | ✅ CSV real generado y recuperado; timestamp pendiente de batería del RTC |
| Buffer y retransmisión | ⏳ Pendiente |
| Backend / Grafana | ⏳ Pendiente |
| Instalación final | ⏳ Pendiente |

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

1. Incorporar validación CRC-8 de temperatura y humedad del NSHT30.
2. Definir y probar el manejo explícito de sensores sin respuesta o con CRC inválido.
3. Realizar contraste/caracterización de los NSHT30 con el instrumento patrón.
4. Verificar retención del RTC cuando esté disponible la batería LIR2032.
5. Probar los TCA9548A nuevos al recibirlos y validar dos multiplexores con direcciones distintas.
6. Escalar la adquisición a los 14 sensores previstos para la instalación.
7. Caracterizar el bus I²C con las longitudes de cable previstas para la instalación.
8. Relevar e integrar la infraestructura de base de datos y Grafana existente.
9. Implementar buffer pendiente, confirmación, retransmisión y deduplicación.
10. Realizar pruebas de fallas, ensayo prolongado y montaje definitivo.
