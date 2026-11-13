# SPC — Instalación definitiva en laboratorio

## Identificadores de ubicación (sin S01/S07)

| Ubicación | MUX | Canal |
|---|---|---|
| C4I | 0x71 | 6 |
| C0I | 0x71 | 5 |
| Y3I | 0x71 | 4 |
| Y0I | 0x71 | 3 |
| PcI | 0x71 | 2 |
| P0I | 0x71 | 1 |
| AmbI | 0x71 | 0 |
| AmbE | 0x70 | 7 |
| C4E | 0x70 | 6 |
| C0E | 0x70 | 5 |
| Y3E | 0x70 | 4 |
| Y0E | 0x70 | 3 |

Los 12 SHT30 usan dirección I²C 0x44 detrás de dos multiplexores; bus ESP32 SDA GPIO21 / SCL GPIO22. OLED SSD1306 128×32 en bus principal (0x3C). RTC DS3231 bus separado SDA17/SCL16; microSD SPI SCK14/MISO27/MOSI13/CS26.

## Cadena de datos

ESP32 → Wi-Fi → receptor HTTP V2 (CRC16-CCITT-FALSE, HTTP 204) → SQLite persistente → sincronizador → InfluxDB 2.x → Grafana.

El tag Influx `sensor` mantiene ese nombre de campo por compatibilidad, pero **su valor es la ubicación**, nunca un número Sxx. Los IDs HTTP también incluyen la ubicación, p. ej. `SPC01-C4I-0000000001`. Los paneles Grafana deben mostrar esas etiquetas.

## Procedimiento de instalación limpia

1. Con Fede, entrar por Termius o SSH nativo (`ssh USUARIO@IP`) e inventariar: `docker ps -a`, `docker volume ls`, `docker compose version`. No presuponer que la IP doméstica 192.168.1.52 sirve en laboratorio.
2. Respaldar en la notebook CSV y cola microSD, dashboards JSON y evidencia de las pruebas.
3. En el servidor **ya hay bucket y dashboards de prueba**: borrar únicamente esos recursos del proyecto tras respaldo; no eliminar otros datos de InfluxDB/Grafana. **SQLite V2 no existía allí**: crear desde cero.
4. Instalar receptor HTTP V2 con SQLite en volumen persistente nuevo y sincronizador con su estado persistente; verificar puerto publicado y ruta `/api/v2/write`. Configurar token, organización y bucket final de InfluxDB sin subir secretos a GitHub.
5. Cargar firmware de **30 segundos** para validación, configurando `secrets.h` para la Wi-Fi/IP/puerto del laboratorio. Verificar 12/12 sensores, HTTP 204, SQLite, InfluxDB y paneles.
6. Detener receptor 2–3 minutos, comprobar adquisición y cola SD durante la caída, reiniciarlo y verificar reenvío y deduplicación.
7. **Después de las pruebas**, detener servicios, respaldar, formatear microSD FAT32 y reiniciar únicamente SQLite/estado de sincronización dedicados a SPC; vaciar datos de prueba del bucket definitivo o crear otro limpio.
8. Cargar firmware de **10 minutos**; verificar primer ciclo, identificadores por ubicación y que el bucket y SQLite de producción solo contengan datos posteriores a la puesta en servicio.
9. Documentar fecha/hora de instalación, IP/puerto, versión de firmware, bucket, rutas de volúmenes y capturas. Nunca publicar `secrets.h` ni `.env` con credenciales.

**Precaución:** no usar `docker compose down -v` ni eliminar volúmenes de servicios compartidos. Un HTTP 204 confirma aceptación en SQLite, no necesariamente escritura ya completada en InfluxDB.

El documento detallado y ambos archivos `.ino` se entregaron también como adjuntos de la conversación para validación en Arduino IDE antes de la instalación.
