const byId = (id) => document.getElementById(id);

const elements = {
  badge: byId("connectionBadge"),
  boot: byId("bootState"),
  distance: byId("distanceValue"),
  meter: byId("distanceMeter"),
  meterContainer: document.querySelector(".meter"),
  sampleAge: byId("sampleAge"),
  sensorMessage: byId("sensorMessage"),
  wifi: byId("wifiState"),
  filesystem: byId("filesystemState"),
  firmware: byId("firmwareState"),
  led: byId("ledIndicator"),
  controls: byId("simulatorControls"),
  notice: byId("simulationNotice"),
  eventPanel: byId("eventPanel"),
  eventLog: byId("eventLog"),
  controlError: byId("controlError"),
};

let stateRequestPending = false;
let controlRequestPending = false;

function setStatus(element, text, good) {
  element.textContent = text;
  element.classList.toggle("good", good);
  element.classList.toggle("bad", !good);
}

function renderState(state) {
  const bootLabels = {
    running: ["En funcionamiento", true],
    waiting_wifi: ["Esperando Wi-Fi", false],
    halted_filesystem: ["Detenido: LittleFS", false],
  };
  const [bootText, bootGood] = bootLabels[state.state] || ["Estado desconocido", false];
  elements.boot.textContent = bootText;
  elements.boot.className = `state-chip ${bootGood ? "good" : "bad"}`;
  elements.badge.textContent = state.simulator ? "Simulador local" : "ESP8266 conectado";
  elements.badge.className = `badge ${state.state === "running" ? "good" : "bad"}`;
  elements.notice.hidden = !state.simulator;
  elements.controls.hidden = !state.simulator;
  elements.eventPanel.hidden = !state.simulator;
  elements.firmware.textContent = state.simulator ? "Modelo local" : "ESP8266";
  elements.firmware.className = state.simulator ? "sim-value" : "good";
  const uptimeSeconds = Math.floor(state.uptime_ms / 1000);
  byId("uptime").textContent = `${Math.floor(uptimeSeconds / 60)} min ${uptimeSeconds % 60} s`;
  setStatus(elements.wifi, state.wifi ? "Conectado" : "Sin conexión", state.wifi);
  setStatus(elements.filesystem, state.filesystem ? "Montado" : "Error de montaje", state.filesystem);
  elements.led.classList.toggle("led-on", state.state === "running");

  const sensor = state.sensor;
  if (sensor.valid && Number.isFinite(sensor.distance_cm)) {
    const distance = sensor.distance_cm;
    elements.distance.textContent = distance.toFixed(1);
    elements.sampleAge.textContent = sensor.age_ms === null ? "Muestra disponible" : `hace ${sensor.age_ms} ms`;
    elements.sensorMessage.textContent = "Eco recibido correctamente.";
    elements.meter.style.width = `${Math.min(100, Math.max(0, distance / 4))}%`;
    elements.meterContainer.setAttribute("aria-valuenow", String(Math.round(distance)));
  } else {
    elements.distance.textContent = "—";
    elements.sampleAge.textContent = state.state === "running" ? "Sin eco" : "Sin lectura";
    elements.sensorMessage.textContent = state.state === "waiting_wifi"
      ? "El arranque espera conectividad Wi-Fi."
      : state.state === "halted_filesystem"
        ? "El firmware se detuvo al montar LittleFS."
        : sensor.age_ms === null
          ? "Esperando primera medición."
        : "No se recibió pulso Echo dentro del timeout.";
    elements.meter.style.width = "0%";
    elements.meterContainer.setAttribute("aria-valuenow", "0");
  }

  const pins = state.pins;
  byId("triggerGpio").textContent = `${pins.trigger_label} / GPIO${pins.trigger_gpio}`;
  byId("echoGpio").textContent = `${pins.echo_label} / GPIO${pins.echo_gpio}`;
  byId("triggerPin").textContent = "Trig";
  byId("echoPin").textContent = "Echo";
  byId("cpuLimit").textContent = `${state.limits.cpu_mhz} MHz`;
  byId("flashLimit").textContent = `${state.limits.flash_bytes / (1024 * 1024)} MB`;

  if (state.simulator) {
    const active = document.activeElement;
    if (active !== byId("distanceControl")) {
      byId("distanceControl").value = String(state.controls.distance_cm);
    }
    byId("distanceOutput").textContent = `${Number(state.controls.distance_cm).toFixed(0)} cm`;
    byId("echoControl").checked = state.controls.echo_available;
    byId("wifiControl").checked = state.controls.wifi_available;
    byId("filesystemControl").checked = state.controls.filesystem_mounts;
    elements.eventLog.replaceChildren(...state.logs.map((entry) => {
      const item = document.createElement("li");
      item.textContent = entry;
      return item;
    }));
  }
}

async function loadState() {
  if (stateRequestPending) return;
  stateRequestPending = true;
  try {
    const response = await fetch("/api/state", {cache: "no-store"});
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    renderState(await response.json());
  } catch (error) {
    elements.badge.textContent = "Dispositivo no disponible";
    elements.badge.className = "badge bad";
    elements.boot.textContent = "Sin respuesta de /api/state";
    elements.boot.className = "state-chip bad";
  } finally {
    stateRequestPending = false;
  }
}

async function updateControls(changes) {
  if (controlRequestPending) return;
  controlRequestPending = true;
  elements.controlError.hidden = true;
  try {
    const response = await fetch("/api/controls", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify(changes),
    });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || `HTTP ${response.status}`);
    renderState(result);
  } catch (error) {
    elements.controlError.textContent = error.message;
    elements.controlError.hidden = false;
  } finally {
    controlRequestPending = false;
  }
}

byId("distanceControl").addEventListener("input", (event) => {
  byId("distanceOutput").textContent = `${event.target.value} cm`;
});
byId("distanceControl").addEventListener("change", (event) => {
  updateControls({distance_cm: Number(event.target.value)});
});
byId("echoControl").addEventListener("change", (event) => {
  updateControls({echo_available: event.target.checked});
});
byId("wifiControl").addEventListener("change", (event) => {
  updateControls({wifi_available: event.target.checked});
});
byId("filesystemControl").addEventListener("change", (event) => {
  updateControls({filesystem_mounts: event.target.checked});
});
byId("rebootButton").addEventListener("click", () => updateControls({reboot: true}));

loadState();
window.setInterval(loadState, 1000);
