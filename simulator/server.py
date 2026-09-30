"""Serve the project dashboard with a controllable local NodeMCU model."""

from __future__ import annotations

import argparse
import json
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit
from urllib.request import Request, urlopen

from simulator.model import ControlError, DeviceModel


PROJECT_ROOT = Path(__file__).resolve().parents[1]
STATIC_ROOT = PROJECT_ROOT / "static"
MAX_REQUEST_BYTES = 1024


class SimulatorHandler(SimpleHTTPRequestHandler):
    model = DeviceModel()
    device_host = "127.0.0.1"
    device_port = 80
    virtual_base_url = None

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

    def _request_path(self) -> str:
        return urlsplit(self.path).path or "/"

    def _state_payload(self) -> dict:
        payload = self.model.snapshot()
        host, port = self.server.server_address
        local_url = f"http://{host}:{port}"
        virtual_base_url = self.__class__.virtual_base_url or local_url
        parsed = urlsplit(virtual_base_url)
        device_host = parsed.hostname or host
        device_port = parsed.port or port
        payload["server"] = {
            "status": "running" if payload.get("state") == "running" else "degraded",
            "host": device_host,
            "port": device_port,
            "base_url": virtual_base_url,
            "bind_url": local_url,
            "virtual_url": virtual_base_url,
            "last_request": self._request_path(),
            "routes": [
                "/",
                "/api/state",
                "/api/proxy?url=http://192.168.0.53/api/state",
                "/css/pico.min.css",
                "/css/style.css",
                "/js/main.js",
            ],
        }
        return payload

    def _proxy_remote_state(self) -> None:
        params = parse_qs(urlsplit(self.path).query)
        target = params.get("url", [""])[0].strip()
        if not target:
            self._send_json(400, {"error": "Falta la query ?url=http://.../api/state"})
            return
        try:
            parsed = urlsplit(target)
            if parsed.scheme not in {"http", "https"} or not parsed.netloc:
                raise ValueError("URL remota inválida.")
            request = Request(target, headers={"User-Agent": "NodeMCU-preview/1.0"})
            with urlopen(request, timeout=5) as response:
                payload = response.read()
            data = json.loads(payload.decode("utf-8"))
            if not isinstance(data, dict):
                raise TypeError("La respuesta remota no es un JSON de estado.")
            self._send_json(200, data)
        except Exception as error:
            self._send_json(502, {"error": f"No se pudo obtener el estado remoto: {error}."})

    def do_GET(self) -> None:
        path = self._request_path()
        self.model.record_http_request("GET", path)
        if path == "/api/state":
            self._send_json(200, self._state_payload())
            return
        if path == "/api/proxy":
            self._proxy_remote_state()
            return
        if path.startswith("/api/"):
            self._send_json(404, {"error": "Ruta de simulación inexistente."})
            return
        super().do_GET()

    def do_POST(self) -> None:
        path = self._request_path()
        self.model.record_http_request("POST", path)
        if path != "/api/controls":
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
        self._send_json(200, self._state_payload())

    def log_message(self, format: str, *args) -> None:
        print(f"[sim] {self.address_string()} - {format % args}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Simula el monitor web de la NodeMCU ESP8266.")
    parser.add_argument("--host", default="127.0.0.1", help="Interfaz de escucha; usar 0.0.0.0 para simular un servidor visible en la red local.")
    parser.add_argument("--port", type=int, default=8080, help="Puerto HTTP (por defecto: 8080).")
    parser.add_argument("--device-ip", default="127.0.0.1", help="IP virtual que la placa simula en la red local; por defecto, vista local de la NodeMCU.")
    parser.add_argument("--device-port", type=int, default=80, help="Puerto virtual que la placa simula; por defecto 80.")
    parser.add_argument("--open-url", default=None, help="URL que se abrirá en el navegador; por defecto se usa la URL virtual de la placa.")
    parser.add_argument("--no-open", action="store_true", help="No abrir el navegador automáticamente (útil en CI/tests).")
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), SimulatorHandler)
    browser_host = "127.0.0.1" if args.host in {"0.0.0.0", "::"} else args.host
    local_url = f"http://{browser_host}:{args.port}"
    SimulatorHandler.virtual_base_url = f"http://{args.device_ip}:{args.device_port}"
    browser_url = args.open_url or local_url
    if args.host in {"127.0.0.1", "localhost"} and args.open_url is None:
        print(f"La vista local del simulador se abrirá en {browser_url}.")
        print(f"La NodeMCU virtual se presenta como {SimulatorHandler.virtual_base_url}, pero esa IP solo es válida si existe realmente en tu red.")
    else:
        browser_url = args.open_url or local_url
        print(f"Simulador NodeMCU disponible en http://{args.host}:{args.port}")
        print(f"Vista local recomendada para abrir en este equipo: {browser_url}")
        print(f"Vista de placa simulada: {SimulatorHandler.virtual_base_url}")
        print("Esta IP virtual solo funciona si existe realmente en tu red local; si no, usa la vista local del navegador.")

    if not args.no_open:
        try:
            import webbrowser

            webbrowser.open(browser_url, new=2)
        except Exception as exc:
            print(f"Advertencia: no se pudo abrir el navegador automáticamente: {exc}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nCerrando simulador.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
