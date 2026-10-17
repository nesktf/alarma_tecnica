"""Serve the project dashboard with a controllable local NodeMCU model."""

from __future__ import annotations

import argparse
import ipaddress
import json
import hashlib
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
    active_tokens: set[str] = set()

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(STATIC_ROOT), **kwargs)

    def _request_path(self) -> str:
        return urlsplit(self.path).path or "/"

    def _is_authenticated(self) -> bool:
        token = self.headers.get("X-Auth-Token", "").strip()
        if token and token in self.__class__.active_tokens:
            return True
        cookie_header = self.headers.get("Cookie", "")
        for part in cookie_header.split(";"):
            part = part.strip()
            if part.startswith("token="):
                cookie_token = part.split("=", 1)[1].strip()
                if cookie_token and cookie_token in self.__class__.active_tokens:
                    return True
        return False

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
        auth = self._is_authenticated()
        payload["authenticated"] = auth
        payload["auth"] = {"authenticated": auth, "method": "token" if auth else "none"}
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
            if not self._is_authenticated():
                self._send_json(401, {"error": "Unauthorized"})
                return
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

        content_length = int(self.headers.get("Content-Length", "0"))
        body_bytes = self.rfile.read(content_length) if content_length > 0 else b""
        content_type = self.headers.get_content_type()

        payload = {}
        if content_type == "application/json" and body_bytes:
            try:
                payload = json.loads(body_bytes.decode("utf-8"))
            except Exception as e:
                self._send_json(400, {"error": f"JSON inválido: {e}"})
                return
        elif content_type == "application/x-www-form-urlencoded" and body_bytes:
            parsed = parse_qs(body_bytes.decode("utf-8"))
            payload = {k: v[0] for k, v in parsed.items()}

        if path == "/api/login":
            pwd = payload.get("password", "")
            valid_passwords = {
                "1234",
                "admin123",
                hashlib.sha256(b"1234").hexdigest(),
                hashlib.sha256(b"admin123").hexdigest(),
            }
            if pwd in valid_passwords:
                token = "simulated_token_12345"
                self.__class__.active_tokens.add(token)
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Set-Cookie", f"token={token}; Path=/; HttpOnly")
                resp = json.dumps({"authenticated": True, "token": token}).encode("utf-8")
                self.send_header("Content-Length", str(len(resp)))
                self.end_headers()
                self.wfile.write(resp)
            else:
                self._send_json(401, {"error": "Credenciales inválidas."})
            return

        if path == "/api/logout":
            self.__class__.active_tokens.clear()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Set-Cookie", "token=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT")
            resp = json.dumps({"authenticated": False}).encode("utf-8")
            self.send_header("Content-Length", str(len(resp)))
            self.end_headers()
            self.wfile.write(resp)
            return

        if path == "/api/system/arm":
            if not self._is_authenticated():
                self._send_json(401, {"error": "Unauthorized"})
                return
            arm = str(payload.get("arm", "false")).lower() in ("true", "1")
            self.model.apply_controls({"set_alarm_state": "ARMING" if arm else "DISARMED"})
            self._send_json(200, {"status": "ok"})
            return

        if path == "/api/sector/toggle":
            if not self._is_authenticated():
                self._send_json(401, {"error": "Unauthorized"})
                return
            sec_id = int(payload.get("sector_id", 0))
            enabled = str(payload.get("enabled", "true")).lower() in ("true", "1")
            if 1 <= sec_id <= 5:
                sector = self.model._sectors.get(sec_id)
                if sector:
                    sector["enabled"] = enabled
            self._send_json(200, {"status": "ok"})
            return

        if path == "/api/sector/ping":
            if not self._is_authenticated():
                self._send_json(401, {"error": "Unauthorized"})
                return
            self._send_json(200, {"status": "ok"})
            return

        if path != "/api/controls":
            self._send_json(404, {"error": "Ruta de simulación inexistente."})
            return

        if content_type != "application/json":
            self._send_json(415, {"error": "El cuerpo debe usar Content-Type application/json."})
            return

        if content_length <= 0 or content_length > MAX_REQUEST_BYTES:
            self._send_json(413, {"error": f"El cuerpo debe tener entre 1 y {MAX_REQUEST_BYTES} bytes."})
            return

        try:
            self.model.apply_controls(payload)
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
