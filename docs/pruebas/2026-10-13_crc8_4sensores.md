# Prueba de CRC-8 NSHT30 — 2026-10-13

## Objetivo
Verificar por separado los CRC-8 de temperatura y humedad relativa en cuatro NSHT30 a través de dos PCA9548A, sin alterar el montaje de integración RTC/microSD.

## Montaje
- ESP32: SDA GPIO21, SCL GPIO22, bus I2C a 100 kHz.
- PCA 0x70: S01 canal 0, S02 canal 1 (A0 bajo).
- PCA 0x71: S03 canal 0, S04 canal 1 (A0 alto).
- RTC y microSD permanecieron conectados, pero no se utilizaron en esta prueba.
- Trama NSHT30: T_MSB, T_LSB, CRC_T, HR_MSB, HR_LSB, CRC_HR.
- CRC-8: inicial 0xFF, polinomio 0x31, sin reflexión; se calcula sobre cada pareja de bytes de datos.

## Resultado observado (ciclo completo compartido por monitor serie)
| Sensor | Temperatura (°C) | HR (%) | CRC temperatura | CRC humedad |
| --- | ---: | ---: | --- | --- |
| S01 | 23.79 | 48.33 | OK | OK |
| S02 | 23.97 | 48.28 | OK | OK |
| S03 | 23.88 | 49.96 | OK | OK |
| S04 | 23.83 | 47.16 | OK | OK |

Los cuatro sensores reportaron MEDICION VALIDA en el ciclo mostrado. Diferencia máxima entre temperaturas: 0.18 °C. Esto valida el chequeo de integridad de trama en esta configuración; no es calibración ni prueba de estabilidad prolongada.

## Incidencia
Durante la prueba, S02 inicialmente no respondió al comando de medición y Wire.endTransmission() devolvió código 4. Los otros tres sensores respondían y validaban ambos CRC. Se revisaron las conexiones de la protoboard y se fijaron niveles de dirección A0/A1/A2 de los multiplexores; posteriormente S02 volvió a responder y validó ambos CRC. Se sospecha contacto intermitente, sin causa confirmada. No atribuir el código 4 a una causa única.

## Próximo paso
Incorporar validación CRC y diagnóstico diferenciado al sketch de integración de cuatro sensores, RTC y microSD, conservando este sketch independiente como referencia. Luego probar errores, repetición de ciclos y ampliación a 14 sensores.
