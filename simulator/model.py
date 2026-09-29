"""Deterministic, hardware-independent state model for the local simulator."""

from __future__ import annotations

import math
import threading
import time
from collections import deque
from datetime import datetime
from typing import Callable


class ControlError(ValueError):
    """Raised when a simulator control value is invalid."""


class DeviceModel:
    SAMPLE_INTERVAL_MS = 1000
    ECHO_TIMEOUT_US = 30000
    SENSOR_MIN_CM = 2.0
    SENSOR_MAX_CM = 400.0

    def __init__(self, clock: Callable[[], float] = time.monotonic) -> None:
        self._clock = clock
        self._started_at = clock()
        self._lock = threading.RLock()
        self._last_sample_at: float | None = self._started_at
        self._wifi_available = True
        self._filesystem_mounts = True
        self._echo_available = True
        self._distance_cm = 48.0
        self._last_distance_cm: float | None = None
        self._logs: deque[str] = deque(maxlen=30)
        self._log("Simulator started; NodeMCU boot sequence complete")

    def _log(self, message: str) -> None:
        timestamp = datetime.now().astimezone().strftime("%H:%M:%S")
        self._logs.append(f"{timestamp}  {message}")

    @property
    def _boot_state(self) -> str:
        if not self._filesystem_mounts:
            return "halted_filesystem"
        if not self._wifi_available:
            return "waiting_wifi"
        return "running"

    def _sample_if_due(self, now: float) -> None:
        if self._boot_state != "running":
            return
        elapsed_ms = 0 if self._last_sample_at is None else (now - self._last_sample_at) * 1000
        if self._last_sample_at is None or elapsed_ms >= self.SAMPLE_INTERVAL_MS - 1e-9:
            self._last_sample_at = now
            if self._echo_available:
                self._last_distance_cm = self._distance_cm
                self._log(f"Distance: {self._distance_cm:.1f} cm")
            else:
                self._last_distance_cm = None
                self._log(f"Echo timeout ({self.ECHO_TIMEOUT_US} us)")

    def snapshot(self) -> dict:
        with self._lock:
            return self._snapshot()

    def _snapshot(self) -> dict:
        now = self._clock()
        self._sample_if_due(now)
        state = self._boot_state
        age_ms = None
        if state == "running" and self._last_sample_at is not None:
            age_ms = max(0, int((now - self._last_sample_at) * 1000))

        return {
            "simulator": True,
            "board": "NodeMCU ESP8266",
            "state": state,
            "uptime_ms": max(0, int((now - self._started_at) * 1000)),
            "wifi": self._wifi_available,
            "filesystem": self._filesystem_mounts,
            "sensor": {
                "valid": state == "running" and self._last_distance_cm is not None,
                "distance_cm": self._last_distance_cm if state == "running" else None,
                "age_ms": age_ms,
                "echo_timeout_us": self.ECHO_TIMEOUT_US,
            },
            "pins": {"trigger_gpio": 12, "echo_gpio": 14, "trigger_label": "D6", "echo_label": "D5"},
            "limits": {
                "cpu_mhz": 80,
                "flash_bytes": 4 * 1024 * 1024,
                "dram_bytes": 80192,
                "iram_bytes": 65536,
            },
            "controls": {
                "distance_cm": self._distance_cm,
                "echo_available": self._echo_available,
                "wifi_available": self._wifi_available,
                "filesystem_mounts": self._filesystem_mounts,
                "sensor_min_cm": self.SENSOR_MIN_CM,
                "sensor_max_cm": self.SENSOR_MAX_CM,
            },
            "logs": list(self._logs),
        }

    def apply_controls(self, controls: dict) -> None:
        with self._lock:
            self._apply_controls(controls)

    def _apply_controls(self, controls: dict) -> None:
        if not isinstance(controls, dict):
            raise ControlError("El cuerpo debe ser un objeto JSON.")
        allowed = {"distance_cm", "echo_available", "wifi_available", "filesystem_mounts", "reboot"}
        unknown = set(controls) - allowed
        if unknown:
            raise ControlError(f"Control desconocido: {', '.join(sorted(unknown))}.")

        distance = self._distance_cm
        if "distance_cm" in controls:
            requested_distance = controls["distance_cm"]
            if isinstance(requested_distance, bool) or not isinstance(requested_distance, (int, float)) or not math.isfinite(requested_distance):
                raise ControlError("distance_cm debe ser un número finito.")
            if not self.SENSOR_MIN_CM <= requested_distance <= self.SENSOR_MAX_CM:
                raise ControlError(
                    f"distance_cm debe estar entre {self.SENSOR_MIN_CM:g} y {self.SENSOR_MAX_CM:g} cm."
                )
            distance = float(requested_distance)

        values = {
            "echo_available": self._echo_available,
            "wifi_available": self._wifi_available,
            "filesystem_mounts": self._filesystem_mounts,
        }
        for key in ("echo_available", "wifi_available", "filesystem_mounts"):
            if key in controls:
                value = controls[key]
                if type(value) is not bool:
                    raise ControlError(f"{key} debe ser true o false.")
                values[key] = value

        reboot = controls.get("reboot", False)
        if type(reboot) is not bool:
            raise ControlError("reboot debe ser true o false.")

        self._distance_cm = distance
        self._echo_available = values["echo_available"]
        self._wifi_available = values["wifi_available"]
        self._filesystem_mounts = values["filesystem_mounts"]
        if controls.get("reboot") is True:
            self._started_at = self._clock()
            self._last_sample_at = self._started_at
            self._last_distance_cm = None
            self._log("Simulated reboot requested")

        self._log(f"Controls updated; boot state: {self._boot_state}")
