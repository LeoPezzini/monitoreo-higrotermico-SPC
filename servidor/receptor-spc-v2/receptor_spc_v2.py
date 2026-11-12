"""Receptor SPC v2: CRC16-CCITT-FALSE y deduplicacion persistente.
POST /api/v2/write, X-SPC-ID, X-SPC-CRC16, body: una linea Influx LP UTF-8.
CRC16 sobre UTF8(ID) + b'\n' + body exacto. HTTP: 204/400/409/422/503.
"""
import os
import re
import sqlite3
import threading
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

PORT = int(os.environ.get('PORT', '8080'))
DB_PATH = Path(os.environ.get('SPC_DB_FILE', '/datos/spc_v2.sqlite3'))
MAX_BYTES = 4096
ID_PATTERN = re.compile(r'[A-Za-z0-9_-]{1,64}\Z')
CRC_PATTERN = re.compile(r'[0-9A-Fa-f]{4}\Z')
lock = threading.Lock()

def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

def connect_db():
    connection = sqlite3.connect(DB_PATH, timeout=10)
    connection.execute('PRAGMA synchronous=FULL')
    return connection

def init_db():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    with connect_db() as conn:
        conn.execute("""CREATE TABLE IF NOT EXISTS mediciones (
            measurement_id TEXT PRIMARY KEY,
            payload BLOB NOT NULL,
            crc16 TEXT NOT NULL,
            received_utc TEXT NOT NULL
        )""")

class ReceptorSPC(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path.split('?', 1)[0] != '/api/v2/write':
            self.send_error(404, 'Ruta no encontrada')
            return
        measurement_id = self.headers.get('X-SPC-ID', '')
        crc_text = self.headers.get('X-SPC-CRC16', '')
        if not ID_PATTERN.fullmatch(measurement_id) or not CRC_PATTERN.fullmatch(crc_text):
            self.send_error(400, 'Falta ID o CRC16 valido')
            return
        try:
            length = int(self.headers.get('Content-Length', '0'))
        except ValueError:
            self.send_error(400, 'Content-Length invalido')
            return
        if not 0 < length <= MAX_BYTES:
            self.send_error(400, 'Tamano invalido')
            return
        data = self.rfile.read(length)
        if len(data) != length:
            self.send_error(400, 'Cuerpo incompleto')
            return
        try:
            line = data.decode('utf-8')
            if not line.strip() or '\r' in line or '\n' in line:
                raise ValueError('Debe ser una sola linea LP')
        except (UnicodeDecodeError, ValueError):
            self.send_error(400, 'Cuerpo LP invalido')
            return
        actual = f'{crc16_ccitt_false(measurement_id.encode("utf-8") + b"\n" + data):04X}'
        if actual != crc_text.upper():
            self.send_error(422, 'CRC16 incorrecto')
            return
        try:
            with lock:
                with connect_db() as conn:
                    previous = conn.execute('SELECT payload FROM mediciones WHERE measurement_id = ?', (measurement_id,)).fetchone()
                    if previous is not None:
                        if previous[0] != data:
                            self.send_error(409, 'ID existente con datos distintos')
                            return
                        print(f'DUPLICADO CONFIRMADO {measurement_id}', flush=True)
                    else:
                        conn.execute('INSERT INTO mediciones VALUES (?, ?, ?, ?)', (
                            measurement_id, data, actual, datetime.now(timezone.utc).isoformat()
                        ))
                        print(f'GUARDADO {measurement_id} CRC={actual}: {line}', flush=True)
        except sqlite3.Error as exc:
            print(f'ERROR DE BASE DE DATOS: {exc}', flush=True)
            self.send_response(503)
            self.end_headers()
            return
        self.send_response(204)
        self.end_headers()

if __name__ == '__main__':
    init_db()
    print(f'Receptor SPC v2 escuchando en 0.0.0.0:{PORT}', flush=True)
    print(f'Base de datos persistente: {DB_PATH}', flush=True)
    ThreadingHTTPServer(('0.0.0.0', PORT), ReceptorSPC).serve_forever()
