# Auditoría de consistencia documental y técnica — 28/10/2026

## Alcance

Revisión del árbol completo del repositorio, del README, registro de decisiones, arquitectura, firmware de reenvío, documentación de pruebas de octubre y receptor Python/Docker. La auditoría **no implica ejecutar físicamente** el ESP32, el servidor ni pruebas automatizadas; se contrastan archivos del repositorio con los resultados de ensayos ya documentados.

## Cambios documentales realizados

1. `docs/decisiones/decision-log.md`: actualización de estados DEC-001 a DEC-005 y incorporación DEC-007 a DEC-019, con evidencia, decisiones físicas y pendientes.
2. `docs/arquitectura/arquitectura-sistema.md`: sustitución de la descripción inicial por arquitectura de cuatro sensores probada, buses, pines de banco, persistencia, ACK, Docker, seguridad y montaje proyectado.
3. `README.md`: actualización de cantidad estimada de sensores (~11, por confirmar), esquema, estado y próximos pasos; corrección de filas de tabla.
4. `servidor/receptor-spc/README.md`: aclaración de que el código publicado es una implementación documentada, pero aún no se comprobó su identidad exacta con el script que corrió en Ubuntu.

## Inventario por áreas

| Área | Contenido existente | Estado / observación |
| --- | --- | --- |
| `README.md` | Objetivo, inventario, ensayos, próximos pasos | Actualizado; algunos apartados históricos describen explícitamente pruebas anteriores |
| `docs/decisiones/` | Decisiones de diseño | Actualizado a DEC-019 |
| `docs/arquitectura/` | Arquitectura de bloques y protocolos | Actualizada con límites de evidencia |
| `docs/pruebas/` | CSV 05/10, pruebas CRC 13/10, RTC/SD/LP 14/10, HTTP 15/10, cola/Grafana 19/10, Docker/Ubuntu 21–22/10 | Historial valioso; no alterar resultados ni transformar ensayos en garantías |
| `firmware/pruebas/` | Sketches incrementales de sensores, PCA, CRC, RTC, SD, LP, HTTP y reenvío | Conservar como historial de desarrollo; no reemplazar por una supuesta versión final |
| `servidor/receptor-spc/` | Python, Dockerfile, ejemplo de variables y manual | Referencia operativa; comparar con script efectivamente desplegado |
| `.gitignore` | Exclusiones de secretos locales | Verificar antes de cualquier publicación nueva; nunca subir tokens reales |

## Brechas prioritarias para cerrar antes de entregar

### A. Firmware final reproducible — prioridad alta
El archivo `firmware/pruebas/integracion_4sensores_http_reenvio/integracion_4sensores_http_reenvio.ino` aún tiene endpoint directo a InfluxDB en el puerto 8086 y cabecera `Authorization: Token ...` obtenida de `secrets.h`. **No es la versión exacta que se describió para el receptor Docker 8080.** Recuperar el sketch realmente cargado al ESP32 en la validación de Ubuntu, revisar que no contenga secretos y archivarlo como versión diferenciada; no modificar a ciegas el sketch histórico. Asegurar que el firmware definitivo adquiera continuamente y no se detenga tras 20 ciclos.

### B. Receptor en servidor — prioridad alta
Contrastar el `receptor_spc.py` publicado con `~/spc-receptor` de Ubuntu, confirmar comando Docker, parámetros de entorno sin mostrar valores secretos y estado de contenedor. El 22/10 quedó detenido tras las pruebas. Acordar con Fede procedimiento de puesta en servicio, red y permisos.

### C. Robustez y seguridad — prioridad alta
Definir controles de acceso al puerto 8080; validar cortes reales, recuperación, RTC, SD llena/corrupta, crecimiento de cola y ACK, tiempo de recuperación, duplicados y consistencia temporal. El receptor no autentica clientes ni dispone de cola propia.

### D. Montaje eléctrico y mecánico — prioridad alta
Confirmar sensores y ubicaciones (~11 estimados); medir cableado blindado I²C, documentar pinout real, alimentación UPS/regulador, gabinete y protección de sensores empotrados/exteriores. Los módulos adquiridos todavía no están ensayados.

### E. Metrología y trazabilidad — prioridad alta
Asignar IDs físicos a los 20 NSHT30, comparar con referencia, documentar condiciones, resultados y criterios de aceptación. No llamar «calibración certificada» a una comparación sin trazabilidad.

### F. Evidencia para informe — prioridad media-alta
Recuperar o repetir capturas originales del monitor serie (CRC, CSV/SD, HTTP -1/204, ACK), terminal Docker Ubuntu, consulta Flux, dashboard Grafana y fotografías de banco/montaje. Guardar fecha, versión de código, condiciones y datos exportables junto a cada figura.

## Aspectos que deliberadamente NO se cambiaron

- Firmware de banco o lógica de ACK: la documentación no es una autorización para modificar el sistema probado.
- Contenedores ni servicios del servidor del laboratorio, ni repositorios de terceros.
- Credenciales, IP de prueba o secretos.
- Resultados históricos de ensayos y conteos: se conservan con sus límites.
- Alarmas por Telegram o correo: extensión futura fuera del alcance actual.

## Criterio de cierre

La documentación queda alineada con lo que **se ha comprobado hasta el 22/10** y con las decisiones de diseño comunicadas hasta el 28/10. La instalación definitiva y su robustez todavía requieren validación física y evidencia reproducible.
