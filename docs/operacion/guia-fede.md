# Guía de operación y traspaso a Fede

Equipo SPC de doce ubicaciones, versión documental del 23/11/2026. La configuración normal toma un ciclo cada 10 min, lo respalda en microSD y entrega pendientes al receptor. El servidor conserva en SQLite y publica en InfluxDB mediante el sincronizador. El panel de Grafana permite el seguimiento.

## Comprobación cotidiana

1. En Grafana revisar que la última muestra de cada ubicación sea reciente, considerando el intervalo de 10 min y los tiempos de consulta.
2. En la OLED revisar ambientes y estado. `Pend` es cantidad de registros, no cantidad de ciclos; un ciclo completo agrega doce.
3. Si el panel deja de actualizarse, anotar la última hora visible y comprobar servidor/red antes de intervenir el nodo.

En el servidor, desde la cuenta autorizada de Fede:

```bash
docker ps -a
docker logs --tail 30 receptor-spc-v2
docker logs --tail 30 spc-sincronizador
```

Un 204 del receptor confirma SQLite. `INFLUX 204` del sincronizador confirma publicación. Receptor activo con InfluxDB caído puede seguir aceptando mediciones; Grafana se atrasará hasta recuperar publicación.

## Jumper y conexión USB

En este montaje, el jumper de alimentación abre la rama que lleva los 5 V de la placa/UPS al ESP32. El USB de la notebook también alimenta al ESP32. La operación acordada es quitar ese puente para usar USB y colocarlo para operación autónoma. La masa de referencia y las líneas de datos se mantienen según el circuito real.

Identificar y rotular físicamente el jumper con Leonardo antes del traspaso; falta incorporar una foto con su posición exacta. La imagen ilustrativa de la presentación no sustituye esa identificación. Abrir la rama del ESP32 no elimina la energía de la UPS ni del resto de la placa. Las baterías pueden mantener alimentación aunque se quite el cargador.

### Entrar en mantenimiento por USB

1. Coordinar la interrupción y registrar su inicio. Apagar la salida de alimentación del montaje mediante su llave y desconectar el USB si estaba conectado.
2. Quitar el jumper identificado con las fuentes hacia el ESP32 desactivadas. Comprobar su posición antes de insertar USB.
3. Conectar la notebook por USB. Para una prueba integrada, habilitar alimentación de los periféricos únicamente con el aislamiento de 5 V del ESP32 comprobado. El USB alimenta al ESP32 y la fuente de la placa a sus periféricos.
4. Abrir serie a 115200 y guardar el diagnóstico. Evitar cargar un nuevo sketch cuando alcanza con observar el estado.

### Volver a operación autónoma

1. Finalizar la observación o programación y desconectar USB.
2. Desactivar la alimentación del montaje antes de reponer el jumper.
3. Colocar el jumper y activar la alimentación de la placa.
4. Comprobar arranque, hora correcta, SD válida, 12/12 sensores, recuperación de pendientes y un nuevo punto en Grafana. Anotar la hora de retorno.

La alimentación USB y externa del ESP32 deben mantenerse separadas durante el servicio. Como referencia general, Espressif documenta opciones de alimentación excluyentes para sus DevKitC; este montaje requiere verificar su placa y su puente reales. [Guía de alimentación de Espressif](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html)

## Qué hay en la microSD

| Archivo | Uso |
| --- | --- |
| `spc_final.csv` | Histórico de muestras válidas con hora local y ubicación |
| `spc_final_cola.txt` | ID y línea de transmisión de cada registro |
| `spc_final_ack.txt` | Avance confirmado; guardar junto a la cola |
| `spc_eventos.csv` | Eventos recientes con fecha y hora |
| `spc_eventos_1.csv`, `spc_eventos_2.csv` | Historial rotado, hasta dos archivos de 10 MiB aproximadamente |

Para copiar la tarjeta, apagar completamente el nodo y comprobar que no siga alimentado por USB/UPS antes de extraerla. Copiar todos los archivos sin editarlos y preservar la tarjeta original hasta verificar el respaldo. Reinsertar antes de encender.

CSV, cola y ACK no se depuran automáticamente. Evitar borrar/formatear una tarjeta de producción para resolver un error. Reiniciar la cola puede reutilizar IDs y producir 409 contra la base existente. Una cola truncada o un ACK inconsistente requiere analizar una copia antes de reparar.

## Qué hacer según el síntoma

| Síntoma | Acción inicial |
| --- | --- |
| `ERROR WIFI` | Comprobar red y alimentación del acceso Wi-Fi; observar si SD sigue guardando |
| `ERROR RECEPTOR HTTP` con Wi-Fi activo | Comprobar servidor, contenedor, puerto y logs. El nodo conserva pendientes |
| Receptor acepta pero Grafana se atrasa | Revisar sincronizador, URL/bucket/token y disponibilidad de InfluxDB |
| HTTP 422 | Verificar contenido/CRC del registro. Conservar la fila pendiente y sus logs |
| HTTP 409 | Investigar reutilización de ID o cambio de contenido; no borrar SQLite para forzar aceptación |
| `ERROR RTC` | Registrar hora observada y `lostPower`; corregir con procedimiento controlado y referencia horaria |
| `ERROR MICROSD` o adquisición bloqueada | Guardar diagnóstico, apagar y respaldar tarjeta. Revisar integridad/espacio/conexión antes de reiniciar |
| Menos de 12 sensores | Identificar ubicación y canal en serie/eventos. Una lectura inválida puede faltar en el CSV de esa ronda |

El log registra cambios de estado y arranques cuando SD y RTC están disponibles. La ausencia de un evento no demuestra ausencia de falla si esos recursos estaban indisponibles.

## Recuperación después de un corte

El servidor del laboratorio requiere encendido manual según lo informado el 23/11. Anotar corte, reposición de energía y encendido del servidor. Revisar que Docker vuelva a ejecutar los servicios existentes. `restart: unless-stopped` actúa con Docker disponible; el retorno eléctrico de la PC tiene su propio procedimiento.

Si los contenedores SPC existen y están detenidos, revisar sus logs y arrancarlos por sus nombres:

```bash
docker start receptor-spc-v2 spc-sincronizador
```

Los servicios InfluxDB y Grafana compartidos se recuperan con el procedimiento de Fede. Mantener sus datos/volúmenes. Verificar `GUARDADO`/`DUPLICADO CONFIRMADO`, luego `INFLUX 204` y consultar puntos con horas del intervalo de caída.

El nodo puede seguir midiendo con la UPS y acumular pendientes mientras haya energía. Si se agota, deja de adquirir hasta volver a encender. Reconstruir el resultado con CSV, eventos, cola y ACK, comparando el mismo intervalo en el servidor. Dejar pendiente la causa si no se conoce la hora del corte o el estado inicial de las baterías.

## Respaldos del servidor

Inventariar los montajes efectivos antes de copiar o restaurar:

```bash
docker inspect receptor-spc-v2 --format '{{json .Mounts}}'
docker inspect spc-sincronizador --format '{{json .Mounts}}'
```

Guardar juntos SQLite de recepción, SQLite de estado del sincronizador, datos/configuración de InfluxDB, dashboard/fuente de Grafana y configuración local protegida. Para una instantánea SQLite de recepción con la API de backup, en una carpeta nueva de respaldo:

```bash
docker stop spc-sincronizador
docker exec receptor-spc-v2 python -c 'import sqlite3; s=sqlite3.connect("/datos/spc_v2.sqlite3"); d=sqlite3.connect("/tmp/spc-respaldo.sqlite3"); s.backup(d); d.close(); s.close()'
docker cp receptor-spc-v2:/tmp/spc-respaldo.sqlite3 ./spc-recepcion.sqlite3
docker cp spc-sincronizador:/estado/sync.sqlite3 ./spc-sincronizacion.sqlite3
docker start spc-sincronizador
```

Los comandos suponen las rutas del código publicado; cotejar los montajes antes. Si falla una copia, conservar el error y restablecer el sincronizador tras el diagnóstico. La copia del estado se realiza con ese proceso detenido; la copia de recepción se toma después mediante la API SQLite para incluir lo aceptado hasta ese instante. Coordinar además el respaldo de InfluxDB y Grafana con su procedimiento existente. Evitar copiar en caliente solo el archivo SQLite por fuera de su API.

Al restaurar, el estado de enviados debe corresponder al contenido del destino. Un bucket vacío con un estado que marca todo como publicado puede dejar datos sin reimportar. Restaurar primero en un entorno separado y verificar antes de reemplazar producción.

## Próximo firmware de ahorro

El código instalado no implementa suspensión. Tras fallo espera nominalmente 10 s; al vaciar con éxito, 100 ms entre registros. Se propone ensayar intentos de conexión más espaciados durante caídas, por ejemplo 210 s, y reposo entre tareas, manteniendo el muestreo cada 10 min.

Antes de intervenir, acordar con el grupo un período de pausa de adquisición y registrar inicio/fin. Ensayar en banco continuidad del RTC, cola/ACK tras despertar, unicidad de IDs, recuperación de conexión, consumo y UPS. El ahorro total depende también de OLED, reguladores y periféricos. Conservar fuente y configuración de la versión instalada como retorno y mantener los mismos formatos de datos durante el cambio o definir su migración.

## Datos para completar al entregar

Registrar con Fede la dirección estable del servidor, ubicación de los archivos del despliegue y respaldos, foto/etiqueta del jumper, versiones del core/librerías Arduino, circuito de corte de alimentación y procedimiento de encendido de la PC. Las credenciales se entregan por el canal privado acordado; Git conserva solo plantillas.
