# Registro de decisiones

## DEC-001 — microSD como buffer local
**Estado:** aceptada conceptualmente.

Se conservarán localmente las mediciones para evitar pérdida de datos durante interrupciones de conectividad. Un registro permanecerá disponible hasta contar con confirmación suficiente de persistencia remota.

## DEC-002 — RTC para identificación temporal
**Estado:** aceptada conceptualmente.

Cada medición deberá incluir fecha y hora. El RTC permitirá mantener una referencia temporal aun cuando no exista conectividad. La lectura y generación de timestamps ya fueron validadas; queda pendiente verificar retención durante cortes con batería LIR2032.

## DEC-003 — integración con infraestructura existente
**Estado:** pendiente de relevamiento.

Se priorizará reutilizar la infraestructura de base de datos y Grafana ya utilizada por el grupo de investigación antes de seleccionar un backend alternativo.

La transmisión Wi-Fi mediante HTTP POST con JSON fue validada en banco con una medición real de NSHT30. Si la infraestructura existente ofrece una API HTTP compatible, se priorizará este mecanismo para evitar introducir un protocolo adicional sin necesidad. La decisión definitiva depende del relevamiento del sistema real.

## DEC-004 — alimentación de respaldo
**Estado:** a evaluar.

Se prevé estudiar una solución UPS DC / power-path que permita funcionamiento continuo y carga simultánea de batería. El dimensionamiento se realizará después de medir el consumo del sistema.

## DEC-005 — dos PCA9548A para la adquisición final
**Estado:** aceptada para continuar el desarrollo.

Dos módulos PCA9548A nuevos fueron validados individualmente y en simultáneo. Se utilizarán inicialmente en direcciones `0x70` y `0x71`, con capacidad suficiente para los 14 NSHT30 previstos. El TCA9548A previamente funcional queda disponible como repuesto.

## DEC-006 — timestamp común por ciclo de adquisición
**Estado:** aceptada para las pruebas actuales.

Se toma una referencia temporal al comienzo de cada ciclo y se asocia el mismo timestamp al conjunto de sensores de esa ronda. Esto representa las mediciones como pertenecientes a un mismo ciclo aunque las lecturas I2C se ejecuten secuencialmente.
