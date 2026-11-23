# Backend SPC v2

Actualizado al 23/11/2026. Receptor y sincronizador usados en la instalación del 18/11, confirmados por Leonardo sin cambios. Las fuentes Python recibidas coinciden en lógica con las publicadas. [Evidencias de producción](../../docs/pruebas/puesta-en-servicio-2026-11-18.md).

## Responsabilidades

| Servicio | Función |
| --- | --- |
| `receptor-spc-v2` | POST `/api/v2/write`, ID y CRC16 sobre ID + LF + cuerpo exacto. SQLite en `/datos/spc_v2.sqlite3`. Confirma tras transacción persistida |
| `spc-sincronizador` | Consulta recepción y estado, envía lotes a InfluxDB y persiste enviados en `/estado/sync.sqlite3` después de HTTP 204 |
| InfluxDB del laboratorio | Almacenamiento de series temporales |
| Grafana del laboratorio | Consulta y visualización por ubicación |

Un duplicado idéntico devuelve 204, un ID con otro cuerpo 409 y un CRC inválido 422. Solicitudes mal formadas pueden devolver 400, una ruta incorrecta 404 y un error SQLite 503. El receptor valida estructura de una línea UTF-8, sin validar toda la semántica de Influx LP; una línea inválida para Influx requiere diagnóstico del sincronizador. HTTP sin TLS/autenticación de cliente, para red autorizada.

## Laboratorio existente

Las capturas muestran `iot_net`, InfluxDB como `http://influxdb:8086`, organización `mi-org`, bucket `monitoreo-higrotermico`, receptor publicado 8080 y Grafana existente. [Guía de Fede](../../docs/operacion/guia-fede.md) incluye revisión, arranque y respaldo.

`docker-compose.laboratorio.yml` es una **receta reconstruida** con esos parámetros. Usa volúmenes externos `spc-mediciones` y `spc-sync-state` para evitar crear silenciosamente un histórico nuevo. Antes de usar, comparar con `docker inspect` la red, montajes, nombres de volumen y directorio que ya mantiene Fede. El acceso a InfluxDB proviene de `.env.laboratorio`, copiado de `.env.laboratorio.example` y completado privadamente.

Una vez que Fede haya contrastado configuración y respaldado los datos, la receta puede validarse y aplicarse desde esta carpeta:

```bash
docker compose -f docker-compose.laboratorio.yml config --quiet
docker compose -f docker-compose.laboratorio.yml up -d --build
```

Si el servicio existente solo está detenido, usar `docker start` sobre sus contenedores. El Compose es para reconstrucción/mantenimiento coordinado, no hace falta recrear una instalación saludable para obtener el código actualizado. La actualización del repositorio no ejecuta operaciones sobre el servidor.

## Banco Windows y antecedentes

Los archivos `docker-compose.monitoring.yml`, `docker-compose.receptor.yml` y `docker-compose.sincronizador.yml` conservan la configuración de notebook del 12/11, con receptor 8081, Grafana 3001 y acceso por `host.docker.internal`. Su `.env.spc.example` usa org `SPC` y bucket `higrotermia`. Esos nombres difieren de los observados en producción.

El [procedimiento de instalación](../../docs/INSTALACION_LABORATORIO_FINAL.md) enlaza el paquete original y distingue el acceso por host-gateway que contenía de la red finalmente observada.

## Pruebas y respaldo

`probar_receptor_v2.ps1` conserva el ensayo de banco para 204/204/422/409. `probar_receptor_v2.py` es la prueba de dos envíos iguales del paquete final del 18/11. Ambas insertan datos de prueba y deben ejecutarse en un entorno dedicado o coordinando su efecto sobre el histórico. No son consultas de salud sin escritura.

Copiar bases y restaurarlas con consistencia entre recepción, estado e InfluxDB. El estado de enviados no debe restaurarse contra un bucket vacío sin definir la reimportación. Detalles en la guía de Fede. Retención/compactación y conciliación exhaustiva siguen abiertos.

El script sincronizador usa `INSERT OR IGNORE` en su estado y consulta en orden de `rowid`. Si Influx confirma y el proceso cae antes de guardar el avance, puede repetir el lote. La misma serie y tiempo conserva la identidad del punto, pero el cotejo de registros se realiza sobre un intervalo definido y por ubicación.
