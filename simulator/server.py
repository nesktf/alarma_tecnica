"""Serve the project dashboard with a controllable local NodeMCU model."""

from __future__ import annotations

import argparse
import json
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

from simulator.model import ControlError, DeviceModel


PROJECT_ROOT = Path(__file__).resolve().parents[1]
STATIC_ROOT = PROJECT_ROOT / "static"
MAX_REQUEST_BYTES = 1024


class SimulatorHandler(SimpleHTTPRequestHandler):
    model = DeviceModel()

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(STATIC_ROOT), **kwargs)

    def _send_json(self, status: int, data: dict) -> None:
        body = json.dumps(data, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if urlsplit(self.path).path == "/api/state":
            self._send_json(200, self.model.snapshot())
            return
        if urlsplit(self.path).path.startswith("/api/"):
            self._send_json(404, {"error": "Ruta de simulación inexistente."})
            return
        super().do_GET()

    def do_POST(self) -> None:
        if urlsplit(self.path).path != "/api/controls":
            self._send_json(404, {"error": "Ruta de simulación inexistente."})
            return
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._send_json(400, {"error": "Content-Length inválido."})
            return
        if content_length <= 0 or content_length > MAX_REQUEST_BYTES:
            self._send_json(413, {"error": "El cuerpo debe tener entre 1 y 1024 bytes."})
            return
        try:
            controls = json.loads(self.rfile.read(content_length))
            self.model.apply_controls(controls)
        except (json.JSONDecodeError, UnicodeDecodeError) as error:
            self._send_json(400, {"error": f"JSON inválido: {error}."})
            return
        except ControlError as error:
            self._send_json(400, {"error": str(error)})
            return
        self._send_json(200, self.model.snapshot())

    def log_message(self, format: str, *args) -> None:
        print(f"[sim] {self.address_string()} - {format % args}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Simula el monitor web de la NodeMCU ESP8266.")
    parser.add_argument("--host", default="127.0.0.1", help="Interfaz de escucha (por defecto, solo localhost).")
    parser.add_argument("--port", type=int, default=8080, help="Puerto HTTP (por defecto: 8080).")
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), SimulatorHandler)
    print(f"Simulador NodeMCU disponible en http://{args.host}:{args.port}")
    print("Solo simula telemetría; no ejecuta ni emula el firmware Xtensa.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nCerrando simulador.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
