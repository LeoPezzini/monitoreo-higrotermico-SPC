# Arquitectura inicial

La solución se divide en cinco bloques:

1. **Adquisición:** sensores NSHT30.
2. **Multiplexación:** TCA9548A para manejar múltiples sensores I2C.
3. **Control:** ESP32.
4. **Persistencia local:** RTC para timestamp y microSD como buffer.
5. **Comunicación y visualización:** Wi-Fi, servidor/base de datos y Grafana.

## Principio de confiabilidad

La medición se guarda localmente antes de considerarse entregada. Los registros que no cuenten con confirmación remota permanecen disponibles para retransmisión.

La implementación concreta de ACK, reintentos y deduplicación queda pendiente hasta definir el protocolo y el backend existentes.
