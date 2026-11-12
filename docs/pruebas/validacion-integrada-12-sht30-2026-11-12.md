# Validacion integrada SPC — 11 y 12 de noviembre de 2026

## Alcance y estado

**Validado en banco con cableado provisional:** 12 sensores SHT30, ESP32 NodeMCU, dos multiplexores I2C, RTC DS3231, microSD, receptor v2 con CRC16 y SQLite, sincronizador, InfluxDB 2.7 y Grafana 12.3.0.

**No validado aun:** instalacion fisica definitiva, los cuatro cables exteriores con sus longitudes finales en conjunto, UPS/llave ON-OFF, ensayo prolongado de fin de semana, captura Wireshark, equivalencia exhaustiva registro por registro SQLite/InfluxDB.

## Sensores y buses

| Sensor | MUX | Canal |
|---|---|---:|
| S01 | 0x70 | 2 |
| S02 | 0x70 | 3 |
| S03 | 0x70 | 4 |
| S04 | 0x70 | 5 |
| S05 | 0x70 | 6 |
| S07 | 0x71 | 0 |
| S08 | 0x71 | 1 |
| S09 | 0x71 | 2 |
| S10 | 0x71 | 3 |
| S11 | 0x71 | 4 |
| S14 | 0x71 | 5 |
| S15 | 0x71 | 6 |

Bus sensores: SDA GPIO21, SCL GPIO22, 100 kHz. RTC en bus independiente SDA GPIO17 y SCL GPIO16 **segun cableado real**. MicroSD SPI: SCK14, MISO27, MOSI13, CS26. CRC8 de temperatura y humedad validado por sensor. Firmware con ajuste inicial de RTC desactivado en uso normal (`AJUSTAR_RTC_UNA_VEZ=false`); el flag lostPower fue corregido tras ajuste unico. La polaridad y el orden mecanico de los conectores finales deben comprobarse antes de energizar; no asumir pinout por recuerdo.

## Adquisicion, RTC, cola y receptor

- Se verificaron 12/12 sensores con CRC8 correcto, sin errores en los ciclos mostrados del firmware integrado.
- El 11/11 se documento un cierre con **480/480** registros confirmados en el ESP32.
- Ensayo de indisponibilidad del receptor: **48 registros** pendientes (secuencias 217–264) se reenviaron tras reiniciar Docker. Consulta SQLite posterior: **48/48**, sin faltantes en ese rango.
- El receptor v2 usa `POST /api/v2/write`, encabezados `X-SPC-ID` y `X-SPC-CRC16`. CRC16-CCITT-FALSE (polinomio 0x1021, init 0xFFFF) sobre bytes UTF-8 del ID, salto de linea y cuerpo exacto. SQLite guarda `measurement_id` como clave primaria.
- Ensayo formal 12/11 en paralelo con adquisicion: 204 para registro nuevo; 204 para duplicado identico; 422 para CRC alterado; 409 para mismo ID con distinto contenido y CRC correcto. Comprobacion SQLite: un solo registro para `SPC01-TESTCRC-20261112-0001`, cero para `SPC01-TESTCRC-20261112-0002`, valor original 25.36 grados C.
- Consulta secuencial en SQLite realizada el 12/11: IDs con sufijo numerico 1..3546, **3546 secuencias unicas y cero faltantes** en esa instantanea. No es prueba de recepcion de cada adquisicion fisica si no se conoce el total generado.

## Cable de 2 metros

Un cable de 2 m entre MUX y SHT30 dio **200 intentos, 200 correctos, cero errores CRC/I2C** a 100 kHz. Ensayo preliminar, no garantiza la operacion simultanea de todos los cables definitivos.

## InfluxDB, sincronizacion y Grafana

Entorno de prueba en notebook Windows mediante Docker:
- `receptor-spc-v2`: Windows puerto 8081 -> contenedor 8080; volumen externo persistente `spc-mediciones` en `/datos`.
- `spc-sincronizador`: consulta SQLite en modo lectura, envia lotes LP a InfluxDB y registra IDs confirmados en volumen persistente de estado separado.
- `spc-influxdb`: InfluxDB 2.7, puerto 8086, organizacion `SPC`, bucket `higrotermia`, retencion ilimitada.
- `spc-grafana`: Grafana 12.3.0, puerto 3001; token **solo lectura** para el bucket, separado del token de escritura del sincronizador.

Se observaron lotes de hasta 100 registros con `INFLUX 204`; Data Explorer y un dashboard Grafana mostraron 12 series de temperatura y 12 de humedad.

Prueba de caida de InfluxDB: `docker stop spc-influxdb` produjo `PENDIENTE / REINTENTO: URLError...` en el sincronizador, mientras receptor/ESP32 continuaban. Tras `docker start spc-influxdb` se observaron escrituras aceptadas en lotes **84 + 2 + 8 + 2 = 96 registros** y nuevos `INFLUX 204` en ciclos siguientes. Grafana recupero curvas en intervalo de 15 minutos. **Esto verifica recuperacion funcional; no prueba por si solo igualdad total de SQLite e InfluxDB.**

Conteo InfluxDB de temperatura sobre el historico: 297 puntos en 11 sensores y 291 en S08 (diferencia de 6 puntos). En los **ultimos 30 minutos** consultados despues, cada uno de los doce sensores mostro **56 puntos**. La diferencia historica permanece sin causa confirmada; no confundir puntos InfluxDB (identificados por measurement/tags/time) con IDs unicos SQLite.

## Evidencias capturadas (archivos locales, no incorporados aqui)

- `25_Pruebas_CRC16_Duplicados_HTTP_204_422_409.png`
- `26_SQLite_Verificacion_Persistencia_Deduplicacion_CRC16.png`
- `27_Caida_InfluxDB_Sincronizador_Pendiente_Reintentos.png`
- `28_Recuperacion_InfluxDB_Reenvio_96_Registros_HTTP204.png`
- `29_Grafana_Continuidad_Mediciones_Tras_Caida_InfluxDB.png`
- `30_SQLite_3546_Secuencias_Consecutivas_Cero_Faltantes.png`
- `32_InfluxDB_12_Sensores_56_Mediciones_30_Minutos.png`

Las capturas contienen evidencia visual en la conversacion de pruebas; aun no se adjuntaron al repositorio. No subir tokens, contrasenas ni IP privadas.

## Pendientes prioritarios para 13/11

1. Verificar conectores, polaridades, continuidad y soldaduras con multimetro antes de energizar. Probar los 12 cables finales y la UPS.
2. Migrar el despliegue Docker a la maquina definitiva con Fede, **sin borrar ni sobrescribir contenedores de terceros**; revisar direcciones de red, puertos, persistencia y politicas de acceso.
3. Definir y respaldar los datos, estado del sincronizador y tokens locales antes de migrar; usar permisos minimos.
4. Comparar puntos de InfluxDB y registros SQLite sobre un intervalo fijo; investigar diferencia historica S08.
5. Capturar tramas HTTP/CRC16 con Wireshark y documentar trafico.
6. Documentar pruebas de reinicio de todos los contenedores y mantenimiento de energia; verificar funcionamiento continuo durante el fin de semana.

## Seguridad y limites

Transmision HTTP sin TLS ni autenticacion del ESP32 en el receptor, adecuada solo en red controlada. Restringir puerto 8081 y proteger tokens. El `204` del receptor certifica persistencia local SQLite, **no** escritura inmediata en InfluxDB. El sincronizador confirma cada lote solo tras `204` de InfluxDB y persiste sus IDs enviados; ante caida entre escritura y marca local puede repetir LP. InfluxDB puede consolidar puntos con igual serie y timestamp. La politica de compactacion/retencion de SQLite, microSD y estado de sincronizacion queda pendiente.
