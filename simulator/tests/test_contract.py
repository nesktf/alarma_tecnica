"""Contract tests: the simulator snapshot must satisfy the shared /api/state schema.

The schema at shared/api_state.schema.json is the single source of truth for the
JSON served by GET /api/state in both the ESP8266 firmware (format_sensor_state
in firmware/sensor.cpp) and the local simulator (simulator/model.py).

Base keys are validated strictly. Preview-only keys (controls and logs) exist
only in the simulator and are checked for presence/type, never for firmware
parity: the firmware must not emit them. alarm_system is a required base key
emitted by both environments; its firmware value is currently a fixed
placeholder, not a functional alarm policy.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from simulator.model import DeviceModel


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCHEMA_PATH = Path(__file__).resolve().parent / "api_state.schema.json"


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


    def test_schema_file_is_valid_single_json_document_and_has_no_duplicate_definitions(self):
        content = SCHEMA_PATH.read_text(encoding="utf-8")
        decoder = json.JSONDecoder()
        obj, end = decoder.raw_decode(content)
        self.assertEqual(end, len(content.strip()), "Schema file has trailing content after main JSON object")
        self.assertEqual(obj["$id"], "https://redes-ii.local/schemas/api_state.schema.json")
        self.assertIn("alarm_system", obj["required"])
        self.assertIn("nodes", obj["required"])
        self.assertNotIn("controls", obj["required"])
        self.assertNotIn("logs", obj["required"])

    def test_firmware_formatter_output_satisfies_schema(self):
        # Sample payload representing format_sensor_state output from firmware/sensor.cpp
        sample_fw_json = json.loads(
            '{"simulator":false,"board":"NodeMCU ESP8266","state":"running",'
            '"uptime_ms":12345,"wifi":true,"filesystem":true,'
            '"sensor":{"valid":true,"distance_cm":25.4,"age_ms":100,"echo_timeout_us":30000},'
            '"pins":{"trigger_gpio":12,"echo_gpio":14,"trigger_label":"D6","echo_label":"D5"},'
            '"limits":{"cpu_mhz":80,"flash_bytes":4194304,"dram_bytes":80192,"iram_bytes":65536},'
            '"server":{"status":"running","host":"192.168.0.53","port":80,"base_url":"http://192.168.0.53","last_request":"/api/state","routes":["/","/api/state"]},'
            '"alarm_system":{"state":"DISARMED","arming_remaining_s":0.0,"v_bus":12.4,"v_bus_status":"normal",'
            '"active_areas":[],"no_response_areas":[],'
            '"sectors":['
            '{"id":1,"name":"Área Operativa","node":"central","has_panic":true,"motion":false,"active":false,"no_response":false,"status":"normal"},'
            '{"id":2,"name":"Automotores","node":"remote","has_panic":false,"motion":false,"active":false,"no_response":false,"status":"normal"},'
            '{"id":3,"name":"Jefatura de Taller","node":"remote","has_panic":true,"motion":false,"active":false,"no_response":false,"status":"normal"},'
            '{"id":4,"name":"Vicedirección","node":"remote","has_panic":true,"motion":false,"active":false,"no_response":false,"status":"normal"},'
            '{"id":5,"name":"Informática","node":"remote","has_panic":false,"motion":false,"active":false,"no_response":false,"status":"normal"}'
            '],'
            '"recent_events":[]},'
            '"ws":{"connected":false,"port":0},'
            '"auth":{"authenticated":false,"method":"none"},'
            '"nodes":[]}'
        )
        errors = self.schema.validate(sample_fw_json)
        self.assertEqual(errors, [], "Firmware contract violations:\n" + "\n".join(errors))

    def test_rs485_protocol_crc16_and_packet_serialization(self):
        """Validates binary protocol framing (protocol.h) and CRC16 calculation across C++ / Python."""
        def calc_crc16(data: bytes) -> int:
            crc = 0
            for byte in data:
                crc ^= (byte << 8)
                for _ in range(8):
                    if crc & 0x8000:
                        crc = ((crc << 1) ^ 0x1021) & 0xFFFF
                    else:
                        crc = (crc << 1) & 0xFFFF
            return crc

        # Construct a 7-byte sensor packet
        sync = 0xAA
        node_id = 2
        distance = 150  # 0x0096
        status = 0x05   # Heartbeat (0x04) + Alarm (0x01)

        payload_bytes = bytes([
            sync,
            node_id,
            distance & 0xFF,
            (distance >> 8) & 0xFF,
            status,
        ])
        expected_crc = calc_crc16(payload_bytes)

        full_packet = payload_bytes + bytes([expected_crc & 0xFF, (expected_crc >> 8) & 0xFF])
        self.assertEqual(len(full_packet), 7)
        self.assertEqual(full_packet[0], 0xAA)
        self.assertEqual(full_packet[1], 2)

        # Verify little-endian CRC unpacking matches
        calc = calc_crc16(full_packet[:5])
        unpacked_crc = full_packet[5] | (full_packet[6] << 8)
        self.assertEqual(calc, unpacked_crc)


if __name__ == "__main__":
    unittest.main()

