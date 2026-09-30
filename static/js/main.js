/**
 * Sistema de Alarma Distribuido - Cliente Web
 * Frontend unificado para NodeMCU ESP8266 y Simulador Local
 */

"use strict";

const byId = (id) => document.getElementById(id);

// ==========================================================================
// Metadatos de sectores del sistema
// ==========================================================================

const SECTORS_META = {
  1: { id: 1, name: "Área Operativa", node: "Arduino Nano (central)", hasPir: true, hasPanic: true, projected: false },
  2: { id: 2, name: "Automotores", node: "RS485 nodo 0x02", hasPir: true, hasPanic: false, projected: false },
  3: { id: 3, name: "Jefatura de Taller", node: "RS485 nodo 0x03", hasPir: true, hasPanic: true, projected: false },
  4: { id: 4, name: "Vicedirección", node: "RS485 nodo 0x04", hasPir: true, hasPanic: true, projected: false },
  5: { id: 5, name: "Informática", node: "RS485 nodo 0x05", hasPir: true, hasPanic: false, projected: false },
  6: { id: 6, name: "Biblioteca", node: "RS485 nodo 0x06", hasPir: true, hasPanic: false, projected: true },
  7: { id: 7, name: "Preceptoría 1", node: "RS485 nodo 0x07", hasPir: true, hasPanic: false, projected: true },
  8: { id: 8, name: "Preceptoría 2", node: "RS485 nodo 0x08", hasPir: true, hasPanic: false, projected: true },
  9: { id: 9, name: "Preceptoría 3", node: "RS485 nodo 0x09", hasPir: true, hasPanic: false, projected: true },
};

// ==========================================================================
// Sintetizador de audio Web Audio API para alertas sonoras
// ==========================================================================

class AlarmAudioSynthesizer {
  constructor() {
    this.ctx = null;
    this.enabled = false;
    this.sirenOsc1 = null;
    this.sirenOsc2 = null;
    this.sirenGain = null;
    this.sirenLfo = null;
    this.sirenLfoGain = null;
    this.isSirenPlaying = false;
    this.lastBeepTime = 0;
  }

  initContext() {
    if (!this.ctx) {
      const AudioCtx = window.AudioContext || window.webkitAudioContext;
      if (AudioCtx) {
        this.ctx = new AudioCtx();
      }
    }
    if (this.ctx && this.ctx.state === "suspended") {
      this.ctx.resume();
    }
  }

  toggle() {
    this.initContext();
    this.enabled = !this.enabled;
    if (!this.enabled) {
      this.stopSiren();
    }
    return this.enabled;
  }

  playSiren() {
    if (!this.enabled || !this.ctx || this.isSirenPlaying) return;
    try {
      this.isSirenPlaying = true;
      const now = this.ctx.currentTime;

      // Oscilador modulador LFO (frecuencia de barrido)
      this.sirenLfo = this.ctx.createOscillator();
      this.sirenLfo.frequency.setValueAtTime(1.5, now); // 1.5 Hz ciclo

      this.sirenLfoGain = this.ctx.createGain();
      this.sirenLfoGain.gain.setValueAtTime(250, now); // amplitud de barrido

      // Oscilador portador principal (sirena aguda)
      this.sirenOsc1 = this.ctx.createOscillator();
      this.sirenOsc1.type = "sawtooth";
      this.sirenOsc1.frequency.setValueAtTime(750, now);

      this.sirenLfo.connect(this.sirenLfoGain);
      this.sirenLfoGain.connect(this.sirenOsc1.frequency);

      this.sirenGain = this.ctx.createGain();
      this.sirenGain.gain.setValueAtTime(0.18, now);

      this.sirenOsc1.connect(this.sirenGain);
      this.sirenGain.connect(this.ctx.destination);

      this.sirenLfo.start(now);
      this.sirenOsc1.start(now);
    } catch (e) {
      console.warn("Audio warning:", e);
    }
  }

  stopSiren() {
    if (!this.isSirenPlaying) return;
    try {
      if (this.sirenOsc1) {
        this.sirenOsc1.stop();
        this.sirenOsc1.disconnect();
      }
      if (this.sirenLfo) {
        this.sirenLfo.stop();
        this.sirenLfo.disconnect();
      }
      this.isSirenPlaying = false;
    } catch (e) {
      // Ignorar si ya estaba desconectado
    }
  }

  playArmingBeep(remainingSeconds) {
    if (!this.enabled || !this.ctx) return;
    const nowMs = Date.now();
    const interval = remainingSeconds <= 10 ? 400 : 1000;
    if (nowMs - this.lastBeepTime < interval) return;
    this.lastBeepTime = nowMs;

    try {
      const now = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const gain = this.ctx.createGain();
      osc.type = "sine";
      osc.frequency.setValueAtTime(880, now);

      gain.gain.setValueAtTime(0.15, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.12);

      osc.connect(gain);
      gain.connect(this.ctx.destination);

      osc.start(now);
      osc.stop(now + 0.12);
    } catch (e) {
      // Ignorar fallos de audio
    }
  }
}

const audio = new AlarmAudioSynthesizer();

// ==========================================================================
// Referencias a elementos del DOM
// ==========================================================================

const el = {
  // Topbar y telemetría
  centralTrafficLight: byId("centralTrafficLight"),
  trafficLightState: byId("trafficLightState"),
  armingTimer: byId("armingTimer"),
  connectionBadge: byId("connectionBadge"),
  vBusValue: byId("vBusValue"),
  vBusStatusBadge: byId("vBusStatusBadge"),
  activeSectorsCount: byId("activeSectorsCount"),
  rs485Status: byId("rs485Status"),
  soundToggleBtn: byId("soundToggleBtn"),
  soundIcon: byId("soundIcon"),
  soundText: byId("soundText"),
  simulationNotice: byId("simulationNotice"),

  // Navegación
  tabButtons: document.querySelectorAll(".tab-btn"),
  tabPanes: document.querySelectorAll(".tab-pane"),

  // Tab 1: Mapa e Inspector
  toggleBlueprintImg: byId("toggleBlueprintImg"),
  toggleProjectedSectors: byId("toggleProjectedSectors"),
  blueprintImg: byId("blueprintImg"),
  inspectorSectorName: byId("inspectorSectorName"),
  inspectorStatusBadge: byId("inspectorStatusBadge"),
  inspectorSectorId: byId("inspectorSectorId"),
  inspectorSectorNode: byId("inspectorSectorNode"),
  inspectorSectorType: byId("inspectorSectorType"),
  inspectorPirStatus: byId("inspectorPirStatus"),
  inspectorPanicStatus: byId("inspectorPanicStatus"),
  inspectorCommStatus: byId("inspectorCommStatus"),
  inspectorSimActions: byId("inspectorSimActions"),
  inspectorBtnToggleMotion: byId("inspectorBtnToggleMotion"),
  inspectorBtnTriggerPanic: byId("inspectorBtnTriggerPanic"),
  inspectorBtnToggleComm: byId("inspectorBtnToggleComm"),

  // Tab 2: Grilla y Eventos
  sectorsCardsGrid: byId("sectorsCardsGrid"),
  eventsTableBody: byId("eventsTableBody"),

  // Tab 3: NodeMCU hardware
  boot: byId("bootState"),
  distance: byId("distanceValue"),
  meter: byId("distanceMeter"),
  sampleAge: byId("sampleAge"),
  sensorMessage: byId("sensorMessage"),
  wifi: byId("wifiState"),
  filesystem: byId("filesystemState"),
  firmware: byId("firmwareState"),
  uptime: byId("uptime"),
  serverState: byId("serverState"),
  serverUrl: byId("serverUrl"),
  serverPort: byId("serverPort"),
  serverLastRequest: byId("serverLastRequest"),
  serverRoutes: byId("serverRoutes"),
  stateSourceInput: byId("stateSourceInput"),
  applySourceButton: byId("applySourceButton"),
  cpuLimit: byId("cpuLimit"),
  flashLimit: byId("flashLimit"),
  triggerGpio: byId("triggerGpio"),
  echoGpio: byId("echoGpio"),
  led: byId("ledIndicator"),
  eventLog: byId("eventLog"),

  // Tab 4: Banco de Pruebas
  tabTestsButton: byId("tabBtnTests"),
  btnSetDisarmed: byId("btnSetDisarmed"),
  btnSetArming: byId("btnSetArming"),
  btnSetArmed: byId("btnSetArmed"),
  btnSetAlarm: byId("btnSetAlarm"),
  resetAllBtn: byId("resetAllBtn"),
  vBusControl: byId("vBusControl"),
  vBusSliderDisplay: byId("vBusSliderDisplay"),
  vBusPresetNominal: byId("vBusPresetNominal"),
  vBusPresetWarning: byId("vBusPresetWarning"),
  vBusPresetAlarm: byId("vBusPresetAlarm"),
  simSectorCardsContainer: byId("simSectorCardsContainer"),
  distanceControl: byId("distanceControl"),
  distanceOutput: byId("distanceOutput"),
  echoControl: byId("echoControl"),
  wifiControl: byId("wifiControl"),
  filesystemControl: byId("filesystemControl"),
  rebootButton: byId("rebootButton"),
  controlError: byId("controlError"),
};

// ==========================================================================
// Estado local de la aplicación cliente
// ==========================================================================

let latestState = null;
let selectedSectorId = 1;
let isPollingActive = true;
let isSendingControl = false;

// ==========================================================================
// Configuración de pestañas
// ==========================================================================

function setupTabs() {
  el.tabButtons.forEach((btn) => {
    btn.addEventListener("click", () => {
      const targetId = btn.getAttribute("data-tab");
      el.tabButtons.forEach((b) => b.classList.remove("active"));
      el.tabPanes.forEach((p) => {
        p.classList.remove("active");
        p.hidden = true;
      });

      btn.classList.add("active");
      const targetPane = byId(targetId);
      if (targetPane) {
        targetPane.classList.add("active");
        targetPane.hidden = false;
      }
    });
  });
}

// ==========================================================================
// Obtener URL de estado (admite /api/state o proxy a NodeMCU real)
// ==========================================================================

function getStateUrl() {
  const value = el.stateSourceInput ? el.stateSourceInput.value.trim() : "";
  if (!value || value === "/api/state") {
    return "/api/state";
  }
  return `/api/proxy?url=${encodeURIComponent(value)}`;
}

// ==========================================================================
// Renderizado: Semáforo del Nodo Central y Telemetría
// ==========================================================================

function renderTrafficLightAndTelemetry(state) {
  const alarm = state.alarm_system || {
    state: "DISARMED",
    arming_remaining_s: 0,
    v_bus: 12.4,
    v_bus_status: "normal",
    active_areas: [],
    no_response_areas: [],
    sectors: [],
  };

  // Semáforo central
  const stateClassMap = {
    DISARMED: "state-disarmed",
    ARMING: "state-arming",
    ARMED: "state-armed",
    ALARM: "state-alarm",
  };
  const stateTextMap = {
    DISARMED: "DESARMADO",
    ARMING: "ARMANDO",
    ARMED: "ARMADO",
    ALARM: "ALARMA ACTIVA",
  };

  el.centralTrafficLight.className = `traffic-light-pill ${stateClassMap[alarm.state] || "state-disarmed"}`;
  el.trafficLightState.textContent = stateTextMap[alarm.state] || alarm.state;

  if (alarm.state === "ARMING" && alarm.arming_remaining_s > 0) {
    el.armingTimer.hidden = false;
    el.armingTimer.textContent = `${Math.ceil(alarm.arming_remaining_s)}s`;
    audio.playArmingBeep(alarm.arming_remaining_s);
  } else {
    el.armingTimer.hidden = true;
  }

  // Audio de sirena en estado ALARM
  if (alarm.state === "ALARM") {
    audio.playSiren();
  } else {
    audio.stopSiren();
  }

  // Badge de conexión y modo simulación
  el.connectionBadge.textContent = state.simulator ? "Simulador local" : "ESP8266 conectado";
  el.connectionBadge.className = `badge ${state.state === "running" ? "good" : "bad"}`;
  el.simulationNotice.hidden = !state.simulator;

  // Telemetría V_bus
  el.vBusValue.textContent = `${alarm.v_bus.toFixed(1)} V`;
  el.vBusStatusBadge.className = `mini-chip ${alarm.v_bus_status}`;
  el.vBusStatusBadge.textContent =
    alarm.v_bus_status === "alarm" ? "Crítico (<11V)" : alarm.v_bus_status === "warning" ? "Alerta (<11.8V)" : "Normal";

  // Sectores activos
  const activeCount = (alarm.active_areas || []).length;
  el.activeSectorsCount.textContent = `${activeCount} / 5`;
  el.activeSectorsCount.className = `telemetry-value ${activeCount > 0 ? "text-yellow" : ""}`;

  // Bus RS485
  const hasNoResponse = (alarm.no_response_areas || []).length > 0;
  el.rs485Status.textContent = hasNoResponse ? `${alarm.no_response_areas.length} sin respuesta` : "Operativo";
  el.rs485Status.className = `telemetry-value ${hasNoResponse ? "text-yellow" : "text-green"}`;
}

// ==========================================================================
// Renderizado: Mapa SVG de Sectores
// ==========================================================================

function renderMap(state) {
  const alarm = state.alarm_system || { sectors: [] };
  const sectors = alarm.sectors || [];

  sectors.forEach((sec) => {
    const group = byId(`mapSector-${sec.id}`);
    const poly = byId(`area-${sec.id}`);
    if (!group || !poly) return;

    // Actualizar clases del polígono según estado
    poly.className.baseVal = `sector-poly ${sec.status}`;

    // Clases del grupo para animación del pulso
    group.classList.remove("state-motion", "state-alarm", "selected");
    if (sec.status === "motion") group.classList.add("state-motion");
    if (sec.status === "alarm") group.classList.add("state-alarm");
    if (sec.id === selectedSectorId) group.classList.add("selected");
  });
}

function setupMapInteractions() {
  // Click en sectores del mapa SVG
  for (let sid = 1; sid <= 9; sid++) {
    const group = byId(`mapSector-${sid}`);
    if (group) {
      group.addEventListener("click", () => {
        selectSector(sid);
      });
    }
  }

  // Toggle de visibilidad de plano arquitectónico de fondo
  el.toggleBlueprintImg.addEventListener("change", (e) => {
    el.blueprintImg.style.display = e.target.checked ? "block" : "none";
  });

  // Toggle de sectores proyectados
  el.toggleProjectedSectors.addEventListener("change", (e) => {
    const isChecked = e.target.checked;
    for (let sid = 6; sid <= 9; sid++) {
      const g = byId(`mapSector-${sid}`);
      if (g) {
        g.style.display = isChecked ? "block" : "none";
        g.style.opacity = isChecked ? "0.85" : "0.3";
      }
    }
  });
}

function selectSector(sectorId) {
  selectedSectorId = sectorId;
  if (latestState) {
    renderMap(latestState);
    renderSectorInspector(sectorId, latestState);
  }
}

// ==========================================================================
// Renderizado: Inspector del Sector Seleccionado
// ==========================================================================

function renderSectorInspector(sectorId, state) {
  const meta = SECTORS_META[sectorId] || {
    id: sectorId,
    name: `Sector ${sectorId}`,
    node: "Desconocido",
    hasPir: true,
    hasPanic: false,
    projected: false,
  };

  const alarm = state.alarm_system || { sectors: [] };
  const secData = (alarm.sectors || []).find((s) => s.id === sectorId) || {
    id: sectorId,
    status: meta.projected ? "projected" : "normal",
    active: false,
    motion: false,
    no_response: false,
  };

  el.inspectorSectorName.textContent = meta.name;
  el.inspectorSectorId.textContent = `Zona #${meta.id}`;
  el.inspectorSectorNode.textContent = meta.node;
  el.inspectorSectorType.textContent = meta.hasPanic ? "Sensor PIR + Pulsador de Emergencia" : "Sensor PIR volumétrico";

  // Badges y textos de estado
  const statusLabels = {
    normal: ["Reposo", "good"],
    motion: ["Movimiento Detectado", "warning"],
    alarm: ["¡ALARMA ACTIVA!", "bad"],
    no_response: ["Sin Respuesta en Bus", "warning"],
    projected: ["Sector Proyectado", ""],
  };
  const [badgeText, badgeClass] = statusLabels[secData.status] || ["Normal", "good"];
  el.inspectorStatusBadge.textContent = badgeText;
  el.inspectorStatusBadge.className = `state-chip ${badgeClass}`;

  // Sensores
  el.inspectorPirStatus.textContent = secData.motion ? "🔴 ACTIVADO (Movimiento)" : "🟢 Reposo";
  el.inspectorPanicStatus.textContent = !meta.hasPanic
    ? "No disponible"
    : secData.active && !secData.motion
    ? "🚨 DISPARADO"
    : "🟢 En espera";

  el.inspectorCommStatus.textContent = meta.id === 1 ? "Local (Arduino Nano)" : secData.no_response ? "❌ Timeout de respuesta" : "🟢 Conectado (RS485)";

  // Configuración de botones de simulación del inspector
  el.inspectorBtnToggleMotion.textContent = secData.motion ? "🛑 Cancelar Movimiento PIR" : "🏃 Simular Movimiento (PIR)";
  el.inspectorBtnToggleMotion.onclick = () => sendControls({ toggle_motion: meta.id });

  if (meta.hasPanic) {
    el.inspectorBtnTriggerPanic.hidden = false;
    el.inspectorBtnTriggerPanic.onclick = () => sendControls({ trigger_emergency: meta.id });
  } else {
    el.inspectorBtnTriggerPanic.hidden = true;
  }

  if (meta.id > 1) {
    el.inspectorBtnToggleComm.hidden = false;
    el.inspectorBtnToggleComm.textContent = secData.no_response ? "🔄 Restaurar Bus RS485" : "⚠️ Simular Corte RS485";
    el.inspectorBtnToggleComm.onclick = () => sendControls({ toggle_no_response: meta.id });
  } else {
    el.inspectorBtnToggleComm.hidden = true;
  }
}

// ==========================================================================
// Renderizado: Grilla de Tarjetas de Sectores y Tabla de Eventos (Tab 2)
// ==========================================================================

function renderSectorsGrid(state) {
  const alarm = state.alarm_system || { sectors: [] };
  const sectors = alarm.sectors || [];
  if (!sectors.length) return;

  el.sectorsCardsGrid.innerHTML = sectors
    .map((sec) => {
      const meta = SECTORS_META[sec.id] || { hasPanic: false };
      const statusTitleMap = {
        normal: "Reposo",
        motion: "Movimiento",
        alarm: "Alarma",
        no_response: "Sin Respuesta",
        projected: "Proyectado",
      };

      return `
      <div class="sector-card status-${sec.status}" onclick="selectAndShowMap(${sec.id})">
        <div class="sector-card-header">
          <h3 class="sector-card-title">Zona ${sec.id}: ${sec.name}</h3>
          <span class="mini-chip ${sec.status === 'alarm' ? 'alarm' : sec.status === 'motion' ? 'warning' : 'normal'}">
            ${statusTitleMap[sec.status] || sec.status}
          </span>
        </div>
        <div class="sector-card-node">${sec.node}</div>
        <div class="sector-card-indicators">
          <span class="indicator-tag">PIR: ${sec.motion ? "Detectando" : "Reposo"}</span>
          ${meta.hasPanic ? `<span class="indicator-tag">Pulsador: Disponible</span>` : ""}
          <span class="indicator-tag">Bus: ${sec.no_response ? "Desconectado" : "OK"}</span>
        </div>
      </div>
    `;
    })
    .join("");
}

window.selectAndShowMap = function (sectorId) {
  selectSector(sectorId);
  const mapBtn = byId("tabBtnMap");
  if (mapBtn) mapBtn.click();
};

function renderEventsTable(events) {
  if (!events || !events.length) {
    el.eventsTableBody.innerHTML = `<tr><td colspan="6" class="text-center muted">Sin eventos registrados aún.</td></tr>`;
    return;
  }

  el.eventsTableBody.innerHTML = events
    .map(
      (ev) => `
    <tr>
      <td><code>${ev.ts || "—"}</code></td>
      <td><span class="severity-chip ${ev.severity || "info"}">${ev.severity || "info"}</span></td>
      <td><strong>${ev.area_name || (ev.area ? `Sector ${ev.area}` : "Sistema")}</strong></td>
      <td><code>${ev.code || "—"}</code></td>
      <td>${ev.event || "—"}</td>
      <td class="muted">${ev.details || "—"}</td>
    </tr>
  `
    )
    .join("");
}

// ==========================================================================
// Renderizado: Panel de Hardware NodeMCU (Tab 3)
// ==========================================================================

function renderBoardAndSensor(state) {
  const bootLabels = {
    running: ["En funcionamiento", true],
    waiting_wifi: ["Esperando Wi-Fi", false],
    halted_filesystem: ["Detenido: LittleFS", false],
  };
  const [bootText, bootGood] = bootLabels[state.state] || ["Estado desconocido", false];
  el.boot.textContent = bootText;
  el.boot.className = `state-chip ${bootGood ? "good" : "bad"}`;

  // Conectividad
  setStatus(el.wifi, state.wifi ? "Conectado" : "Desconectado", state.wifi);
  setStatus(el.filesystem, state.filesystem ? "Montado" : "Error de montaje", state.filesystem);
  setStatus(el.firmware, state.state === "running" ? "Ejecución normal" : "Bloqueado", state.state === "running");
  el.uptime.textContent = `${Math.floor((state.uptime_ms || 0) / 1000)} s`;

  // Límites
  if (state.limits) {
    el.cpuLimit.textContent = `${state.limits.cpu_mhz} MHz`;
    el.flashLimit.textContent = `${Math.round(state.limits.flash_bytes / (1024 * 1024))} MB`;
  }
  if (state.pins) {
    el.triggerGpio.textContent = `GPIO${state.pins.trigger_gpio} (${state.pins.trigger_label})`;
    el.echoGpio.textContent = `GPIO${state.pins.echo_gpio} (${state.pins.echo_label})`;
  }

  // Sensor ultrasónico
  const sensor = state.sensor || {};
  if (sensor.valid && typeof sensor.distance_cm === "number") {
    el.distance.textContent = sensor.distance_cm.toFixed(1);
    const pct = Math.min(100, Math.max(0, (sensor.distance_cm / 400) * 100));
    el.meter.style.width = `${pct}%`;
    el.sampleAge.textContent = `${sensor.age_ms || 0} ms atrás`;
    el.sensorMessage.textContent = `Medición válida dentro del rango (2 – 400 cm).`;
    if (el.led) el.led.style.background = "#50d890";
  } else {
    el.distance.textContent = "—";
    el.meter.style.width = "0%";
    el.sampleAge.textContent = "Sin lectura";
    el.sensorMessage.textContent = "Sin eco o sensor deshabilitado.";
    if (el.led) el.led.style.background = "#ff6878";
  }

  // Servidor HTTP
  const server = state.server || {};
  el.serverState.textContent = server.status === "running" ? "Activo" : "Degradado";
  el.serverState.className = `state-chip ${server.status === "running" ? "good" : "bad"}`;
  el.serverUrl.textContent = server.base_url || "—";
  el.serverPort.textContent = server.port || "8080";
  el.serverLastRequest.textContent = server.last_request || "/";

  if (server.routes && server.routes.length) {
    el.serverRoutes.innerHTML = server.routes.map((r) => `<span class="route-pill">${r}</span>`).join("");
  }

  // Logs seriales
  if (state.logs && state.logs.length) {
    el.eventLog.innerHTML = state.logs.map((log) => `<li>${log}</li>`).join("");
    el.eventLog.scrollTop = el.eventLog.scrollHeight;
  }
}

function setStatus(element, text, good) {
  if (!element) return;
  element.textContent = text;
  element.className = good ? "text-green" : "text-red";
}

// ==========================================================================
// Renderizado: Banco de Pruebas (Tab 4)
// ==========================================================================

function setupTestBench() {
  // Botones de máquina de estado central
  el.btnSetDisarmed.onclick = () => sendControls({ alarm_state: "DISARMED" });
  el.btnSetArming.onclick = () => sendControls({ alarm_state: "ARMING" });
  el.btnSetArmed.onclick = () => sendControls({ alarm_state: "ARMED" });
  el.btnSetAlarm.onclick = () => sendControls({ alarm_state: "ALARM" });
  el.resetAllBtn.onclick = () => sendControls({ reset_all: true });

  // Slider de tensión V_bus
  el.vBusControl.addEventListener("input", (e) => {
    el.vBusSliderDisplay.textContent = `${parseFloat(e.target.value).toFixed(1)} V`;
  });
  el.vBusControl.addEventListener("change", (e) => {
    sendControls({ v_bus: parseFloat(e.target.value) });
  });

  // Presets de tensión
  el.vBusPresetNominal.onclick = () => {
    el.vBusControl.value = 12.4;
    el.vBusSliderDisplay.textContent = "12.4 V";
    sendControls({ v_bus: 12.4 });
  };
  el.vBusPresetWarning.onclick = () => {
    el.vBusControl.value = 11.5;
    el.vBusSliderDisplay.textContent = "11.5 V";
    sendControls({ v_bus: 11.5 });
  };
  el.vBusPresetAlarm.onclick = () => {
    el.vBusControl.value = 10.5;
    el.vBusSliderDisplay.textContent = "10.5 V";
    sendControls({ v_bus: 10.5 });
  };

  // Controles de hardware NodeMCU
  el.distanceControl.addEventListener("input", (e) => {
    el.distanceOutput.textContent = `${e.target.value} cm`;
  });
  el.distanceControl.addEventListener("change", (e) => {
    sendControls({ distance_cm: parseFloat(e.target.value) });
  });

  el.echoControl.onchange = (e) => sendControls({ echo_available: e.target.checked });
  el.wifiControl.onchange = (e) => sendControls({ wifi_available: e.target.checked });
  el.filesystemControl.onchange = (e) => sendControls({ filesystem_mounts: e.target.checked });
  el.rebootButton.onclick = () => sendControls({ reboot: true });
}

function renderTestBenchMatrix(state) {
  const alarm = state.alarm_system || { sectors: [] };
  const sectors = (alarm.sectors || []).filter((s) => !s.projected);

  el.simSectorCardsContainer.innerHTML = sectors
    .map((sec) => {
      const meta = SECTORS_META[sec.id] || {};
      return `
      <div class="sim-sector-card">
        <div class="sim-sector-header">
          <h4>Zona ${sec.id}: ${sec.name}</h4>
          <span class="mini-chip ${sec.status === 'alarm' ? 'alarm' : sec.status === 'motion' ? 'warning' : 'normal'}">
            ${sec.status}
          </span>
        </div>
        <div class="sim-sector-actions">
          <button class="secondary outline" onclick="sendSimMotion(${sec.id})">
            ${sec.motion ? "🛑 Cancelar Movimiento" : "🏃 Simular Movimiento"}
          </button>
          ${
            meta.hasPanic
              ? `<button class="contrast outline" onclick="sendSimPanic(${sec.id})">🚨 Disparar Emergencia</button>`
              : ""
          }
          ${
            sec.id > 1
              ? `<button class="secondary outline" onclick="sendSimComm(${sec.id})">${sec.no_response ? "🔄 Restaurar Bus" : "⚠️ Simular Corte RS485"}</button>`
              : ""
          }
        </div>
      </div>
    `;
    })
    .join("");
}

window.sendSimMotion = (id) => sendControls({ toggle_motion: id });
window.sendSimPanic = (id) => sendControls({ trigger_emergency: id });
window.sendSimComm = (id) => sendControls({ toggle_no_response: id });

// ==========================================================================
// Envío de controles al simulador (POST /api/controls)
// ==========================================================================

async function sendControls(payload) {
  if (isSendingControl) return;
  isSendingControl = true;
  el.controlError.hidden = true;

  try {
    const res = await fetch("/api/controls", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });

    if (!res.ok) {
      const errData = await res.json().catch(() => ({}));
      throw new Error(errData.error || `Error HTTP ${res.status}`);
    }

    const newState = await res.json();
    handleStateUpdate(newState);
  } catch (err) {
    console.error("Control update failed:", err);
    el.controlError.textContent = `Fallo al aplicar control: ${err.message}`;
    el.controlError.hidden = false;
  } finally {
    isSendingControl = false;
  }
}

// ==========================================================================
// Bucle de consulta de estado (GET /api/state)
// ==========================================================================

async function fetchState() {
  if (!isPollingActive) return;

  try {
    const url = getStateUrl();
    const res = await fetch(url, { cache: "no-store" });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const state = await res.json();
    handleStateUpdate(state);
  } catch (err) {
    el.connectionBadge.textContent = "Desconectado";
    el.connectionBadge.className = "badge bad";
  }
}

function handleStateUpdate(state) {
  latestState = state;
  const controlsAvailable = state.simulator === true;
  el.tabTestsButton.hidden = !controlsAvailable;
  el.inspectorSimActions.hidden = !controlsAvailable;

  if (!controlsAvailable && el.tabTestsButton.classList.contains("active")) {
    byId("tabBtnBoard").click();
  }

  // 1. Topbar y Telemetría
  renderTrafficLightAndTelemetry(state);

  // 2. Mapa interactivo e inspector
  renderMap(state);
  renderSectorInspector(selectedSectorId, state);

  // 3. Grilla y eventos
  renderSectorsGrid(state);
  if (state.alarm_system && state.alarm_system.recent_events) {
    renderEventsTable(state.alarm_system.recent_events);
  }

  // 4. Panel NodeMCU
  renderBoardAndSensor(state);

  // 5. Matriz de prueba
  renderTestBenchMatrix(state);
}

// ==========================================================================
// Inicialización al cargar el DOM
// ==========================================================================

document.addEventListener("DOMContentLoaded", () => {
  setupTabs();
  setupMapInteractions();
  setupTestBench();

  // Control de sonido
  el.soundToggleBtn.addEventListener("click", () => {
    const isEnabled = audio.toggle();
    el.soundIcon.textContent = isEnabled ? "🔊" : "🔇";
    el.soundText.textContent = isEnabled ? "Activo" : "Silenciado";
    el.soundToggleBtn.className = isEnabled ? "sound-toggle-btn good" : "sound-toggle-btn secondary outline";
  });

  // Selector de fuente de estado
  if (el.applySourceButton) {
    el.applySourceButton.addEventListener("click", () => fetchState());
  }

  // Primera consulta y bucle cada 1000ms
  fetchState();
  setInterval(fetchState, 1000);
});
