import json
import threading
import unittest
from http.server import ThreadingHTTPServer
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from simulator.model import ControlError, DeviceModel
from simulator.server import SimulatorHandler


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now

    def advance_ms(self, milliseconds):
        self.now += milliseconds / 1000


class DeviceModelTests(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.device = DeviceModel(clock=self.clock)

    def test_starts_running_with_node_mcu_profile(self):
        state = self.device.snapshot()
        self.assertEqual(state["state"], "running")
        self.assertEqual(state["board"], "NodeMCU ESP8266")
        self.assertEqual(state["pins"]["trigger_gpio"], 12)
        self.assertEqual(state["pins"]["echo_gpio"], 14)
        self.assertFalse(state["sensor"]["valid"])
        self.clock.advance_ms(1000)
        self.assertTrue(self.device.snapshot()["sensor"]["valid"])

    def test_echo_timeout_is_not_reported_as_zero_distance(self):
        self.device.apply_controls({"echo_available": False})
        self.clock.advance_ms(1000)
        state = self.device.snapshot()
        self.assertFalse(state["sensor"]["valid"])
        self.assertIsNone(state["sensor"]["distance_cm"])
        self.assertIn("Echo timeout", state["logs"][-1])

    def test_sample_interval_is_one_second(self):
        self.clock.advance_ms(999)
        self.assertEqual(len(self.device.snapshot()["logs"]), 1)
        self.clock.advance_ms(1)
        self.assertEqual(len(self.device.snapshot()["logs"]), 2)

    def test_filesystem_and_wifi_boot_failures_match_firmware_startup(self):
        self.device.apply_controls({"filesystem_mounts": False})
        state = self.device.snapshot()
        self.assertEqual(state["state"], "halted_filesystem")
        self.assertFalse(state["sensor"]["valid"])

        self.device.apply_controls({"filesystem_mounts": True, "wifi_available": False})
        self.assertEqual(self.device.snapshot()["state"], "waiting_wifi")

    def test_distance_range_is_validated(self):
        self.device.apply_controls({"distance_cm": 400})
        self.assertEqual(self.device.snapshot()["controls"]["distance_cm"], 400)
        with self.assertRaises(ControlError):
            self.device.apply_controls({"distance_cm": 401})

    def test_invalid_controls_do_not_partially_update_and_reboot_resets_uptime(self):
        with self.assertRaises(ControlError):
            self.device.apply_controls({"distance_cm": 12, "wifi_available": "false"})
        self.assertEqual(self.device.snapshot()["controls"]["distance_cm"], 48)

        self.clock.advance_ms(5000)
        self.device.apply_controls({"reboot": True})
        self.assertEqual(self.device.snapshot()["uptime_ms"], 0)


class SimulatorHttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.previous_model = SimulatorHandler.model
        SimulatorHandler.model = DeviceModel(clock=FakeClock())
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), SimulatorHandler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base_url = f"http://127.0.0.1:{cls.server.server_port}"

    def setUp(self):
        SimulatorHandler.model = DeviceModel(clock=FakeClock())

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.thread.join()
        cls.server.server_close()
        SimulatorHandler.model = cls.previous_model

    def test_serves_dashboard_and_state_json(self):
        with urlopen(self.base_url + "/") as response:
            self.assertEqual(response.status, 200)
            self.assertIn("Alarma técnica", response.read().decode("utf-8"))
        with urlopen(self.base_url + "/api/state") as response:
            state = json.load(response)
        self.assertTrue(state["simulator"])
        self.assertEqual(state["state"], "running")

    def test_control_api_rejects_out_of_range_sensor_values(self):
        request = Request(
            self.base_url + "/api/controls",
            data=b'{"distance_cm": 500}',
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with self.assertRaises(HTTPError) as raised:
            urlopen(request)
        try:
            self.assertEqual(raised.exception.code, 400)
            self.assertIn("distance_cm", raised.exception.read().decode("utf-8"))
        finally:
            raised.exception.close()

    def test_control_api_can_simulate_a_wifi_boot_failure(self):
        request = Request(
            self.base_url + "/api/controls",
            data=b'{"wifi_available": false}',
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urlopen(request) as response:
            state = json.load(response)
        self.assertEqual(state["state"], "waiting_wifi")
        self.assertFalse(state["wifi"])


if __name__ == "__main__":
    unittest.main()
