# Prueba integrada de cuatro sensores y HTTP a InfluxDB (2026-10-15)

**Estado:** sketch compilado y probado fisicamente; 20 ciclos completos correctos segun monitor serie. Pendiente contrastar los 80 registros con Data Explorer.

## Validaciones previas
- InfluxDB v2.7.0 local Windows 10: escritura manual y consulta Flux correctas.
- HTTP POST desde Python: 204.
- ESP32 192.168.1.93 a InfluxDB 192.168.1.40:8086: GET /health 200 y POST de TEST_ESP32 204.
- Data Explorer recupero TEST_ESP32: temperatura 25.5, humedad 55.2, 2026-10-15T12:10:02Z.
- Adquisicion local 4 NSHT30 + CRC + RTC + SD + LP validada previamente.

## Esta prueba
- Cuatro sensores S01-S04, 2 PCA9548A, RTC y microSD, sin cambiar conexiones.
- Un ciclo inmediato y 19 ciclos adicionales cada ~30 s: 20 ciclos y hasta 80 mediciones.
- CRC invalido o fallo I2C: descarta lectura. Timestamp RTC Argentina UTC-3 convertido a Unix UTC segundos.
- Guarda /mediciones_http.csv, /mediciones_http.lp y /envios_http.csv.
- Envia Line Protocol por HTTP POST a InfluxDB, precision=s; confirma aceptacion solo con HTTP 204.
- **Sin reenvio automatico, ACK persistente robusto ni deduplicacion.** El archivo .lp es historico completo, no cola de pendientes.
- Al reiniciar se vuelve a ejecutar la prueba y se agregan registros a los archivos. La cadencia es aproximada y no estricta.

## Preparacion local
Copiar secrets.example.h como secrets.h junto al .ino y completar SSID, password y token localmente. **Nunca subir secrets.h al repositorio**. Usar un token limitado a escritura en sensores-prueba. El servidor debe estar ejecutandose; ESP32 y PC deben poder comunicarse por LAN.

## Verificacion
En Data Explorer consultar measurement higrotermico y sensor S01 a S04, rango temporal de la prueba. Comparar registros recibidos y archivo CSV/LP/estados SD. Una respuesta 204 valida aceptacion HTTP, no constituye mecanismo de reenvio persistente.

## Cadencia
30 segundos para ensayo inicial; sugerencia de 5 minutos para monitoreo habitual, sujeto a definicion con catedra. Resolucion de panel Grafana no obliga a adquirir una sola vez por hora.


## Resultado de prueba fisica integrada (2026-10-15)

Prueba realizada por el usuario con ESP32 y cuatro NSHT30. Registro del monitor serie:
- 20/20 ciclos completos desde 2026-10-15T09:20:28 hasta 09:29:59 hora Argentina; intervalos observados de ~30 s.
- 4 sensores por ciclo: 80 lecturas validas y 80 escrituras CSV=OK y LP=OK informadas.
- 80 solicitudes HTTP con respuesta **204 ACEPTADO**; ningun error I2C, CRC, SD o HTTP reportado.
- IP del ESP32: 192.168.1.93; servidor InfluxDB local: 192.168.1.40:8086.
- Timestamp Unix UTC inicial 1792066828 y final 1792067399.
- **Pendiente:** consultar Data Explorer para confirmar las cuatro series y conteos almacenados. Aun no hay prueba de desconexion/reenvio, deduplicacion ni funcionamiento prolongado.
- Las mediciones de humedad variaron en el ensayo; no se interpreta aun como calibracion o fenomeno del material.

Estado del sketch: **probado fisicamente, 20 ciclos correctos segun monitor serie**. El archivo .lp sigue siendo historico, no una cola persistente de reenvio.

## Consulta de verificacion InfluxDB (captura 2026-10-15)

Consulta Flux agrupada por sensor y campo, con count(): Data Explorer muestra **8 tablas / 8 filas**. En la primera pagina son visibles S01 humidity_rh=20, S01 temperature_c=20 y S02 humidity_rh=20. El resto de los conteos no es visible en la captura (hay paginacion); verificar las paginas 2 y 3 antes de afirmar que todos son 20. Esto corrobora la presencia de series almacenadas y los conteos visibles, no la entrega garantizada frente a cortes.

## Verificacion final en Data Explorer — prueba cerrada (2026-10-15)

Las capturas de las tres paginas de resultados Flux confirman ocho conteos de 20: S01, S02, S03 y S04, cada uno para temperature_c y humidity_rh. Total: 160 valores de campo, equivalentes a 80 puntos de medicion de dos campos cada uno. Coinciden con 20 ciclos x 4 sensores y 80 respuestas HTTP 204 observadas en el monitor serie. **Prueba de extremo a extremo validada** en banco. Siguen pendientes ensayos de desconexion/reenvio, deduplicacion y estabilidad prolongada.


## Ensayo de caida y recuperacion del servidor — 2026-10-15

Se reinicio la prueba de 20 ciclos manteniendo ESP32, sensores, RTC y microSD encendidos. Se detuvo `influxd.exe` desde PowerShell y luego se volvio a iniciar, sin reiniciar el ESP32.

**Evidencia de monitor serie:**
- Ciclo 5/20, 2026-10-15T09:56:33 (Unix UTC 1792068993): S01, S02, S03 y S04 mostraron `CSV=OK | LP=OK` y `HTTP: -1 | NO CONFIRMADO`. El servidor estaba detenido.
- Ciclo 11/20, 2026-10-15T09:59:33 (Unix UTC 1792069173): los cuatro sensores mostraron `CSV=OK | LP=OK` y `HTTP: 204 | ACEPTADO` tras reiniciar el servidor.

**Inspeccion del archivo microSD `mediciones_http.csv` aportado por el usuario:**
- Encabezado: `timestamp_local,sensor,temperatura_C,humedad_RH,estado`.
- 160 filas de datos, correspondientes a dos ejecuciones de 20 ciclos con cuatro sensores cada una; cada timestamp de ciclo tiene S01, S02, S03 y S04.
- Se encuentran las cuatro filas del ciclo 5 a las 09:56:33: S01 (22.07 C, 50.54 %), S02 (22.10 C, 51.00 %), S03 (22.42 C, 51.44 %), S04 (22.51 C, 48.62 %).
- El CSV sigue conteniendo datos hasta 2026-10-15T10:04:03.
- El archivo CSV verifica persistencia local de las mediciones, **no** el estado de entrega remota. Los registros durante la caida no se reenvian con el firmware actual.

**Conclusion:** comprobado respaldo local durante indisponibilidad de InfluxDB y reanudacion de envios nuevos sin reiniciar el ESP32. Aun pendiente: implementar cola persistente de reenvio, confirmacion duradera, tratamiento idempotente y ensayo de recuperacion de pendientes. Mantener el sketch validado sin cambios; desarrollar el mecanismo en una nueva version.

**Proximo paso:** disenar estado de entrega persistente en SD y mecanismo de reenvio incremental. Probar nueva caida controlada y comparar conteos de InfluxDB con registros SD.
