/**
 * main.js — Cliente web de producción para el Firmware NodeMCU ESP8266
 * Sistema de Alarma Distribuido para Colegio
 *
 * Modo de solo lectura: visualización en tiempo real de telemetría, mapa SVG,
 * grilla de sectores y estado del microcontrolador.
 */

// ==========================================================================
// Síntesis de Audio Web (Sirena y Beeps de Armado)
// ==========================================================================

class AlarmAudioSynthesizer {
  constructor() {
    this.ctx = null;
    this.enabled = false;
    this.isSirenPlaying = false;
    this.sirenOsc1 = null;
    this.sirenLfo = null;
    this.sirenGain = null;
    this.sirenLfoGain = null;
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
      this.ctx.resume().catch(() => {});
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
    if (!this.enabled) return;
    this.initContext();
    if (!this.ctx || this.isSirenPlaying) return;
    try {
      this.isSirenPlaying = true;
      const now = this.ctx.currentTime;

      // Oscilador modulador LFO (frecuencia de barrido)
      this.sirenLfo = this.ctx.createOscillator();
      this.sirenLfo.frequency.setValueAtTime(1.5, now);

      this.sirenLfoGain = this.ctx.createGain();
      this.sirenLfoGain.gain.setValueAtTime(250, now);

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
      this.isSirenPlaying = false;
      console.warn("Audio warning:", e);
    }
  }

  stopSiren() {
    if (!this.isSirenPlaying) return;
    try {
      if (this.sirenOsc1) {
        try { this.sirenOsc1.stop(); } catch (_) {}
        try { this.sirenOsc1.disconnect(); } catch (_) {}
      }
      if (this.sirenLfo) {
        try { this.sirenLfo.stop(); } catch (_) {}
        try { this.sirenLfo.disconnect(); } catch (_) {}
      }
    } finally {
      this.isSirenPlaying = false;
      this.sirenOsc1 = null;
      this.sirenLfo = null;
      this.sirenGain = null;
      this.sirenLfoGain = null;
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
// Metadatos de Sectores
// ==========================================================================

const SECTORS_META = {
  1: { id: 1, name: "Área Operativa", node: "Local (Nano central)", hasPir: true, hasPanic: false, projected: false },
  2: { id: 2, name: "Automotores", node: "Nano remoto #2 (RS485)", hasPir: true, hasPanic: false, projected: false },
  3: { id: 3, name: "Jefatura de Taller", node: "Nano remoto #3 (RS485)", hasPir: true, hasPanic: true, projected: false },
  4: { id: 4, name: "Vicedirección", node: "Nano remoto #4 (RS485)", hasPir: true, hasPanic: true, projected: false },
  5: { id: 5, name: "Informática", node: "Nano remoto #5 (RS485)", hasPir: true, hasPanic: false, projected: false },
  6: { id: 6, name: "Biblioteca", node: "Nano remoto #6 (RS485)", hasPir: true, hasPanic: false, projected: true },
  7: { id: 7, name: "Preceptoría 1", node: "Nano remoto #7 (RS485)", hasPir: true, hasPanic: false, projected: true },
  8: { id: 8, name: "Preceptoría 2", node: "Nano remoto #8 (RS485)", hasPir: true, hasPanic: false, projected: true },
  9: { id: 9, name: "Preceptoría 3", node: "Nano remoto #9 (RS485)", hasPir: true, hasPanic: false, projected: true },
};

function byId(id) {
  return document.getElementById(id);
}

// ==========================================================================
// Referencias a elementos del DOM de Producción
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
  cpuLimit: byId("cpuLimit"),
  flashLimit: byId("flashLimit"),
  triggerGpio: byId("triggerGpio"),
  echoGpio: byId("echoGpio"),
  led: byId("ledIndicator"),
  eventLog: byId("eventLog"),
};

let latestState = null;
let selectedSectorId = 1;
let isPollingActive = true;

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

  // Badge de conexión
  el.connectionBadge.textContent = state.state === "running" ? "ESP8266 Conectado" : "Esperando";
  el.connectionBadge.className = `badge ${state.state === "running" ? "good" : "bad"}`;

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
  if (el.toggleBlueprintImg && el.blueprintImg) {
    el.toggleBlueprintImg.addEventListener("change", (e) => {
      el.blueprintImg.style.display = e.target.checked ? "block" : "none";
    });
  }

  // Toggle de sectores proyectados
  function updateProjectedVisibility() {
    const isChecked = !!(el.toggleProjectedSectors && el.toggleProjectedSectors.checked);
    for (let sid = 6; sid <= 9; sid++) {
      const g = byId(`mapSector-${sid}`);
      if (g) {
        g.style.display = isChecked ? "block" : "none";
        g.style.opacity = isChecked ? "0.85" : "0.3";
      }
    }
  }

  if (el.toggleProjectedSectors) {
    el.toggleProjectedSectors.addEventListener("change", updateProjectedVisibility);
    updateProjectedVisibility();
  }
}

function selectSector(sectorId) {
  selectedSectorId = sectorId;
  if (latestState) {
    renderMap(latestState);
    renderSectorInspector(sectorId, latestState);
  }
}

// ==========================================================================
// Renderizado: Inspector del Sector Seleccionado (Solo Lectura)
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
  const meterParent = el.meter ? (el.meter.closest("[role='meter']") || el.meter.parentElement) : null;
  if (sensor.valid && typeof sensor.distance_cm === "number") {
    el.distance.textContent = sensor.distance_cm.toFixed(1);
    const pct = Math.min(100, Math.max(0, (sensor.distance_cm / 400) * 100));
    el.meter.style.width = `${pct}%`;
    if (meterParent) {
      meterParent.setAttribute("aria-valuenow", sensor.distance_cm.toFixed(1));
    }
    el.sampleAge.textContent = `${sensor.age_ms || 0} ms atrás`;
    el.sensorMessage.textContent = `Medición válida dentro del rango (2 – 400 cm).`;
    if (el.led) el.led.style.background = "#50d890";
  } else {
    el.distance.textContent = "—";
    el.meter.style.width = "0%";
    if (meterParent) {
      meterParent.setAttribute("aria-valuenow", "0");
    }
    el.sampleAge.textContent = "Sin lectura";
    el.sensorMessage.textContent = "Sin eco o sensor deshabilitado.";
    if (el.led) el.led.style.background = "#ff6878";
  }

  // Servidor HTTP
  const server = state.server || {};
  el.serverState.textContent = server.status === "running" ? "Activo" : "Degradado";
  el.serverState.className = `state-chip ${server.status === "running" ? "good" : "bad"}`;
  el.serverUrl.textContent = server.base_url || "—";
  el.serverPort.textContent = server.port || "80";
  el.serverLastRequest.textContent = server.last_request || "—";

  if (server.routes && el.serverRoutes) {
    el.serverRoutes.innerHTML = server.routes.map((r) => `<span class="route-pill">${r}</span>`).join("");
  }

  // Log serial
  if (state.logs && el.eventLog) {
    el.eventLog.innerHTML = state.logs.map((log) => `<li>${escapeHtml(log)}</li>`).join("");
  }
}

function setStatus(target, text, isGood) {
  if (!target) return;
  target.textContent = text;
  target.className = isGood ? "text-green" : "text-red";
}

function escapeHtml(str) {
  return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

// ==========================================================================
// Bucle de consulta de estado (GET /api/state)
// ==========================================================================

async function fetchState() {
  if (!isPollingActive) return;

  try {
    const res = await fetch("/api/state", { cache: "no-store" });
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
}

// ==========================================================================
// Inicialización al cargar el DOM
// ==========================================================================

document.addEventListener("DOMContentLoaded", () => {
  setupTabs();
  setupMapInteractions();

  // Control de sonido
  if (el.soundToggleBtn) {
    el.soundToggleBtn.addEventListener("click", () => {
      const isEnabled = audio.toggle();
      el.soundIcon.textContent = isEnabled ? "🔊" : "🔇";
      el.soundText.textContent = isEnabled ? "Activo" : "Silenciado";
      el.soundToggleBtn.className = isEnabled ? "sound-toggle-btn good" : "sound-toggle-btn secondary outline";
      if (isEnabled && latestState && latestState.alarm_system && latestState.alarm_system.state === "ALARM") {
        audio.playSiren();
      }
    });
  }

  // Interacción para reanudar AudioContext si el navegador lo suspendió
  document.addEventListener("click", () => {
    if (audio.enabled && audio.ctx && audio.ctx.state === "suspended") {
      audio.ctx.resume().catch(() => {});
    }
  });

  // Primera consulta y bucle cada 1000ms
  fetchState();
  setInterval(fetchState, 1000);
});
