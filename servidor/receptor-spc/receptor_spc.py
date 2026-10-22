"""Receptor HTTP mínimo para Line Protocol InfluxDB 2.x.

Recibe POST /api/v2/write en puerto 8080 y reenvía el cuerpo a INFLUX_URL.
El token queda exclusivamente en variables de entorno.
Prueba de integración, no despliegue endurecido para Internet.
"""
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError

PUERTO = 8080
INFLUX_URL = os.environ["INFLUX_URL"]
INFLUX_TOKEN = os.environ["INFLUX_TOKEN"].strip()


class ReceptorSPC(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path.split("?")[0] != "/api/v2/write":
            self.send_error(404)
            return

        try:
            longitud = int(self.headers.get("Content-Length", "0"))
            if longitud <= 0 or longitud > 4096:
                self.send_error(400, "Tamano invalido")
                return

            datos = self.rfile.read(longitud)
            solicitud = Request(
                INFLUX_URL,
                data=datos,
                headers={
                    "Authorization": "Token " + INFLUX_TOKEN,
                    "Content-Type": "text/plain; charset=utf-8",
                },
                method="POST",
            )
            with urlopen(solicitud, timeout=15) as respuesta:
                codigo = respuesta.status
            print("InfluxDB: HTTP", codigo, flush=True)
            self.send_response(204 if codigo == 204 else 503)

        except (HTTPError, URLError, TimeoutError, OSError, ValueError) as error:
            print("Error de reenvio:", error, flush=True)
            self.send_response(503)

        self.end_headers()


servidor = ThreadingHTTPServer(("0.0.0.0", PUERTO), ReceptorSPC)
print("Receptor HTTP escuchando en puerto", PUERTO, flush=True)
print("Esperando mediciones del ESP32...", flush=True)

try:
    servidor.serve_forever()
except KeyboardInterrupt:
    print("Deteniendo receptor...")
finally:
    servidor.server_close()
