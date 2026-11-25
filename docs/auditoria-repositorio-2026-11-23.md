# Revisión completa del repositorio del 23 de noviembre

## Alcance

Se revisó el árbol de 49 archivos versionados que terminaba en `142b2d6`, incluidos firmware de pruebas y banco, dos receptores, sincronizador, Compose, plantillas y documentación. Se contrastó con la fuente FINAL recibida en Library, el paquete del 18/11, los chats exportados, las capturas de banco y migración y la confirmación de Leonardo del 23/11 sobre las versiones instaladas.

## Resultado por área

| Área | Resultado y actualización |
| --- | --- |
| README y arquitectura | Todavía indicaban banco del 12/11 y montaje pendiente. Ahora distinguen instalación del 13/11, migración del 18/11, seguimiento y corte del 23/11 |
| Firmware | Se conserva banco y pruebas. Se agrega fuente FINAL exacta con ubicación, OLED, sincronización FAT, eventos y rotación |
| Backend | La comparación AST de receptor y sincronizador recibidos coincide con los ya versionados, excluyendo comentarios/docstring. Se conserva su código |
| Configuración | Se conserva Compose del paquete y se agrega receta de laboratorio diferenciada de notebook, con red y destino observados |
| Decisiones | Se conservan DEC-001 a 024 y agregan DEC-025 a 031 con estado y evidencia |
| Ensayos | Se conserva el historial con enlace al estado actual y se añade el registro de producción y cuatro capturas de migración |
| Operación | Se agrega guía para Fede, jumper, síntomas, arranque tras corte, archivos, respaldo y mantenimiento |
| Secretos | Se mantienen plantillas y se amplía exclusión de `.env.*`, cachés y bases locales, conservando `.example` |

## Diferencias resueltas

El firmware publicado en banco usaba IDs Sxx. La fuente de producción usa ubicación y canales finales. La BETA tenía otro tamaño de log; la FINAL rota a 10 MiB y conserva dos archivos anteriores. El README omitía eventos y seguimiento real. Grafana 12.3.0 y receptor 8081 corresponden al banco; la instalación muestra Grafana existente 11.0.0 y receptor 8080. La organización/bucket finales observados son `mi-org`/`monitoreo-higrotermico`.

El paquete del 18/11 contenía acceso por host-gateway. La captura de migración muestra que el sincronizador usó `influxdb:8086` en `iot_net`. La receta nueva reconstruye esos parámetros; su aplicación requiere cotejar volúmenes y ajustes reales. La revisión del repositorio no recrea contenedores ni cambia el equipo instalado.

## Comprobaciones

Se verifica identidad binaria del firmware con el original recibido, sintaxis Python, estructura YAML, enlaces locales Markdown y diferencia Git sin errores de espacios. El receptor se verifica en un proceso local temporal con un registro nuevo, repetido, conflicto, CRC inválido y reinicio persistente. El sincronizador se contrasta contra un destino HTTP local de prueba y su estado persistente. Resultado de receptor local: 204 nuevo, 204 idéntico, 409 conflicto y 422 CRC inválido; reinicio con una fila conservada. Sincronizador local: 503 mantuvo pendiente, 204 persistió el avance y el reinicio no volvió a publicar ese ID. Sintaxis Python y cinco archivos YAML válidos; enlaces locales reparados, incluido el README histórico trasladado.

El entorno de revisión carece de Arduino CLI/core/librerías de compilación y de un servidor Docker operativo. Quedan sin ejecutar la compilación del sketch y `docker compose config` en Docker. El análisis de YAML no sustituye esa validación. Los ensayos locales Python no son una nueva prueba física ni una conexión al laboratorio.

## Pendientes concretos

- Seguimiento del corte del 23/11: [recuperación observada el 24/11](pruebas/puesta-en-servicio-2026-11-18.md). Quedan pendientes el cotejo de registros y la medición de autonomía.
- Foto del jumper y confirmación de su aislamiento en la preperforada para el traspaso.
- Versiones exactas del core ESP32 y librerías del compilado instalado.
- Cotejo del archivo de despliegue vigente con la receta reconstruida, y montajes efectivos.
- Conciliación de S08 e incidente SD de secuencia 4805; mantenimiento de históricos.
- Ensayo de consumo, autonomía y ahorro energético antes de cargar otra versión.
