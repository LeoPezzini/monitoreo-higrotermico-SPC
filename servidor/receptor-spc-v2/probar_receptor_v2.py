import sys
import urllib.request
import urllib.error

URL = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8080/api/v2/write"
MID = "SPC-PRUEBA-0001"
BODY = b"higrotermico,sensor=TEST temperature_c=25.0,humidity_rh=50.0 1795000000"

def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

crc = f"{crc16_ccitt_false(MID.encode() + b'\n' + BODY):04X}"
headers = {
    "X-SPC-ID": MID,
    "X-SPC-CRC16": crc,
    "Content-Type": "text/plain; charset=utf-8",
}

for n in (1, 2):
    req = urllib.request.Request(URL, data=BODY, method="POST", headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            print(f"Envio {n}: HTTP {r.status} (esperado 204)")
    except urllib.error.HTTPError as e:
        print(f"Envio {n}: HTTP {e.code} ERROR: {e.read().decode(errors='replace')}")
        sys.exit(1)
    except Exception as e:
        print(f"Envio {n}: ERROR {type(e).__name__}: {e}")
        sys.exit(1)

print("OK: el segundo envio usa el mismo ID/payload y debe ser aceptado como duplicado identico.")
