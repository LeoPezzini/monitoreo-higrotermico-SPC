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
- 20 x NSHT30
- 2 x TCA9548A de 8 canales
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
| NSHT30 mediante TCA9548A | ⏳ Pendiente |
| RTC | ⏳ Pendiente |
| microSD | ⏳ Pendiente |
| Adquisición multisensor | ⏳ Pendiente |
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

1. Validar un NSHT30 a través del TCA9548A.
2. Probar varios sensores y ambos multiplexores.
3. Integrar RTC.
4. Integrar microSD.
5. Definir el formato de registro.
6. Implementar adquisición multisensor.
7. Relevar e integrar la infraestructura de Grafana existente.
8. Implementar almacenamiento pendiente, confirmación, retransmisión y deduplicación.
9. Realizar pruebas de fallas y un ensayo prolongado.
10. Realizar el montaje definitivo.
