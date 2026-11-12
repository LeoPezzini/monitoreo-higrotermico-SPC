# Monitoreo higrotermico SPC — sistema de 12 sensores

**Estado actualizado: 12/11/2026.** Sistema para medir temperatura y humedad en placas experimentales de materiales aislantes. En banco se verifico la cadena de 12 sensores hasta Grafana. La instalacion final y las pruebas de campo siguen pendientes.

## Arquitectura actual

```text
12 SHT30 (5 exterior, 7 interior)
   -> dos MUX I2C (0x70 / 0x71)
   -> ESP32 NodeMCU + RTC DS3231 + microSD (CSV, cola, ACK)
   -> Wi-Fi / POST con CRC16 e ID unico
   -> receptor Python v2 en Docker (HTTP 204 / 422 / 409)
   -> SQLite persistente (deduplicacion)
   -> sincronizador persistente con reintentos
   -> InfluxDB 2.7 (bucket higrotermia)
   -> Grafana 12.3.0 (12 temperaturas y 12 humedades)
```

El **ACK HTTP 204 del receptor** certifica que SQLite acepto el registro; no implica escritura inmediata en InfluxDB. La microSD protege datos no recibidos; SQLite conserva datos recibidos hasta sincronizacion. El receptor anterior, que reenviaba directamente a InfluxDB, queda preservado como implementacion historica separada.

## Hardware y distribucion

| Componente | Estado al 12/11 |
|---|---|
| ESP32 NodeMCU / ESP-WROOM-32 | Firmware de 12 SHT30 funcionando en banco |
| 12 sensores SHT30 seleccionados | Funcionan con cableado actual; 20 modulos disponibles inicialmente |
| MUX exterior | `0x70`: A0=A1=A2 a GND; 5 sensores S01–S05 |
| MUX interior | `0x71`: A0 a 3,3 V; A1=A2 a GND; 7 sensores S07, S08, S09, S10, S11, S14, S15 |
| RTC DS3231 | SDA GPIO17, SCL GPIO16, bus I2C independiente; flag lostPower corregido |
| microSD SPI | SCK14, MISO27, MOSI13, CS26; cola y ACK persistentes |
| Gabinete y placas | Gabinete amurado, perforaciones realizadas y cables cortados; soldaduras en progreso el 12/11 |
| Cable de prueba de 2 metros | 200/200 lecturas correctas a 100 kHz; ensayo preliminar |
| UPS 5 V y llave ON/OFF | Integracion y verificacion electrica pendientes |

La cantidad de placas, sus nomenclaturas formales y las ubicaciones especificas se completaran cuando se disponga del plano/etiquetado del proyecto. **No asumir que el pin 1 del conector es VCC sin verificar continuidad y polaridad en la placa real.**

### Mapeo de sensores del firmware

| Sensor | Ubicacion por MUX | MUX | CH |
|---|---|---|---:|
| S01 | Exterior | `0x70` | 2 |
| S02 | Exterior | `0x70` | 3 |
| S03 | Exterior | `0x70` | 4 |
| S04 | Exterior | `0x70` | 5 |
| S05 | Exterior | `0x70` | 6 |
| S07 | Interior | `0x71` | 0 |
| S08 | Interior | `0x71` | 1 |
| S09 | Interior | `0x71` | 2 |
| S10 | Interior | `0x71` | 3 |
| S11 | Interior | `0x71` | 4 |
| S14 | Interior | `0x71` | 5 |
| S15 | Interior | `0x71` | 6 |

## Estado de validacion

| Bloque | Estado y evidencia |
|---|---|
| Adquisicion 12 sensores, CRC8 y RTC | **Validado en banco**; todos respondieron correctamente en ciclos integrados |
| microSD + cola y ACK | **Validado en banco**; 480/480 ACK reportados al cierre del ensayo del 11/11 |
| Caida del receptor | **Validada**; 48/48 registros pendientes (217–264) recuperados en SQLite |
| CRC16 y deduplicacion del receptor v2 | **Validada**; respuestas 204, 204, 422, 409 y consulta SQLite consistente |
| Secuencias de sensores en SQLite | **Validada en la instantanea consultada**; IDs numerados 1–3546 sin huecos |
| SQLite → InfluxDB | **Validada funcionalmente**; HTTP 204 y reintentos |
| InfluxDB caido y recuperado | **Validada funcionalmente**; 96 registros aceptados en lotes tras reinicio, siguio sincronizando |
| Grafana | **Validado**; doce series por variable, temperatura y humedad |
| Comparacion exhaustiva SQLite ↔ InfluxDB | Pendiente; hubo diferencia historica de seis puntos para S08, no observada en ventana reciente de 30 min (56 por sensor) |
| UPS / cableado final | Pendiente de montaje y ensayo |
| Wireshark | Pendiente |
| Despliegue definitivo, fin de semana | Pendiente |

Ver el detalle en [registro de validacion 11–12/11](docs/pruebas/validacion-integrada-12-sht30-2026-11-12.md).

## Proximos pasos reales

1. Terminar soldaduras, crimpeado y etiquetado de sensores/cables; comprobar polaridades y canales antes de energizar.
2. Ensayar doce sensores **con longitudes definitivas**, incluyendo todos los cables exteriores simultaneamente; documentar errores I2C/CRC8 por sensor.
3. Verificar esquema de UPS y llave ON/OFF con multimetro; ensayar cortes de energia y recuperacion.
4. Migrar backend Docker a la infraestructura definitiva con Fede, coordinando puertos, volumenes, permisos, tokens, IP estable y copias de seguridad, sin modificar servicios ajenos.
5. Validar adquisicion y dashboards desde ubicaciones definitivas; monitoreo continuo durante el fin de semana.
6. Capturar trafico real en Wireshark y completar comparacion de datos SQLite/InfluxDB en un intervalo fijo.
7. Completar etiquetado formal de placas, esquema electrico y registro fotografico; ensayar SD llena, corrupcion, retencion y politica de mantenimiento.

## Codigo y documentacion

- [Firmware 12 sensores + plantilla secrets](firmware/integrado_12sht30_spc_v2/README.md)
- [Backend v2, Docker Compose, sincronizador y pruebas](servidor/receptor-spc-v2/README.md)
- [Arquitectura vigente](docs/arquitectura/arquitectura-sistema.md)
- [Registro de decisiones](docs/decisiones/decision-log.md)
- [Caracterizacion SHT30](docs/pruebas/caracterizacion-sht30-2026-11.md)
- [Historial README del prototipo de 4 sensores](docs/historico/readme-etapa-4-sensores-hasta-2026-10.md)

**Seguridad:** credenciales en `secrets.h` o `.env.spc` locales, nunca versionadas; receptor HTTP sin TLS y sin autenticacion de cliente, por lo que el puerto debe restringirse a red autorizada. No afirmar exactly-once extremo a extremo.
