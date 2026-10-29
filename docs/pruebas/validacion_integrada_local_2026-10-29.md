# Validación integrada local: NSHT30 + RTC + OLED + microSD — 29/10/2026

## Alcance
Ensayo de banco sin Wi-Fi ni transmisión HTTP, con ESP32 DevKit de 30 pines, 4 sensores NSHT30 distribuidos entre 2 multiplexores PCA9548A, RTC DS3231, OLED SSD1306 (128 × 32) y microSD HW-203. Firmware de prueba independiente del validado en el laboratorio.

## Configuración y procedimiento
- Multiplexores 0x70 y 0x71 y OLED 0x3C en I²C principal: SDA GPIO21, SCL GPIO22.
- Sensores S01/S02 en canales 0/1 del primer multiplexor; S03/S04 en canales 0/1 del segundo. Dirección de sensores 0x44; CRC-8 comprobado antes de aceptar cada lectura.
- RTC en I²C independiente: SDA GPIO16, SCL GPIO17. Hora local argentina; marca temporal local para CSV y epoch UTC en Line Protocol.
- microSD SPI: CS GPIO26, MOSI GPIO13, SCK GPIO14, MISO GPIO27.
- Periodicidad programada: 30 segundos. 20 ciclos por ejecución. Salidas `/prueba_local_spc.csv` y `/prueba_local_spc.lp` en modo agregado; OLED rota entre cuatro sensores y estado general.

## Evidencia observada
En la primera ejecución se verificó el ciclo 1 (29/10/2026 18:56:57):
| Sensor | Temperatura (°C) | Humedad relativa (%) | CSV | LP |
|---|---:|---:|---|---|
| S01 | 24.81 | 35.20 | OK | OK |
| S02 | 24.62 | 35.98 | OK | OK |
| S03 | 24.83 | 38.87 | OK | OK |
| S04 | 24.48 | 35.53 | OK | OK |

La primera ejecución alcanzó 48 registros completos hasta el ciclo 12. Al mover el montaje para fotografiarlo, se observó `SD:ERR` en la OLED y el contador permaneció en 48. Se sospecha un falso contacto en jumpers del módulo microSD, **sin confirmación causal**. El firmware de prueba, una vez que `sdOK` pasa a falso, no reintenta inicializar la tarjeta.

En una segunda ejecución, sin mover el montaje, el usuario informó que se completaron los **20 ciclos con 80 registros completos**, con mensaje final `PRUEBA FINALIZADA: no se toman mas mediciones.` Se guardó una captura de los ciclos 17 a 20 y del cierre. Esta es una **validación funcional del ensayo de banco**, no una prueba de operación continua prolongada ni de tolerancia a cortes de alimentación.

El usuario confirmó también la compatibilidad mecánica de los conectores JST-XH y headers hembra con su placa preperforada.

## Archivos y límites de la evidencia
Los mensajes del monitor serie confirman que el firmware reportó escritura CSV y LP satisfactoria. **Pendiente:** retirar la tarjeta con el equipo apagado y revisar ambos archivos en una computadora, contar filas, comprobar timestamps, contenido, duplicados y coherencia. Como el firmware agrega al mismo archivo en sucesivos arranques, los archivos podrían contener registros de ambas ejecuciones; no asumir que tienen solamente 80 filas.

Las fotos del montaje, la OLED y las capturas del monitor serie fueron tomadas y organizadas por el usuario, pero aún no están adjuntadas a este documento.

## Decisiones para montaje definitivo
- Placa preperforada como base permanente; ESP32, multiplexores, RTC y microSD desmontables mediante headers hembra.
- Conectores JST-XH de cuatro contactos para sensores, con pinout uniforme documentado y etiquetas por canal.
- Ubicar microSD de modo que la tarjeta sea accesible y asegurar mecánicamente módulos y conexiones.
- Alimentación desde UPS de 5 V con interruptor y regulación 3,3 V, pendientes de validación eléctrica.
- Para firmware definitivo: recuperación de microSD, gestión de fallas, adquisición continua y visualización legible sin afectar el muestreo.

## Próxima validación
Inspección de CSV/LP y contraste con capturas, seguido de diseño de distribución física de módulos y alimentación.
