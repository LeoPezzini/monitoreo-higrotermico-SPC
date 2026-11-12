# Backend SPC v2 — receptor SQLite + sincronizador InfluxDB + Grafana

Esta carpeta contiene el **receptor v2** usado en las pruebas de 12/11/2026, el sincronizador de SQLite a InfluxDB y dos Compose para monitoreo y sincronizacion. El receptor antiguo se mantiene aparte en `../receptor-spc/`: no son intercambiables.

## Flujo

ESP32 -> POST `/api/v2/write` :8081 -> receptor CRC16 -> SQLite volumen persistente -> sincronizador (estado persistente) -> InfluxDB 2.7 :8086 -> Grafana :3001.

**Semantica ACK:** `HTTP 204` del receptor significa persistencia SQLite, **no** confirmacion de InfluxDB. Un duplicado identico da 204; CRC incorrecto 422; mismo ID con cuerpo distinto 409. El sincronizador reintenta si InfluxDB cae.

## Puesta en marcha sobre entorno nuevo

**Antes:** instalar Docker Engine/Compose, comprobar puertos disponibles, redes y servicios preexistentes. No ejecutar `docker compose down -v` ni `docker volume prune` sobre datos utiles.

1. InfluxDB y Grafana:
   ```powershell
   docker compose -f docker-compose.monitoring.yml up -d
   ```
   Configurar InfluxDB en http://localhost:8086: org `SPC`, bucket `higrotermia`, retencion permanente. Crear token **write** solo para sincronizador y token **read** solo para Grafana. Mantenerlos fuera de Git.

2. Construir y arrancar receptor, usando volumen persistente. Esta opcion usa Docker Compose del backend:
   ```powershell
   docker compose -f docker-compose.receptor.yml up -d --build
   ```
   El receptor publica 8081 y crea/utiliza `spc-mediciones` (externo para el sincronizador). Configurar la URL del ESP32 con IP del host destino (no `localhost`).

3. Copiar `.env.spc.example` como `.env.spc` e introducir el **token de escritura**. Nunca subir `.env.spc`.
   ```powershell
   docker compose -f docker-compose.sincronizador.yml up -d
   docker logs --tail 30 spc-sincronizador
   ```
   El Compose de sincronizador utiliza `host.docker.internal:8086`, coherente con pruebas Docker Desktop sobre Windows. **En Ubuntu/Linux la resolucion puede ser diferente**; configurar ruta de red antes del despliegue definitivo. No asumir que el host Windows y Ubuntu tienen la misma red.

4. Abrir Grafana `http://localhost:3001` y crear fuente InfluxDB (Flux) con URL `http://spc-influxdb:8086` si ambos servicios corren en la misma red Compose; org `SPC`, bucket `higrotermia`, token **solo lectura**. Crear panel `temperature_c` y otro `humidity_rh`, filtrados por las etiquetas S01 S02 S03 S04 S05 S07 S08 S09 S10 S11 S14 S15.

5. Verificar:
   ```powershell
   docker ps
   docker logs --tail 20 receptor-spc-v2
   docker logs --tail 20 spc-sincronizador
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\probar_receptor_v2.ps1
   ```
   El script de prueba usa IDs fijos; si se ejecuta una segunda vez, su primera prueba pasa a ser un **duplicado**. El esperado sigue siendo 204.

## Migracion de datos

Los volumenes son parte del sistema, no archivos descartables. Respaldar/restaurar **`spc-mediciones`**, **`spc-sync-state`**, volumenes de InfluxDB y Grafana, o bien iniciar un entorno nuevo documentando que no se conserva el historial. Un estado de sincronizador restaurado sin el bucket InfluxDB correspondiente puede marcar registros como ya enviados aunque falten en el destino.

Verificar el nombre real de los volumenes generados por Compose con `docker volume ls` y `docker inspect`. En la notebook de prueba el volumen del receptor existente era `spc-mediciones`, montado en `/datos`.

## Limitaciones

Receptor no autentica clientes, HTTP sin TLS, puerto 8081 debe limitarse a red autorizada. No hay garantia general de exactly-once extremo a extremo; respaldar bases/estado y probar reinicios. No confundir cantidad de IDs SQLite con cantidad de puntos InfluxDB si coinciden sensor y timestamp.
