"""Sincroniza SQLite del receptor SPC v2 con InfluxDB 2.x, sin alterar el receptor."""
import os
import sqlite3
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

SOURCE = os.getenv('SPC_SOURCE_DB', '/datos/spc_v2.sqlite3')
STATE = os.getenv('SPC_SYNC_DB', '/estado/sync.sqlite3')
URL = os.getenv('INFLUX_URL', 'http://host.docker.internal:8086').rstrip('/')
ORG = os.environ['INFLUX_ORG']
BUCKET = os.environ['INFLUX_BUCKET']
TOKEN = os.environ['INFLUX_TOKEN']
INTERVAL = int(os.getenv('SPC_SYNC_INTERVAL', '5'))
BATCH = int(os.getenv('SPC_SYNC_BATCH', '100'))

Path(STATE).parent.mkdir(parents=True, exist_ok=True)
with sqlite3.connect(STATE) as db:
    db.execute('CREATE TABLE IF NOT EXISTS enviados (measurement_id TEXT PRIMARY KEY, enviado_utc TEXT DEFAULT CURRENT_TIMESTAMP)')

endpoint = URL + '/api/v2/write?' + urllib.parse.urlencode({'org': ORG, 'bucket': BUCKET, 'precision': 's'})
print(f'Sincronizador SPC iniciado: {SOURCE} -> {URL} org={ORG} bucket={BUCKET}', flush=True)

while True:
    try:
        with sqlite3.connect(f'file:{SOURCE}?mode=ro', uri=True, timeout=10) as source, sqlite3.connect(STATE, timeout=10) as state:
            source.execute('ATTACH DATABASE ? AS estado', (STATE,))
            rows = source.execute("""
                SELECT m.measurement_id, m.payload FROM mediciones AS m
                LEFT JOIN estado.enviados AS e ON e.measurement_id = m.measurement_id
                WHERE e.measurement_id IS NULL
                ORDER BY m.rowid LIMIT ?
            """, (BATCH,)).fetchall()
            if rows:
                payload = b'\n'.join(bytes(r[1]) for r in rows)
                request = urllib.request.Request(endpoint, data=payload, method='POST', headers={
                    'Authorization': 'Token ' + TOKEN,
                    'Content-Type': 'text/plain; charset=utf-8',
                    'Accept': 'application/json',
                })
                with urllib.request.urlopen(request, timeout=20) as response:
                    if response.status != 204:
                        raise RuntimeError(f'InfluxDB respondio HTTP {response.status}')
                with state:
                    state.executemany('INSERT OR IGNORE INTO enviados (measurement_id) VALUES (?)', [(r[0],) for r in rows])
                print(f'INFLUX 204: {len(rows)} registros sincronizados; ultimo={rows[-1][0]}', flush=True)
            else:
                time.sleep(INTERVAL)
    except (sqlite3.Error, urllib.error.URLError, TimeoutError, RuntimeError, OSError) as exc:
        print(f'PENDIENTE / REINTENTO: {type(exc).__name__}: {exc}', flush=True)
        time.sleep(INTERVAL)
