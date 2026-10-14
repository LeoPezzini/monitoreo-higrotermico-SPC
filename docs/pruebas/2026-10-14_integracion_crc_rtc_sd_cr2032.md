# Integración de cuatro NSHT30, CRC-8, RTC y microSD — 2026-10-14

## Resultado observado
Se ejecutó el sketch integrado con dos PCA9548A y cuatro NSHT30. Los cuatro sensores devolvieron CRC correcto de temperatura y humedad relativa y el monitor serie informó escritura de cada registro en la microSD. El archivo no se releyó en esta sesión para comprobar su contenido.

Ciclo a las 10:05:56 (hora mostrada por el RTC):
| Sensor | Temperatura °C | HR % | CRC T / HR | SD |
|---|---:|---:|---|---|
| S01 | 21.30 | 53.52 | OK / OK | escritura reportada |
| S02 | 21.22 | 53.44 | OK / OK | escritura reportada |
| S03 | 21.61 | 54.30 | OK / OK | escritura reportada |
| S04 | 21.52 | 51.74 | OK / OK | escritura reportada |

## Modificación de respaldo del RTC
Para utilizar una pila primaria CR2032 en lugar de una LIR2032, se identificó mediante continuidad un camino de carga VCC → resistencia SMD → diodo → positivo del portapila. Se retiró la resistencia serie. Luego se comprobó ausencia de continuidad desde VCC hacia el ánodo del diodo y una lectura prácticamente nula de tensión en los contactos del portapila vacío con el RTC alimentado. Es una validación práctica del camino identificado, no una caracterización exhaustiva de todos los posibles caminos de corriente.

Con la CR2032 colocada, el RTC se ajustó una vez a fecha/hora de compilación y se volvió a cargar el sketch con AJUSTAR_RTC=false. Tras un corte de alimentación, la lectura avanzó desde 2026-10-14T10:06:26 hasta 2026-10-14T10:08:38, sin reajuste automático. Respaldo validado en corte breve (aprox. 2 min 12 s entre las lecturas mostradas); duración a largo plazo no ensayada.

## Precauciones y próximos pasos
- Mantener AJUSTAR_RTC=false en uso normal; true solo para puesta en hora inicial.
- La fecha/hora de compilación debe contrastarse con una referencia y considerar la zona horaria.
- El RTC actualmente contiene hora civil local de Argentina, no UTC; antes de escribir timestamps Unix a InfluxDB convertir correctamente a UTC (Argentina UTC-3) o definir explícitamente otra política de tiempo.
- La siguiente prueba será construir una línea InfluxDB Line Protocol con una medición real, sin transmisión al servidor.
- Pendientes: validar contenido CSV tras escritura, manejo de errores, pruebas prolongadas, ampliación a 14 sensores y envío con ACK.
