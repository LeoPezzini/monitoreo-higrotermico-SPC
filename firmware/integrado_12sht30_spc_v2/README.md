# Firmware integrado — doce SHT30 y receptor v2

Archivo: [integrado_12sht30_spc_v2.ino](integrado_12sht30_spc_v2.ino).

Esta es una copia del firmware de banco utilizado en las pruebas integradas de noviembre de 2026, no una imagen binaria certificada del despliegue final.

- Sensores: S01 S02 S03 S04 S05 S07 S08 S09 S10 S11 S14 S15; dos MUX 0x70/0x71.
- I2C sensores: SDA21/SCL22 a 100 kHz. RTC independiente SDA17/SCL16.
- microSD: SCK14/MISO27/MOSI13/CS26.
- CRC8 por canal de temperatura/humedad; CRC16-CCITT-FALSE a nivel de aplicacion; HTTP POST con X-SPC-ID y X-SPC-CRC16; ACK 204 y reintentos desde microSD.
- El sketch importa `secrets.h`. Copiar `secrets.example.h` como `secrets.h` y configurar SSID, clave y URL del servidor. **Nunca subir secretos reales a GitHub.**
- El RTC se ajusto una unica vez con fecha/hora de compilacion; en el sketch publicado `AJUSTAR_RTC_UNA_VEZ=false`. No activar en funcionamiento habitual, para no sobrescribir la hora del reloj.
- **Confirmar el pinout de los conectores y la alimentacion con multimetro** antes de enchufar sensores exteriores; el pinout mecanico final no se verifico aun.
- El firmware solo conoce el receptor v2 y recibe su confirmacion SQLite. InfluxDB y Grafana son servicios del servidor; no corresponden a una conexion directa del ESP32.

Ver [registro de validacion](../../docs/pruebas/validacion-integrada-12-sht30-2026-11-12.md).
