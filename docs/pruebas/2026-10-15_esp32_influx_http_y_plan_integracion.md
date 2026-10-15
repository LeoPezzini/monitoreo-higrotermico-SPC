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
