import json
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.error import HTTPError
from urllib.parse import quote
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

    def test_simulates_alarm_states_and_sector_controls(self):
        self.device.apply_controls({"alarm_state": "ARMED"})
        self.device.apply_controls({"toggle_motion": 2})
        state = self.device.snapshot()["alarm_system"]
        self.assertEqual(state["state"], "ALARM")
        self.assertEqual(state["sectors"][1]["status"], "alarm")

        self.device.apply_controls({"alarm_state": "DISARMED"})
        self.device.apply_controls({"toggle_motion": 2})
        self.device.apply_controls({"toggle_no_response": 2})
        sector = self.device.snapshot()["alarm_system"]["sectors"][1]
        self.assertEqual(sector["status"], "no_response")
        self.assertTrue(sector["no_response"])

    def test_alarm_arming_completes_after_sixty_seconds(self):
        self.device.apply_controls({"alarm_state": "ARMING"})
        self.assertEqual(self.device.snapshot()["alarm_system"]["state"], "ARMING")
        self.clock.advance_ms(59999)
        self.assertEqual(self.device.snapshot()["alarm_system"]["state"], "ARMING")
        self.clock.advance_ms(1)
        alarm = self.device.snapshot()["alarm_system"]
        self.assertEqual(alarm["state"], "ARMED")
        self.assertEqual(alarm["arming_remaining_s"], 0)

    def test_alarm_arming_triggers_if_motion_is_active_at_completion(self):
        self.device.apply_controls({"toggle_motion": 3, "alarm_state": "ARMING"})
        self.clock.advance_ms(60000)
        alarm = self.device.snapshot()["alarm_system"]
        self.assertEqual(alarm["state"], "ALARM")
        self.assertEqual(alarm["sectors"][2]["status"], "alarm")

    def test_emergency_and_bus_voltage_controls_are_reflected(self):
        self.device.apply_controls({"trigger_emergency": 1, "v_bus": 11.5})
        alarm = self.device.snapshot()["alarm_system"]
        self.assertEqual(alarm["state"], "ALARM")
        self.assertEqual(alarm["sectors"][0]["status"], "alarm")
        self.assertEqual(alarm["v_bus_status"], "warning")

        self.device.apply_controls({"v_bus": 10.5})
        self.assertEqual(self.device.snapshot()["alarm_system"]["v_bus_status"], "alarm")

    def test_reset_all_restores_hardware_and_alarm_controls(self):
        self.device.apply_controls({
            "distance_cm": 22,
            "wifi_available": False,
            "alarm_state": "ALARM",
            "v_bus": 10.5,
            "toggle_motion": 4,
        })
        self.device.apply_controls({"reset_all": True})
        state = self.device.snapshot()
        self.assertEqual(state["state"], "running")
        self.assertEqual(state["uptime_ms"], 0)
        self.assertEqual(state["controls"]["distance_cm"], 48)
        self.assertEqual(state["alarm_system"]["state"], "DISARMED")
        self.assertEqual(state["alarm_system"]["v_bus"], 12.4)
        self.assertFalse(state["alarm_system"]["active_areas"])

    def test_invalid_alarm_control_does_not_partially_update(self):
        with self.assertRaises(ControlError):
            self.device.apply_controls({"v_bus": 10.5, "toggle_motion": 9})
        state = self.device.snapshot()["alarm_system"]
        self.assertEqual(state["v_bus"], 12.4)
        self.assertFalse(state["active_areas"])

    def test_invalid_alarm_types_and_unavailable_sector_actions_are_rejected(self):
        invalid_controls = (
            {"alarm_state": []},
            {"alarm_state": None},
            {"v_bus": 31},
            {"trigger_emergency": 2},
            {"toggle_no_response": 1},
        )
        for controls in invalid_controls:
            with self.subTest(controls=controls), self.assertRaises(ControlError):
                self.device.apply_controls(controls)


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

    def test_control_api_accepts_alarm_and_bus_controls(self):
        request = Request(
            self.base_url + "/api/controls",
            data=b'{"alarm_state":"ARMED"}',
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urlopen(request) as response:
            state = json.load(response)
        self.assertEqual(state["alarm_system"]["state"], "ARMED")

        request = Request(
            self.base_url + "/api/controls",
            data=b'{"toggle_motion":2}',
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urlopen(request) as response:
            state = json.load(response)
        self.assertEqual(state["alarm_system"]["state"], "ALARM")
        self.assertEqual(state["alarm_system"]["sectors"][1]["status"], "alarm")

    def test_server_activity_is_visible_in_state_payload(self):
       with urlopen(self.base_url + "/api/state") as response:
           state = json.load(response)
       self.assertEqual(state["server"]["status"], "running")
       self.assertEqual(state["server"]["last_request"], "/api/state")
       self.assertIn("/api/state", state["server"]["routes"])
       self.assertIn("/api/controls", state["server"]["routes"])
       self.assertIn("bind_url", state["server"])
       self.assertIn("virtual_url", state["server"])

    def test_proxy_can_forward_a_remote_state_endpoint(self):
       class PreviewHandler(BaseHTTPRequestHandler):
           def do_GET(self):
               payload = json.dumps({"simulator": False, "state": "running", "wifi": True}).encode("utf-8")
               self.send_response(200)
               self.send_header("Content-Type", "application/json; charset=utf-8")
               self.send_header("Content-Length", str(len(payload)))
               self.end_headers()
               self.wfile.write(payload)

           def log_message(self, *args, **kwargs):
               return

       remote_server = ThreadingHTTPServer(("127.0.0.1", 0), PreviewHandler)
       thread = threading.Thread(target=remote_server.serve_forever, daemon=True)
       thread.start()
       try:
           target = f"http://127.0.0.1:{remote_server.server_port}/api/state"
           with urlopen(self.base_url + "/api/proxy?url=" + quote(target)) as response:
               state = json.load(response)
           self.assertEqual(state["state"], "running")
           self.assertFalse(state["simulator"])
       finally:
           remote_server.shutdown()
           thread.join()
           remote_server.server_close()


if __name__ == "__main__":
    unittest.main()
