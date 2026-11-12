# Arquitectura del sistema de monitoreo higrotérmico

**Revisión documental:** 28/10/2026. **Estado:** cadena de cuatro sensores validada en banco y contra servidor Ubuntu del laboratorio; montaje definitivo y escalado pendientes.

## 1. Objetivo y alcance

Adquirir temperatura y humedad relativa en placas experimentales de materiales aislantes, conservar los registros ante interrupciones de conectividad y visualizarlos mediante la infraestructura existente de InfluxDB y Grafana. Se dispone de 20 NSHT30; la instalación se estima en **aproximadamente 11**, cantidad y posiciones todavía sujetas a confirmación. Las pruebas integradas documentadas se realizaron con **cuatro** sensores, no con la instalación completa.

## 2. Diagrama funcional comprobado

```text
 S01,S02 NSHT30 --- PCA9548A 0x70 --+
                                    +-- I2C GPIO21/22 -- ESP32 DevKit
 S03,S04 NSHT30 --- PCA9548A 0x71 --+                       |
                                                          +-- I2C GPIO16/17 -- RTC DS3231M
                                                          +-- SPI -- microSD HW-203
                                                          |          |-- CSV histórico
                                                          |          |-- LP histórico
                                                          |          |-- pendientes_v1.lp
                                                          |          +-- confirmados_v1.log
                                                          |
                                                          +-- Wi-Fi / HTTP POST /api/v2/write :8080
                                                                 |
                                                        Receptor Python en Docker
                                                        (servidor Ubuntu, red iot_net)
                                                                 |
                                                          HTTP + Line Protocol
                                                                 |
                                                          InfluxDB 2.7 :8086
                                                                 |
                                                               Grafana
```

El RTC y la microSD son periféricos del ESP32, no elementos interpuestos entre sensores y red. El puerto 8080 corresponde al receptor; 8086 a la API HTTP de InfluxDB. El receptor está conectado a la red Docker `iot_net` y accede al servicio mediante el nombre `influxdb:8086`.

## 3. Adquisición, buses y temporalidad

- Dos multiplexores PCA9548A comprobados en `0x70` y `0x71`; cuatro sensores de banco `S01`–`S04` en canales 0 y 1 de cada uno.
- NSHT30 en dirección `0x44`, con verificación CRC-8 tanto de temperatura como de humedad. Si falla I²C/CRC, se omite la medición inválida; falta definir un registro explícito y persistente del fallo por sensor.
- El firmware de prueba configura I²C de sensores a 100 kHz en GPIO21/22; RTC en un segundo bus I²C GPIO16/17. MicroSD: SPI con CS26, MOSI13, SCK14 y MISO27. **Son conexiones del sketch de banco, no pinout definitivo del gabinete.**
- El RTC DS3231M mantiene fecha/hora con CR2032 no recargable tras deshabilitar el circuito de carga del módulo; se comprobó un corte breve. La retención prolongada y deriva siguen sin caracterizar.
- Se asigna un timestamp común por ciclo aunque los sensores se lean secuencialmente. El sketch de banco considera el RTC en hora local UTC−3 y convierte a Unix UTC sumando tres horas; revisar esta convención al cerrar el firmware.
- El ciclo de banco es aproximadamente cada 30 segundos y termina nuevas adquisiciones tras 20 ciclos; luego continúa reintentando pendientes. **No es todavía firmware de operación continua.**

## 4. Formato y persistencia

La medición válida se serializa en CSV y en InfluxDB Line Protocol. Ejemplo ilustrativo de LP (no medición experimental):

```text
higrotermico,sensor=S01 temperature_c=25.50,humidity_rh=55.20 1790000000
```

Se usa measurement `higrotermico`, tag `sensor`, fields `temperature_c` y `humidity_rh`, y precisión de timestamp en segundos.

El firmware de banco emplea cuatro archivos diferenciados:
- `/mediciones_http.csv`: histórico legible.
- `/mediciones_http.lp`: histórico en Line Protocol; **no es la cola**.
- `/pendientes_v1.lp`: cola persistente append-only; cada línea representa un registro a confirmar.
- `/confirmados_v1.log`: diario de ACK con pares `N,~N`.

La escritura local precede a la transmisión. Si la SD falla, el firmware de banco evita transmitir el registro sin respaldo completo.

## 5. Confirmación, fallas y reenvío

```text
medir -> CRC correcto -> timestamp -> CSV/LP/cola en microSD
                                           |
                                           v
                                   POST al receptor :8080
                                           |
                           +---------------+----------------+
                           |                                |
                     InfluxDB acepta 204             error, timeout, HTTP -1
                           |                                |
                  receptor devuelve 204                no avanza ACK
                           |                                |
                 ESP32 persiste ACK                    queda pendiente
                           |                                |
                      avanza cursor                  reintenta más tarde
```

Al reiniciar, el ESP32 recupera el último ACK válido desde la SD. La prueba del 21/10 observó ACK 588, registro 589 pendiente durante la caída y posterior aceptación de registros #589–#596 y siguientes. El mecanismo mantiene timestamps originales en el reenvío. El receptor **no guarda una segunda cola**.

**Semántica:** se busca reenvío hasta confirmación, no se ha demostrado entrega exactamente-una-vez. Si InfluxDB acepta una escritura pero el ACK no se persiste, el mismo punto puede reenviarse. Faltan pruebas de deduplicación, SD llena, corrupción de archivos, cortes durante escritura y compactación de cola/diario.

## 6. Receptor Docker e infraestructura del laboratorio

El receptor es un programa Python con `ThreadingHTTPServer` expuesto en `/api/v2/write` (puerto 8080). Recibe un cuerpo LP y lo reenvía a la API de InfluxDB 2.7 por puerto 8086. Obtiene el token desde variables de entorno del servidor y devuelve HTTP 204 **solo cuando InfluxDB respondió 204**; ante error de reenvío responde 503. No realiza autenticación del ESP32, almacenamiento local ni validación completa de valores.

El 22/10 se verificó comunicación ESP32 → receptor en Ubuntu → InfluxDB → Grafana, sin modificar los contenedores preexistentes `influxdb`, `grafana`, `ingestor` y `telegraf`. Al finalizar se detuvo `receptor-spc`. Para operación permanente faltan direccionamiento estable, restricción de acceso al puerto, ensayos de reinicio y acuerdo de mantenimiento con el responsable del servidor.

**Credenciales:** el receptor lee `.env` fuera de Git. El sketch de reenvío histórico publicado todavía contiene lógica de token para conexión directa a InfluxDB. No se ha archivado una copia verificada del firmware exacto cargado para la prueba con Ubuntu; la compilación final deberá eliminar el token de InfluxDB del ESP32.

## 7. Resultados observados y límites

- 19/10: cola y ACK persistentes, recuperación de registros #89–#96 tras error HTTP -1; cuatro curvas de temperatura y cuatro de humedad en Grafana.
- 21/10: receptor Docker en notebook, corte provocado, reinicio del ESP32 y reenvío posterior.
- Consulta Flux en ventana de 15 minutos: 26 registros de `temperature_c` y 26 de `humidity_rh` **por sensor S01–S04**. La igualdad de conteos no prueba ausencia global de duplicados.
- 22/10: POST desde ESP32 al Ubuntu del laboratorio con HTTP 204 y datos visibles en Grafana.

Ver [ensayos del 19/10](../pruebas/2026-10-19_cola_ack_reenvio_grafana.md) y [ensayos 21–22/10](../pruebas/2026-10-21_22_receptor_docker_recuperacion_y_migracion.md).

## 8. Diseño físico y alimentación (previstos; aún no validados)

- Gabinete estanco próximo a placas; cable de cuatro conductores blindado individual por sensor, recorridos interiores estimados ~0,4 m y exteriores idealmente hasta ~1 m. Validar I²C con el cable real; la pantalla del cable no elimina la capacitancia ni reemplaza una prueba.
- Sensores en cavidades de placas experimentales ~30 × 30 cm, con cámara de aire, separadores repetibles y cierre con masilla; instalación no recuperable. La protección de PCB/conexiones y la ubicación exacta deben acordarse y probarse antes de empotrar.
- Alimentación prevista: cargador USB 5 V → entrada UPS 5 V con baterías → salida respaldada 5 V → ESP32 y regulador 3,3 V → periféricos según tensión admitida. Aún faltan comprobar polaridades, corriente, estabilidad, conmutación, autonomía y compatibilidad eléctrica. No alimentar ESP32 simultáneamente por USB y pin 5 V sin revisar la placa.
- Identificar y caracterizar los 20 sensores, dejando los no instalados como repuestos documentados. Cantidad instalada estimada ~11, sin cierre definitivo.
- Alertas Telegram/correo quedan como posible extensión, no como requisito de esta etapa.

## 9. Próximas validaciones prioritarias

1. Confirmar cantidad, ubicación y propósito de cada sensor con responsable del ensayo.
2. Ensayar un sensor con longitud real de cable y documentar errores I²C/CRC; luego ampliar de 4 al número final.
3. Probar UPS, consumo, cortes y autonomía; diseñar esquema definitivo en KiCad.
4. Caracterizar los 20 NSHT30 frente a referencia; registrar identificación y resultados.
5. Verificar montaje y protección en una cavidad de prueba y en ambiente exterior.
6. Revisar/archivar firmware final sin secretos, con adquisición continua, reporte de fallas y gestión de cola/ACK.
7. Ensayar operación prolongada, cortes de alimentación, SD llena/dañada y reinicio del servidor.
8. Capturar fotos, logs, consultas Flux y pantallas Grafana con fecha y condiciones de ensayo; completar manual de mantenimiento.

## 10. Trazabilidad

- [Registro de decisiones](../decisiones/decision-log.md)
- [Pruebas del receptor y migración a Ubuntu](../pruebas/2026-10-21_22_receptor_docker_recuperacion_y_migracion.md)
- [Manual del receptor](../../servidor/receptor-spc/README.md)
- [Firmware histórico de reenvío (banco)](../../firmware/pruebas/integracion_4sensores_http_reenvio/integracion_4sensores_http_reenvio.ino)
