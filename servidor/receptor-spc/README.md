# Receptor SPC — Docker / InfluxDB 2.x

Copia documentada del receptor Python probado en notebook Windows (21/10/2026) y en Ubuntu del laboratorio (22/10/2026). El archivo `receptor_spc.py` corresponde a la versión funcional compartida durante las pruebas; el Dockerfile de este directorio es una receta reproducible equivalente a la construcción probada.

## Función
`ESP32 -> POST /api/v2/write :8080 -> receptor Python -> InfluxDB :8086`

Acepta Line Protocol de hasta 4096 bytes, añade el token solo al reenvío a InfluxDB y responde HTTP 204 si InfluxDB confirma 204. Ante error de reenvío responde 503. **No es un proxy genérico ni almacena datos por sí solo.** La microSD del ESP32 conserva pendientes y ACK.

## Preparación
```bash
cp .env.example .env
nano .env                  # introducir org, bucket y token reales sin publicarlos
docker build -t receptor-spc:prueba .
```

## Ejecución en servidor Ubuntu
La red Docker del laboratorio se identificó como `iot_net`; antes de repetir verificar `docker network ls` y `docker inspect influxdb`.

```bash
docker run -d --name receptor-spc --restart unless-stopped \
  --network iot_net -p 8080:8080 --env-file .env receptor-spc:prueba
docker ps
docker logs --tail 20 receptor-spc
docker stop receptor-spc
docker start receptor-spc
```

Para inspección interactiva sin detener el servicio: `docker logs -f receptor-spc`, salir con Ctrl+C. **No usar `docker run` otra vez si el contenedor ya existe**; usar `docker start`.

## Prueba de escritura desde Ubuntu
```bash
curl -i -X POST "http://localhost:8080/api/v2/write" \
  -H "Content-Type: text/plain" \
  --data-binary "higrotermico,sensor=TEST_UBUNTU temperature_c=25.5,humidity_rh=50.0 $(date +%s)"
```
Esperado: HTTP 204; verificar registro en InfluxDB. Este test crea un dato ficticio etiquetado TEST_UBUNTU.

## Seguridad y límites
- El archivo `.env` está ignorado por el repositorio. No subir tokens, contraseñas ni `secrets.h`.
- El endpoint del receptor **no autentica clientes**; publicar `-p 8080:8080` puede exponerlo en las interfaces del servidor. Restringir puerto por firewall/red autorizada antes de dejarlo permanentemente activo.
- El receptor es un prototipo sin TLS, validación semántica exhaustiva, cola propia ni deduplicación. HTTP 204 significa aceptación de escritura, no garantía general de entrega exactamente una vez.
- Confirmar URL estable del servidor y mecanismo de despliegue acordado con el responsable de Ubuntu.
- No editar ni reiniciar los servicios `influxdb`, `grafana`, `ingestor` y `telegraf` para desplegar este receptor.

Registro completo: [ensayos 21–22/10](../../docs/pruebas/2026-10-21_22_receptor_docker_recuperacion_y_migracion.md).
