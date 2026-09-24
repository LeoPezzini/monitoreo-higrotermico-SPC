# Registro de decisiones

## DEC-001 — microSD como buffer local
**Estado:** aceptada conceptualmente.

Se conservarán localmente las mediciones para evitar pérdida de datos durante interrupciones de conectividad. Un registro permanecerá disponible hasta contar con confirmación suficiente de persistencia remota.

## DEC-002 — RTC para identificación temporal
**Estado:** aceptada conceptualmente.

Cada medición deberá incluir fecha y hora. El RTC permitirá mantener una referencia temporal aun cuando no exista conectividad.

## DEC-003 — integración con infraestructura existente
**Estado:** pendiente de relevamiento.

Se priorizará reutilizar la infraestructura de base de datos y Grafana ya utilizada por el grupo de investigación antes de seleccionar un protocolo o backend alternativo.

## DEC-004 — alimentación de respaldo
**Estado:** a evaluar.

Se prevé estudiar una solución UPS DC / power-path que permita funcionamiento continuo y carga simultánea de batería. El dimensionamiento se realizará después de medir el consumo del sistema.
