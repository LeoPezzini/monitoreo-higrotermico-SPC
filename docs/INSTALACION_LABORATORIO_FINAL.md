# Instalación de laboratorio

Actualizado al 23/11/2026. El montaje físico quedó el 13/11 y la integración de producción el 18/11. Para el mantenimiento cotidiano usar la [guía de Fede](operacion/guia-fede.md). El [README](../README.md) contiene el mapeo vigente por ubicación y los archivos de SD.

## Fuentes de producción

- [Firmware FINAL](../firmware/SPC_JASON_PRODUCCION_10min_FINAL/README.md), confirmado instalado por Leonardo el 23/11.
- [Backend y configuración](../servidor/receptor-spc-v2/README.md). Receptor y sincronizador confirmados sin cambios en la migración.
- [Evidencias y seguimiento](pruebas/puesta-en-servicio-2026-11-18.md).

El paquete recibido del 18/11 incluía `docker-compose.spc-produccion.yml`, con `8080:8080`, `host.docker.internal:8086` y `host-gateway`. Se conserva [ese Compose](../servidor/receptor-spc-v2/docker-compose.spc-produccion.yml) como antecedente del paquete. Las capturas muestran que el sincronizador finalmente utilizó `http://influxdb:8086` sobre `iot_net`, con org `mi-org` y bucket `monitoreo-higrotermico`.

La [receta de laboratorio](../servidor/receptor-spc-v2/docker-compose.laboratorio.yml) reconstruye la configuración observada y conserva servicios compartidos fuera del proyecto. Antes de aplicarla hay que contrastar montajes y configuración con los contenedores existentes. Los archivos privados se mantienen en el servidor, fuera de Git.

## Reinstalación o actualización coordinada

1. Inventariar contenedores, red y montajes. Respaldar CSV, cola, ACK, eventos, recepción SQLite, estado del sincronizador, InfluxDB y Grafana.
2. Conservar las identidades de registro de producción. El formateo de SD/reinicio de cola puede reutilizar IDs y generar conflictos contra SQLite; definir continuidad o migración antes de hacerlo.
3. Completar las plantillas locales y validar el Compose específico del laboratorio. Verificar que apunta a los mismos volúmenes. Respetar los servicios compartidos del laboratorio.
4. Si se programa el ESP32, aplicar el procedimiento del jumper y registrar la interrupción. Usar el sketch cuya carpeta coincide con el nombre `.ino`. Mantener ajuste automático del RTC desactivado en operación.
5. Comprobar hora, SD, 12/12 lecturas, respuesta 204, publicación en InfluxDB y actualización por ubicación en Grafana. Registrar versión, fecha y alcance verificado.
6. Para otra versión de firmware, ensayar antes con el formato de cola/ACK y verificar reinicio y recuperación. Mantener la fuente instalada disponible como retorno.

La limpieza de datos de banco previa a la primera instalación fue una operación de esa etapa. La conservación de los datos ya adquiridos es parte del mantenimiento actual. Los cambios propuestos de ahorro energético requieren coordinar con el grupo el período de interrupción de mediciones.
