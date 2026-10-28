# Registro de decisiones

## DEC-001 — microSD como buffer local
**Estado:** implementada y validada en banco (19–22/10/2026).

Se conservarán localmente las mediciones para evitar pérdida de datos durante interrupciones de conectividad. Un registro permanecerá disponible hasta contar con confirmación suficiente de persistencia remota.

## DEC-002 — RTC para identificación temporal
**Estado:** implementada y validada en banco; retención prolongada pendiente.

Cada medición deberá incluir fecha y hora. El RTC permitirá mantener una referencia temporal aun cuando no exista conectividad. La lectura y generación de timestamps ya fueron validadas; queda pendiente verificar retención durante cortes con batería LIR2032.

## DEC-003 — integración con infraestructura existente
**Estado:** resuelta y validada en servidor Ubuntu (22/10/2026).

Se reutilizan InfluxDB 2.7 y Grafana del laboratorio. El ESP32 envía HTTP POST con Influx Line Protocol a un receptor Python en Docker (puerto 8080), conectado a la red interna `iot_net`; este reenvía a `influxdb:8086`. La integración ESP32 → Ubuntu → InfluxDB → Grafana fue verificada el 22/10/2026. La prueba HTTP/JSON inicial se conserva como antecedente, no como formato operativo actual.

## DEC-004 — alimentación de respaldo
**Estado:** arquitectura elegida; módulo UPS de 5 V adquirido, todavía sin validación eléctrica.

Se prevé alimentar el conjunto desde una salida respaldada de UPS de 5 V, distribuyendo 5 V al ESP32 y a un regulador de 3,3 V para los periféricos correspondientes. El cargador USB, UPS, regulador, autonomía y conmutación **no se consideran validados** hasta medirlos y ensayar cortes. No conectar fuentes en paralelo sin verificar el circuito.

## DEC-005 — dos PCA9548A para la adquisición final
**Estado:** aceptada para continuar el desarrollo.

Dos módulos PCA9548A nuevos fueron validados individualmente y en simultáneo. Se utilizarán inicialmente en direcciones `0x70` y `0x71`, con capacidad para hasta 16 canales. Se estiman unos 11 NSHT30 en la instalación, sujeto a definición del ensayo; hay 20 sensores disponibles. El TCA9548A previamente funcional queda disponible como repuesto.

## DEC-006 — timestamp común por ciclo de adquisición
**Estado:** aceptada para las pruebas actuales.

Se toma una referencia temporal al comienzo de cada ciclo y se asocia el mismo timestamp al conjunto de sensores de esa ronda. Esto representa las mediciones como pertenecientes a un mismo ciclo aunque las lecturas I2C se ejecuten secuencialmente.

## DEC-007 — verificación CRC-8 en lecturas NSHT30
**Estado:** implementada y validada en banco (13–14/10/2026).

Se comprueba CRC-8 de temperatura y humedad relativa en cada lectura. Una medición con CRC incorrecto o lectura I²C fallida se descarta y no se transmite como válida. La señalización y el registro sistemático de fallas por sensor siguen pendientes.

## DEC-008 — almacenamiento doble: CSV histórico y Line Protocol
**Estado:** implementada y validada en banco (14/10/2026).

La microSD conserva un CSV legible y un histórico en InfluxDB Line Protocol (LP). Los registros incluyen identificación del sensor, valores y marca temporal; el LP usa `higrotermico`, tag `sensor`, campos `temperature_c` y `humidity_rh`. El CSV y el histórico LP **no sustituyen** a la cola de reenvío.

## DEC-009 — cola persistente append-only y diario de ACK
**Estado:** implementada y validada en banco (19–22/10/2026).

El firmware registra cada línea LP en `/pendientes_v1.lp` antes de intentar transmitir. Tras HTTP 204 escribe en `/confirmados_v1.log` un ACK `N,~N`; al reiniciar recupera el último ACK válido y reenvía los registros pendientes en orden. Se observó recuperación después de fallas HTTP -1 y reinicio del ESP32. El firmware de banco limita a ocho reenvíos por pasada. **Pendiente:** compactación, manejo de corrupción/SD llena, duración indefinida y análisis de duplicados.

## DEC-010 — formato de transmisión Line Protocol sobre HTTP
**Estado:** validada en banco y en Ubuntu (15–22/10/2026).

Se adopta HTTP POST con cuerpo LP y precisión temporal en segundos. Los primeros ensayos con JSON/HTTP 200 fueron exploratorios; la escritura actual requiere confirmación HTTP 204. Se mantiene el timestamp original al retransmitir.

## DEC-011 — receptor Python desacoplado en Docker
**Estado:** validada funcionalmente (21–22/10/2026); despliegue permanente pendiente.

Un receptor Python escucha `POST /api/v2/write` en el puerto 8080 y reenvía a InfluxDB 2.7 en el 8086, dentro de la red Docker `iot_net`. Devuelve HTTP 204 al ESP32 únicamente cuando InfluxDB devuelve 204. No incorpora cola ni deduplicación propia. El receptor fue probado en notebook y servidor Ubuntu del laboratorio; al concluir la visita se detuvo el contenedor.

## DEC-012 — credenciales de InfluxDB en el servidor
**Estado:** arquitectura adoptada; limpieza del firmware final pendiente.

El receptor obtiene el token desde variables de entorno cargadas en el servidor (`.env`, fuera de Git). El firmware histórico publicado todavía usa `secrets.h` e incluye `INFLUX_TOKEN` para acceso directo: **no afirmar que el binario de prueba esté libre de token**. La versión final deberá compilarse y comprobarse sin esa credencial y con URL del receptor.

## DEC-013 — reutilización no invasiva de infraestructura existente
**Estado:** validada el 22/10/2026.

Se agrega el contenedor `receptor-spc` a `iot_net`, sin reinstalar ni modificar `influxdb`, `grafana`, `ingestor` ni `telegraf`. La IP de prueba no constituye una dirección permanente; acordar nombre o reserva DHCP, controles de acceso y operación con el responsable del servidor.

## DEC-014 — estrategia de entrega: al menos una vez, no exactamente una vez
**Estado:** criterio técnico documentado; ensayos exhaustivos pendientes.

Si la escritura remota se acepta pero el ESP32 pierde la respuesta o no logra guardar el ACK, puede reenviar el mismo punto. Los recuentos Flux de 26 registros por campo y sensor en 15 minutos comprueban consistencia en esa ventana, **no** ausencia general de duplicados. No se afirma semántica exactamente-una-vez.

## DEC-015 — instalación de sensores en cavidades de placas
**Estado:** criterio de montaje acordado; geometría y protección pendientes de validación.

Se prevé instalación permanente/no recuperable en cavidades perforadas de placas de aproximadamente 30 × 30 cm, con cámara de aire y cierre posterior con masilla. Se evalúan separadores mecánicos iguales para todos los módulos. El chip de humedad debe quedar expuesto al aire de la cavidad; proteger conexiones sin contaminarlo ni obstruirlo. La ubicación entre placa y pared y el número final de sensores requieren confirmación con la responsable del ensayo.

## DEC-016 — cableado corto, blindado y gabinete centralizado
**Estado:** materiales seleccionados/adquiridos; prueba eléctrica pendiente.

Se planifica un cable de cuatro conductores blindado por sensor, longitudes interiores próximas a 0,4 m y exteriores idealmente de hasta 1 m, con gabinete estanco cercano. La idoneidad I²C debe verificarse con el cable real, incluidos pull-ups, capacitancia, errores de lectura y CRC. El blindaje no garantiza por sí mismo el funcionamiento.

## DEC-017 — caracterización de sensores y repuestos
**Estado:** planificada, no ejecutada.

Hay 20 módulos NSHT30. Se estiman unos 11 instalados y unos 9 de reserva, sujetos a confirmación. Se propone identificar físicamente cada módulo, contrastarlo frente a referencia y documentar desviaciones y criterios de aceptación. No denominar «calibración» formal a una caracterización comparativa sin trazabilidad metrológica.

## DEC-018 — priorizar confiabilidad y mantenibilidad frente a alarmas
**Estado:** criterio de alcance adoptado (octubre de 2026).

No se incorporarán ahora notificaciones Telegram/correo ni nuevos servicios para alarmas. Se priorizan adquisición, respaldo local, recuperación, montaje, caracterización, documentación de conexiones, código comentado y ensayos prolongados. Las alertas quedan como posible ampliación futura.

## DEC-019 — validación de UPS antes de integrar alimentación
**Estado:** pendiente de prueba; compra realizada.

Se planifica una UPS de salida nominal 5 V con portabaterías y un regulador de 3,3 V. La fuente USB INOVA CAR-3007 declara 5 V y 5,1 A máximos, valor aún no medido. Confirmar especificaciones reales de UPS, tipo/polaridad de celdas, corriente, compatibilidad del regulador, conmutación sin reinicio y autonomía antes de conectar el sistema definitivo.

## Referencias de evidencia
- [CRC-8 de cuatro sensores](../pruebas/2026-10-13_crc8_4sensores.md)
- [Integración RTC, microSD y CRC](../pruebas/2026-10-14_integracion_crc_rtc_sd_cr2032.md)
- [Line Protocol local](../pruebas/2026-10-14_integracion_local_influx_lp.md)
- [Cola, ACK y Grafana](../pruebas/2026-10-19_cola_ack_reenvio_grafana.md)
- [Receptor Docker, recuperación y Ubuntu](../pruebas/2026-10-21_22_receptor_docker_recuperacion_y_migracion.md)
