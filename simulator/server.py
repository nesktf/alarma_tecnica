"""Serve the project dashboard with a controllable local NodeMCU model."""

from __future__ import annotations

import argparse
import ipaddress
import json
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener, urlopen

from simulator.model import ControlError, DeviceModel

PROJECT_ROOT = Path(__file__).resolve().parents[1]
STATIC_ROOT = PROJECT_ROOT / "static"
MAX_REQUEST_BYTES = 1024
MAX_PROXY_RESPONSE_BYTES = 65536


class _NoRedirectHandler(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def _is_safe_proxy_target(target_url: str) -> bool:
    parsed = urlsplit(target_url)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        return False
    if not parsed.path.endswith("/api/state"):
        return False
    hostname = parsed.hostname
    if not hostname:
        return False
    if hostname.lower() == "localhost":
        return True
    try:
        addr = ipaddress.ip_address(hostname)
        if addr.is_link_local or addr.is_multicast or addr.is_reserved:
            return False
        return True
    except ValueError:
        return False


class SimulatorHandler(SimpleHTTPRequestHandler):
    model = DeviceModel()
    device_host = "127.0.0.1"
    device_port = 80
    virtual_base_url: str | None = None

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(STATIC_ROOT), **kwargs)

    def _request_path(self) -> str:
        return urlsplit(self.path).path or "/"

    def _send_json(self, status: int, data: dict) -> None:
        body = json.dumps(data, separators=(",", ":"), allow_nan=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _state_payload(self) -> dict:
        payload = self.model.snapshot()
        host, port = self.server.server_address[:2]
        local_url = f"http://{host}:{port}"
        virtual_base_url = self.__class__.virtual_base_url or local_url
        parsed = urlsplit(virtual_base_url)
        payload["server"] = {
            "status": "running" if payload["state"] == "running" else "degraded",
            "host": parsed.hostname or host,
            "port": parsed.port or port,
            "base_url": virtual_base_url,
            "bind_url": local_url,
            "virtual_url": virtual_base_url,
            "last_request": self.model.last_request,
            "routes": ["/", "/api/state", "/api/controls"],
        }
        payload["ws"] = {"connected": False, "port": 0}
        payload["auth"] = {"authenticated": False, "method": "none"}
        return payload

    def _proxy_remote_state(self) -> None:
        params = parse_qs(urlsplit(self.path).query)
        target = params.get("url", [""])[0].strip()
        if not target:
            self._send_json(400, {"error": "Falta la query ?url=http://.../api/state"})
            return
        if not _is_safe_proxy_target(target):
            self._send_json(400, {"error": "URL remota rechazada por la política de seguridad (debe apuntar a /api/state en un host permitido)."})
            return
        try:
            opener = build_opener(_NoRedirectHandler())
            request = Request(target, headers={"User-Agent": "NodeMCU-preview/1.0"})
            with opener.open(request, timeout=3) as response:
                payload = response.read(MAX_PROXY_RESPONSE_BYTES + 1)
                if len(payload) > MAX_PROXY_RESPONSE_BYTES:
                    raise ValueError("La respuesta remota excede el límite máximo de tamaño permitido (64 KB).")
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
        if self.headers.get_content_type() != "application/json":
            self._send_json(415, {"error": "El cuerpo debe usar Content-Type application/json."})
            return
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._send_json(400, {"error": "Content-Length inválido."})
            return
        if content_length <= 0 or content_length > MAX_REQUEST_BYTES:
            self._send_json(413, {"error": f"El cuerpo debe tener entre 1 y {MAX_REQUEST_BYTES} bytes."})
            return

        try:
            body = self.rfile.read(content_length)
            if len(body) != content_length:
                self._send_json(400, {"error": "Cuerpo incompleto."})
                return
            controls = json.loads(body)
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


def _loopback_host(host: str) -> str:
    if host.lower() == "localhost":
        return "127.0.0.1"
    try:
        address = ipaddress.ip_address(host)
    except ValueError as error:
        raise ValueError("El simulador solo acepta una dirección IP loopback.") from error
    if not address.is_loopback:
        raise ValueError("El simulador solo acepta una dirección IP loopback.")
    return str(address)


def main() -> None:
    parser = argparse.ArgumentParser(description="Simula el monitor web de la NodeMCU ESP8266.")
    parser.add_argument("--host", default="127.0.0.1", help="Dirección loopback local (no se permite exponer el simulador a la red).")
    parser.add_argument("--port", type=int, default=8080, help="Puerto HTTP (por defecto: 8080).")
    parser.add_argument("--device-ip", default="127.0.0.1", help="IP virtual que la placa simula; solo se muestra en el dashboard.")
    parser.add_argument("--device-port", type=int, default=80, help="Puerto virtual que la placa simula; por defecto 80.")
    parser.add_argument("--open-url", default=None, help="URL que se abrirá en el navegador; por defecto se usa la URL local.")
    parser.add_argument("--no-open", action="store_true", help="No abrir el navegador automáticamente (útil en CI/tests).")
    args = parser.parse_args()

    try:
        bind_host = _loopback_host(args.host)
    except ValueError as error:
        parser.error(str(error))
    if not 0 <= args.port <= 65535 or not 1 <= args.device_port <= 65535:
        parser.error("Los puertos deben estar en el rango 1-65535 (HTTP también admite 0 para puerto efímero).")

    server = ThreadingHTTPServer((bind_host, args.port), SimulatorHandler)
    host, port = server.server_address[:2]
    local_url = f"http://{host}:{port}"
    SimulatorHandler.virtual_base_url = f"http://{args.device_ip}:{args.device_port}"
    browser_url = args.open_url or local_url
    print(f"Simulador disponible solo en este equipo: {local_url}")
    print(f"Vista de placa simulada: {SimulatorHandler.virtual_base_url}")

    if not args.no_open:
        import webbrowser

        if not webbrowser.open(browser_url, new=2):
            print(f"Advertencia: no se pudo abrir automáticamente {browser_url}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nCerrando simulador.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
