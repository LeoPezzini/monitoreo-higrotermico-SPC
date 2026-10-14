# Prueba de integración local — 2026-10-14

## Alcance

ESP32 DevKit; cuatro NSHT30 en dos PCA9548A (0x70: CH0 S01, CH1 S02; 0x71: CH0 S03, CH1 S04); RTC DS3231M por I2C independiente (SDA16/SCL17), CR2032 no recargable con circuito de carga deshabilitado; módulo microSD HW-203 SPI (CS26, MOSI13, SCK14, MISO27).

Sketch: [integracion_local_4sensores_influx_lp.ino](../../firmware/pruebas/integracion_local_4sensores_influx_lp/integracion_local_4sensores_influx_lp.ino).

## Procedimiento y observaciones

- Lecturas cada ~30 s, CRC-8 verificado en temperatura y humedad; mediciones inválidas se descartan.
- RTC guarda hora de Argentina (UTC-3). Timestamp Unix UTC = `rtc.now().unixtime() + 10800`; InfluxDB debe recibir `precision=s`.
- Cada ronda de cuatro sensores comparte el timestamp del inicio del ciclo.
- `/mediciones_v2.csv`: fecha local, sensor, temperatura, humedad, estado.
- `/influx_pendiente.lp`: medición `higrotermico`, etiqueta `sensor`, campos `temperature_c` y `humidity_rh`, timestamp Unix en segundos.
- Ambos archivos se abren nuevamente al iniciar y se imprimen por serie para inspección.

El registro aportado por el usuario contiene 41 ciclos entre 10:19:11 y 10:39:11 del 14/10/2026, 164 registros por archivo. Los cuatro sensores están presentes en cada ciclo. Se verificó coincidencia de los valores CSV/LP y el timestamp correspondiente en el último ciclo. No se observaron errores de lectura ni mensajes de fallo de escritura en ese registro.

Ejemplo CSV:
```csv
2026-10-14T10:39:11,S01,21.55,56.01,OK
```

Ejemplo LP:
```text
higrotermico,sensor=S01 temperature_c=21.55,humidity_rh=56.01 1791985151
```

## Limitaciones y pendientes

- Prueba funcional de banco; no prueba de resistencia prolongada ni de cortes de energía o extracción de tarjeta.
- `GUARDADO` y relectura validan funcionamiento básico; no garantizan durabilidad frente a fallos súbitos.
- El nombre `influx_pendiente.lp` no implica que exista un gestor de cola: aún no hay ACK, reintentos, cursor persistente ni deduplicación.
- El firmware no envía HTTP ni usa credenciales.
- La marca temporal representa el comienzo de la ronda, no el instante exacto de lectura de cada sensor.
- La conversión UTC-3 presupone que el RTC sigue configurado en hora argentina.
- Para InfluxDB 2.x, enviar con `precision=s`, un token con permisos mínimos y bucket de pruebas autorizado.
