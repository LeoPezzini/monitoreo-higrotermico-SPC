# Arquitectura del sistema — 12 SHT30, receptor SPC v2

**Revision: 12/11/2026.** Este documento describe la arquitectura validada en banco, no la instalacion final. [Version historica de octubre](../historico/arquitectura-cuatro-sensores-2026-10.md).

## Cadena de datos actual

```text
Exterior: 5 SHT30 -> MUX 0x70 --+
                                  +-- I2C SDA21 / SCL22 -> ESP32 NodeMCU
Interior: 7 SHT30 -> MUX 0x71 --+                        |
                                                        +-- DS3231 I2C independiente (SDA17 / SCL16)
                                                        +-- microSD SPI (SCK14 MISO27 MOSI13 CS26)
                                                        |    CSV, cola persistente, diario ACK
                                                        +-- Wi-Fi -> POST :8081/api/v2/write
                                                               ID, CRC16, Influx LP
                                                               |
                                                        Receptor Python v2 Docker :8080
                                                               |-- valida CRC16
                                                               |-- responde 204/422/409
                                                               +-- SQLite persistente (spc-mediciones)
                                                                     |
                                                               Sincronizador + estado persistente
                                                                     |
                                                               InfluxDB 2.7, bucket higrotermia
                                                                     |
                                                               Grafana 12.3.0, dos paneles
```

## Hardware y direccionamiento

| Zona | MUX | Direccion A2 A1 A0 | Sensor / canal |
|---|---|---|---|
| Exterior | `0x70` | GND GND GND | S01/CH2 S02/CH3 S03/CH4 S04/CH5 S05/CH6 |
| Interior | `0x71` | GND GND 3,3 V | S07/CH0 S08/CH1 S09/CH2 S10/CH3 S11/CH4 S14/CH5 S15/CH6 |

La nomenclatura de placas y su ubicacion precisa se incorpora cuando se reciba el listado del equipo. La orientacion del conector aun debe verificarse con multimetro. Un cable de prueba de 2 m obtuvo 200/200 lecturas CRC8 OK a 100 kHz; aun no se ha demostrado estabilidad I2C de los cables definitivos instalados todos juntos.

## Integridad y persistencia

1. SHT30 devuelve 6 bytes por lectura con CRC8 para temperatura y humedad. Lectura invalida no se envia como dato correcto.
2. RTC DS3231 aporta timestamp por ciclo. Se ajusto una sola vez; `AJUSTAR_RTC_UNA_VEZ=false` en firmware actual.
3. microSD persiste cola de registros con ID y diario de ACK, para reenvio cuando el receptor no esta disponible.
4. Receptor v2 valida CRC16-CCITT-FALSE calculado sobre ID + LF + cuerpo exacto, deduplica por ID y almacena en SQLite. `HTTP 204` confirma persistencia en SQLite. `422` indica CRC incorrecto, `409` ID ya usado con otro payload.
5. Sincronizador lee SQLite, manda LP por lotes a InfluxDB y recuerda en volumen persistente los IDs confirmados. Cuando InfluxDB falla, mantiene pendientes y reintenta.
6. Grafana consulta InfluxDB con token read; sincronizador utiliza token write. URL y credenciales del ESP32 no incluyen token de InfluxDB.

## Validacion realizada

- Doce SHT30 con CRC8 correcto, RTC, SD y POST/204 en ciclos de banco.
- Caida receptor: 48 registros pendientes recuperados de secuencias 217 a 264, sin faltantes.
- CRC16/SQLite: 204 nuevo, 204 duplicado identico, 422 CRC incorrecto, 409 colision de ID; SQLite conserva solo dato valido.
- En una instantanea SQLite, secuencias 1–3546 unicas y sin huecos.
- InfluxDB y Grafana mostraron doce series de temperatura y doce de humedad.
- Caida y recuperacion InfluxDB: reintentos observados, luego 96 registros en 4 lotes HTTP 204 y continuidad.
- Conteo historico InfluxDB: S08 tenia 6 puntos menos que los otros once sensores; ventana reciente de 30 minutos 56 puntos por sensor. Investigar sin atribuir aun la causa.

## Despliegue y limites

- En notebook Windows: receptor `:8081`, InfluxDB `:8086`, Grafana Docker `:3001`. Son puertos del banco, no configuracion irrevocable del laboratorio.
- Respaldar volumen SQLite, volumen de estado del sincronizador, InfluxDB, Grafana y credenciales antes de trasladar datos. **No restaurar estado de enviados si el bucket destino esta vacio**, pues impediria reimportar registros.
- El receptor HTTP no usa TLS ni autenticacion por dispositivo; proteger por red/firewall.
- No se garantiza exactly-once extremo a extremo ni equivalencia registro a punto InfluxDB cuando measurement/tags/timestamp se repiten.
- Pendientes: soldadura y cableado final, UPS, prueba de 12 ramales simultaneos, instalacion con Fede, prueba prolongada, Wireshark, comparacion exhaustiva SQLite/InfluxDB, mantenimiento de SD y retencion.

## Fuentes de referencia

- [Pruebas integradas noviembre 2026](../pruebas/validacion-integrada-12-sht30-2026-11-12.md)
- [Firmware y asignacion de canales](../../firmware/integrado_12sht30_spc_v2/README.md)
- [Backend y guia de migracion](../../servidor/receptor-spc-v2/README.md)
- [Registro de decisiones](../decisiones/decision-log.md)
