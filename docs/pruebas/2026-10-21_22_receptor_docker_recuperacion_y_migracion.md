# Validación del receptor Docker, recuperación y despliegue en laboratorio — 21 y 22/10/2026

## Objetivo y alcance
Integrar la adquisición de cuatro NSHT30 desde ESP32 con microSD y un receptor HTTP intermedio, que reenvía Line Protocol a InfluxDB 2.7 y permite visualizar las series en Grafana. Las pruebas de 21/10 se realizaron con receptor Docker en una notebook Windows; las del 22/10, en el servidor Ubuntu del laboratorio. Esta documentación refleja observaciones compartidas y confirmaciones de los participantes, no un ensayo certificado de larga duración.

## Arquitectura comprobada
```text
S01–S04 NSHT30 -> PCA9548A (0x70,0x71) -> ESP32
                                         |-> RTC DS3231M
                                         |-> microSD (CSV, LP, cola y ACK)
                                         +-> Wi-Fi / HTTP POST :8080
                                             -> receptor Python en Docker
                                             -> InfluxDB 2.7 :8086
                                             -> Grafana
```
El receptor escucha `/api/v2/write`, reenvía el cuerpo Line Protocol con un token de escritura obtenido de variables de entorno y devuelve 204 **solo si InfluxDB respondió 204**. En fallas del reenvío devuelve 503. No almacena por sí mismo una cola ni implementa deduplicación. La cola y el ACK persistente residen en la microSD del ESP32.

## 21/10: pruebas con Docker en notebook
1. Instalación de WSL 2, Ubuntu y Docker Desktop; `docker run --rm hello-world` satisfactorio.
2. Construcción de la imagen `receptor-spc:prueba` con `python:3.12-slim`; receptor en puerto 8080.
3. Escritura de prueba desde PowerShell: HTTP 204 y datos de prueba visibles en InfluxDB.
4. ESP32 conectado por Wi-Fi al receptor de la notebook: cuatro sensores, registros CSV/LP/cola microSD, reenvíos aceptados HTTP 204 y curvas de temperatura/humedad en Grafana.
5. Caída provocada del receptor: el firmware informó `HTTP -1 | PENDIENTE` y mantuvo el cursor en el primer registro no confirmado.
6. Reinicio del ESP32 durante la interrupción: se recuperó el último ACK persistente; al restaurar el receptor se retransmitieron registros pendientes con HTTP 204. En la prueba observada, el ACK previo era 588, el registro #589 permaneció pendiente y después se aceptaron #589–#596 y los siguientes, hasta #692.
7. Se observaron en Grafana puntos con las marcas temporales originales de los ciclos realizados durante la interrupción, no solo con la hora de reenvío.

### Consulta de integridad en InfluxDB
Se verificó que los campos reales son `temperature_c` y `humidity_rh`, measurement `higrotermico` y tags `sensor=S01` a `S04`. Una consulta Flux de conteo sobre un intervalo de 15 minutos devolvió **26 registros de temperatura por sensor** (S01–S04), y otra consulta equivalente devolvió 26 de humedad por sensor. Esto comprueba simetría de recuentos para esa ventana, **no** demuestra por sí solo inexistencia de duplicados, pérdidas fuera del intervalo o entrega exactamente una vez. La revisión inicial devolvía `No Results` por filtros/ventanas no coincidentes; se corrigió verificando primero los nombres de campos y los datos crudos.

## 22/10: migración del receptor a Ubuntu del laboratorio
- En Ubuntu, imagen Docker construida exitosamente con `docker build -t receptor-spc:prueba .`.
- Se constató que seguían activos los servicios preexistentes: `influxdb` (InfluxDB 2.7), `grafana`, `ingestor` y `telegraf`. No se modificaron esos servicios.
- `curl -i http://localhost:8086/health` respondió HTTP 200, `status: pass`, InfluxDB v2.7.12.
- `docker inspect influxdb` identificó la red Docker `iot_net` y el nombre resoluble `influxdb`. Se configuró el receptor para reenviar a `http://influxdb:8086/api/v2/write` con los parámetros reales de org, bucket y precision=s, sin publicar el token.
- El receptor se ejecutó en `iot_net` y se probó desde Ubuntu con un POST ficticio `sensor=TEST_UBUNTU`: HTTP 204.
- Se cambió la URL de destino del ESP32 al puerto 8080 del Ubuntu del laboratorio. El log del receptor registró solicitudes desde el ESP32 `192.168.1.107` con HTTP 204, y `InfluxDB: HTTP 204`. Se verificaron los datos en Grafana. **La transmisión directa ESP32 → Ubuntu → InfluxDB quedó validada en la red local.**
- Se explicó ejecución desacoplada (`docker run -d`) con política `--restart unless-stopped`, lectura de logs con `docker logs`, parada con `docker stop receptor-spc` y reanudación con `docker start receptor-spc`. El receptor se detuvo al terminar la visita. No consta ensayo de reinicio completo del servidor Ubuntu.

## Configuración operativa de referencia
```bash
# Ejecutar desde ~/spc-receptor; nunca subir el archivo .env con token.
docker build -t receptor-spc:prueba .
docker run -d --name receptor-spc --restart unless-stopped \
  --network iot_net -p 8080:8080 --env-file .env receptor-spc:prueba
docker ps
docker logs --tail 20 receptor-spc
# Al retirarse:
docker stop receptor-spc
# En la próxima visita:
docker start receptor-spc
```
Si se ejecutó antes con `--rm`, detener esa instancia y recrearla sin `--rm` para usar `docker start` en visitas posteriores. Verificar que el nombre `receptor-spc` no esté ya ocupado antes de crear otra instancia.

## Evidencias para anexar al informe
Pendientes de capturar en una próxima visita, sin repetir necesariamente todos los ensayos: `docker ps`, `docker logs` mostrando POST 204 del ESP32, salud de InfluxDB, panel Grafana con cuatro series de temperatura y cuatro de humedad, fotografía del banco y del montaje final, registro de una caída y reenvío. No se dispone de capturas de la instalación Ubuntu de esta visita.

## Limitaciones y próximos ensayos
- El ensayo actual utiliza cuatro sensores; objetivo físico: aproximadamente 14, con distribución final por confirmar.
- No se ha demostrado deduplicación extremo a extremo ante todos los cortes posibles (por ejemplo, entre 204 y persistencia de ACK). Mantener identidad de series y timestamps y definir política de idempotencia.
- El receptor actual no autentica al cliente ESP32: el puerto 8080 debe quedar restringido a la red/cliente autorizado y se debe acordar seguridad con el administrador.
- Verificar persistencia y crecimiento de archivos SD, integridad ante corte de alimentación, recuperación de cola dañada, sincronización horaria y ensayo continuo.
- Confirmar direccionamiento estable (reserva DHCP o nombre de host) para el servidor antes del despliegue definitivo; no depender de direcciones IP observadas en la prueba.
- Probar longitudes reales de I2C, disposición física, alimentación y gabinete; contrastar sensores con instrumento de referencia.
- Revisar el firmware final para que **no transporte ni exponga el token de InfluxDB** si la autenticación queda a cargo del receptor.

## Estado al cierre
La prueba funcional de cuatro sensores, persistencia SD, retransmisión y envío al servidor Ubuntu del laboratorio fue satisfactoria. Montaje físico, escalado, seguridad, durabilidad y validación metrológica siguen pendientes.
