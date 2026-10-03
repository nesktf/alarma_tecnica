"""Contract tests: the simulator snapshot must satisfy the shared /api/state schema.

The schema at shared/api_state.schema.json is the single source of truth for the
JSON served by GET /api/state in both the ESP8266 firmware (format_sensor_state
in firmware/src/sensor.cpp) and the local simulator (simulator/model.py).

Base keys are validated strictly. Preview-only keys (controls, alarm_system,
logs) exist only in the simulator and are checked for presence/type, never for
firmware parity: the firmware must not emit them.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from simulator.model import DeviceModel


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCHEMA_PATH = PROJECT_ROOT / "shared" / "api_state.schema.json"


class _Schema:
    """Minimal draft-07 subset validator: type, required, const, enum,
    minimum, properties, items, additionalProperties, ref (local #/...)."""

    def __init__(self, schema: dict) -> None:
        self._schema = schema

    def validate(self, instance, schema: dict | None = None, path: str = "$") -> list[str]:
        schema = schema if schema is not None else self._schema
        errors: list[str] = []

        ref = schema.get("$ref")
        if ref and ref.startswith("#/"):
            target = self._schema
            for part in ref[2:].split("/"):
                target = target[part]
            return self.validate(instance, target, path)

        expected_type = schema.get("type")
        if expected_type is not None and not self._check_type(instance, expected_type):
            errors.append(f"{path}: expected type {expected_type}, got {type(instance).__name__}")
            return errors

        if "const" in schema and instance != schema["const"]:
            errors.append(f"{path}: expected const {schema['const']!r}, got {instance!r}")
        if "enum" in schema and instance not in schema["enum"]:
            errors.append(f"{path}: {instance!r} not in enum {schema['enum']}")
        if isinstance(instance, (int, float)) and not isinstance(instance, bool):
            if "minimum" in schema and instance < schema["minimum"]:
                errors.append(f"{path}: {instance} < minimum {schema['minimum']}")

        if isinstance(instance, dict):
            for key in schema.get("required", []):
                if key not in instance:
                    errors.append(f"{path}: missing required key '{key}'")
            properties = schema.get("properties", {})
            if schema.get("additionalProperties") is False:
                for key in instance:
                    if key not in properties:
                        errors.append(f"{path}: unexpected key '{key}'")
            for key, sub_schema in properties.items():
                if key in instance:
                    errors.extend(self.validate(instance[key], sub_schema, f"{path}.{key}"))

        if isinstance(instance, list) and "items" in schema:
            for index, item in enumerate(instance):
                errors.extend(self.validate(item, schema["items"], f"{path}[{index}]"))

        return errors

    @staticmethod
    def _check_type(instance, expected: str | list) -> bool:
        types = expected if isinstance(expected, list) else [expected]
        for candidate in types:
            if candidate == "object" and isinstance(instance, dict):
                return True
            if candidate == "array" and isinstance(instance, list):
                return True
            if candidate == "string" and isinstance(instance, str):
                return True
            if candidate == "integer" and isinstance(instance, int) and not isinstance(instance, bool):
                return True
            if candidate == "number" and isinstance(instance, (int, float)) and not isinstance(instance, bool):
                return True
            if candidate == "boolean" and isinstance(instance, bool):
                return True
            if candidate == "null" and instance is None:
                return True
        return False


class ApiContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.schema = _Schema(json.loads(SCHEMA_PATH.read_text(encoding="utf-8")))

    def setUp(self) -> None:
        self.device = DeviceModel()
        self.device._sample_if_due(self.device._clock())

    def test_simulator_snapshot_satisfies_base_contract(self):
        snapshot = self.device.snapshot()
        errors = self.schema.validate(snapshot)
        self.assertEqual(errors, [], "Contract violations:\n" + "\n".join(errors))

    def test_base_contract_keys_are_present_and_typed(self):
        snapshot = self.device.snapshot()
        self.assertIn("simulator", snapshot)
        self.assertEqual(snapshot["board"], "NodeMCU ESP8266")
        self.assertIn(snapshot["state"], {"running", "waiting_wifi", "halted_filesystem"})
        self.assertIsInstance(snapshot["uptime_ms"], int)
        self.assertIsInstance(snapshot["wifi"], bool)
        self.assertIsInstance(snapshot["filesystem"], bool)
        for key in ("valid", "distance_cm", "age_ms", "echo_timeout_us"):
            self.assertIn(key, snapshot["sensor"])
        for key in ("trigger_gpio", "echo_gpio", "trigger_label", "echo_label"):
            self.assertIn(key, snapshot["pins"])
        for key in ("cpu_mhz", "flash_bytes", "dram_bytes", "iram_bytes"):
            self.assertIn(key, snapshot["limits"])
        for key in ("status", "host", "port", "base_url", "last_request", "routes"):
            self.assertIn(key, snapshot["server"])

    def test_preview_only_keys_are_simulator_specific(self):
        schema = self.schema._schema
        preview_keys = [
            key for key, value in schema["properties"].items()
            if isinstance(value, dict) and value.get("preview_only")
        ]
        self.assertEqual(preview_keys, ["controls", "logs"])
        snapshot = self.device.snapshot()
        for key in preview_keys:
            self.assertIn(key, snapshot, f"Preview key '{key}' missing from simulator snapshot")

    def test_distinguishes_valid_measurement_from_timeout(self):
        self.device._echo_available = False
        self.device._sample_if_due(self.device._clock())
        snapshot = self.device.snapshot()
        self.assertFalse(snapshot["sensor"]["valid"])
        self.assertIsNone(snapshot["sensor"]["distance_cm"])

    def test_boot_failure_states_are_in_the_contract(self):
        self.device._filesystem_mounts = False
        snapshot = self.device.snapshot()
        self.assertEqual(snapshot["state"], "halted_filesystem")
        self.device._filesystem_mounts = True
        self.device._wifi_available = False
        self.assertEqual(self.device.snapshot()["state"], "waiting_wifi")


if __name__ == "__main__":
    unittest.main()
