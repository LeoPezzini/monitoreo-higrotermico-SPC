# Caracterización comparativa SHT30 — 10 y 11 de noviembre de 2026

## Alcance y límites
Dos series de 501 ciclos con 15 sensores SHT30 activos (S01–S15); S16 deshabilitado por fallas de comunicación. En cada serie: 7515 filas CRC_OK, 501 filas DESHABILITADO, cero estados de error adicionales. Lectura I2C mediante dos multiplexores; registro en microSD. Instrumento Testo 635, sonda 0614 1635, lectura Ti de temperatura como referencia aproximada, registrada manualmente cada ~50 ciclos. Montaje y proximidad espacial limitan la interpretación metrológica. No hubo instrumento patrón de humedad relativa: NO se realizó calibración absoluta de HR.

## Datos originales y trazabilidad
- Primera serie: MED0004.CSV. Ti por ciclo (1,50,...,500): 27.3, 27.3, 27.4, 27.6, 27.6, 27.7, 27.7, 27.8, 27.8, 27.8, 27.8 °C.
- Segunda serie: MED0007.CSV. Ti: 27.4, 27.5, 27.5, 27.5, 27.6, 27.6, 27.6, 27.6, 27.7, 27.7, 27.7 °C.
- Comparación aproximada por interpolación lineal de Ti según número de ciclo. Conservar CSV y capturas originales sin modificar en carpeta de evidencias.

## Promedios por sensor
| Sensor | T serie 1 °C | HR serie 1 % | T serie 2 °C | HR serie 2 % |
|---|---:|---:|---:|---:|
| S01 | 27.379 | 35.284 | 27.545 | 43.990 |
| S02 | 27.295 | 37.569 | 27.488 | 46.183 |
| S03 | 27.266 | 34.895 | 27.494 | 43.336 |
| S04 | 27.207 | 34.863 | 27.441 | 43.390 |
| S05 | 27.197 | 35.072 | 27.458 | 43.504 |
| S06 | 27.120 | 40.845 | 27.405 | 48.720 |
| S07 | 27.126 | 33.360 | 27.430 | 41.644 |
| S08 | 27.133 | 35.761 | 27.426 | 43.762 |
| S09 | 27.142 | 37.177 | 27.417 | 45.112 |
| S10 | 27.138 | 35.231 | 27.432 | 43.389 |
| S11 | 27.132 | 40.973 | 27.434 | 48.622 |
| S12 | 27.264 | 31.226 | 27.562 | 39.223 |
| S13 | 27.212 | 41.459 | 27.504 | 49.133 |
| S14 | 27.205 | 39.090 | 27.496 | 46.840 |
| S15 | 27.344 | 35.693 | 27.587 | 44.212 |

## Selección operativa preliminar de 12 sensores
**Propuesta, pendiente de confirmar físicamente:** S01, S02, S03, S04, S05, S07, S08, S09, S10, S11, S14, S15. Excluir S06, S12 y S13 de los 15 activos; S16 continúa inhabilitado. Criterio práctico: retirar un extremo bajo persistente de HR (S12) y dos extremos altos (S06 y S13), preservando un conjunto de 12 y continuidad del mapeo. **S11 también indica HR alta y es un candidato alternativo a excluir**; la decisión final debe considerar ubicación física y resultado de cableado, no exactitud demostrada. S03 y S04 concuerdan muy bien en HR en ambas series (diferencia entre promedios ~0.032 y ~0.054 puntos porcentuales). La cercanía a la mediana NO demuestra exactitud de HR.

## Uso de la caracterización
- Temperatura: para cada sensor i, un posible offset es C_i = promedio(Ti_Testo_interpolado - Ti_SHT30). Los sesgos entre ambas series difieren; NO fijar ni programar offsets definitivos antes de decidir cómo tratar variación ambiental, colocación e incertidumbre. Conservar medición cruda y, si se usa corrección, guardar además valor corregido y coeficiente/versión aplicada.
- Humedad: NO aplicar offset absoluto ni presentar el promedio del grupo como patrón. Solo comparar consistencia relativa y registrar limitación.
- Firmware: mantener mapeo físico de IDs; no renumerar sin tabla de correspondencias. Cambiar selección de canales únicamente después de confirmar los 12 sensores instalados.
- Próximos ensayos independientes: transmisión al receptor y prueba de cable I2C de 2 metros; no son parte de la caracterización metrológica.
