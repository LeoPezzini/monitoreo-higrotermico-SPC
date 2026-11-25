# Instalación y seguimiento de producción

Actualización al 25/11/2026, basada en los chats exportados, el paquete final del 18/11 y las capturas entregadas. Leonardo confirmó el 23/11 que firmware y scripts no fueron modificados durante la migración.

## Secuencia

| Etapa | Resultado documentado |
| --- | --- |
| 13/11 | Sensores instalados, gabinete y alimentación autónoma. Trabajos en fibra impidieron completar la conexión al servidor |
| 13–16/11 | Adquisición local cada 30 s. Se recuperó la SD para análisis offline, considerando desde aproximadamente las 20:30 del viernes |
| 18/11 | Receptor v2 y sincronizador en Ubuntu; ciclo de doce ubicaciones con CRC8 y confirmaciones HTTP. Periodicidad regular de 10 min |
| 19–20/11 | Dashboard de producción con evolución temporal interior/exterior y ventanas de 24 y 48 h |
| 23/11 | Se informó el corte eléctrico y el apagado del servidor |
| 24/11 | Encendido manual del servidor, reenvío automático y seguimiento de las series recuperadas en Grafana |

## Evidencia de la migración

- [Log del sincronizador](../evidencias/produccion-2026-11/07_sincronizacion_influx_204.png). Destino `http://influxdb:8086`, organización `mi-org`, bucket `monitoreo-higrotermico`; muestra aceptación de una escritura de prueba. La fila TEST es un ensayo, no un punto del montaje.
- [Inspección de red](../evidencias/produccion-2026-11/08_red_docker_iot_net.png). InfluxDB y sincronizador en `iot_net`.
- [Ciclo real del nodo](../evidencias/produccion-2026-11/09_esp32_ciclo_real_12_sensores_crc_http.png). Fecha 18/11, doce ubicaciones, resumen 12/12, cero errores y respuestas 204 visibles. La imagen recorta parte del listado final; no afirmar desde ella la totalidad de las confirmaciones.
- [Panel de 48 horas](../evidencias/produccion-2026-11/18_Monitoreo_48h_Respuesta_Higrotermica.png). Series identificadas por ubicación, cuatro paneles de T/HR interior/exterior. La continuidad visual es evidencia de seguimiento, no una conciliación exhaustiva de cada fila.

## Archivos y versiones

El paquete `SPC_MANIANA_FINAL_2026-11-18.zip` contiene firmware FINAL, receptor, sincronizador, prueba Python y Compose inicial con acceso por host-gateway. La captura de la instalación demuestra que luego se configuró acceso por `iot_net` e `influxdb:8086`. La receta de laboratorio añadida al repositorio reconstruye esa configuración observada y requiere contrastar montajes/variables con los contenedores existentes antes de usarla. Las fuentes Python recibidas tienen la misma lógica que las ya publicadas en v2.

Grafana de producción es el servicio existente en el laboratorio. La captura `03_servicios_docker_produccion.png` entregada por Leonardo muestra imagen `grafana/grafana:11.0.0`; la versión 12.3.0 corresponde al banco con Compose de notebook.

## Corte eléctrico y recuperación del 24/11

El dispositivo de adquisición permaneció encendido y siguió registrando en la microSD. El 24/11, alrededor de las 11:00 (hora argentina), se encendió manualmente la computadora del laboratorio. Cuando volvieron a estar disponibles los servicios, el dispositivo reanudó automáticamente el envío. La secuencia de la OLED mostró 4.452, 4.117 y 3.359 registros pendientes. A las 16:40 del 24/11, Grafana mostró las series del fin de semana sin huecos visibles, con las marcas de tiempo de adquisición. El seguimiento visual queda pendiente de cotejo por ubicación entre microSD, SQLite e InfluxDB. La duración exacta del corte y la autonomía de la UPS siguen sin medirse.

## Antecedentes abiertos

En banco S08 tenía seis puntos históricos menos en InfluxDB y hubo una discontinuidad de secuencia alrededor de 4805 durante un incidente SD. Son incidentes para conciliar con sus registros originales. La autonomía y corriente del montaje completo siguen sin medirse. Las capturas de Wireshark entregadas sí documentan POST, encabezados ID/CRC16 y respuestas 204; el pendiente de captura de la revisión del 12/11 queda superado.
