(() => {
  "use strict";

  const state = {
    authenticated: false,
    authToken: localStorage.getItem("auth_token") || null,
    selectedSectorId: 1,
    soundEnabled: false,
    systemState: "DISARMED",
    armingRemainingS: 0,
    graceRemainingS: 0,
    triggerReason: 0,
    triggerSector: 0,
    sectors: [],
    events: []
  };

  class AlarmSoundPlayer {
    constructor() {
      this.ctx = null;
      this.osc = null;
      this.modOsc = null;
      this.gain = null;
      this.isPlaying = false;
    }

    _init() {
      if (!this.ctx) {
        const AudioCtx = window.AudioContext || window.webkitAudioContext;
        this.ctx = new AudioCtx();
      }
    }

    start() {
      if (this.isPlaying) return;
      try {
        this._init();
        if (this.ctx.state === "suspended") {
          this.ctx.resume();
        }

        const now = this.ctx.currentTime;
        this.osc = this.ctx.createOscillator();
        this.modOsc = this.ctx.createOscillator();
        this.gain = this.ctx.createGain();
        const modGain = this.ctx.createGain();

        // Dual-tone siren modulation (600 Hz to 900 Hz, 2 Hz LFO)
        this.osc.type = "sawtooth";
        this.osc.frequency.setValueAtTime(750, now);

        this.modOsc.type = "sine";
        this.modOsc.frequency.setValueAtTime(2.0, now);

        modGain.gain.setValueAtTime(150, now);

        this.modOsc.connect(modGain);
        modGain.connect(this.osc.frequency);

        this.gain.gain.setValueAtTime(0.15, now);
        this.osc.connect(this.gain);
        this.gain.connect(this.ctx.destination);

        this.modOsc.start();
        this.osc.start();
        this.isPlaying = true;
      } catch (err) {
        console.warn("Audio Context error:", err);
      }
    }

    stop() {
      if (!this.isPlaying) return;
      try {
        if (this.osc) {
          this.osc.stop();
          this.osc.disconnect();
          this.osc = null;
        }
        if (this.modOsc) {
          this.modOsc.stop();
          this.modOsc.disconnect();
          this.modOsc = null;
        }
        this.isPlaying = false;
      } catch (err) {
        console.warn("Audio Stop error:", err);
      }
    }
  }

  const soundPlayer = new AlarmSoundPlayer();

  const dom = {
    loginModal: document.getElementById("loginModal"),
    loginForm: document.getElementById("loginForm"),
    loginPassword: document.getElementById("loginPassword"),
    loginError: document.getElementById("loginError"),
    netStatusBadge: document.getElementById("netStatusBadge"),
    trafficLight: document.getElementById("centralTrafficLight"),
    trafficLightState: document.getElementById("trafficLightState"),
    armingTimer: document.getElementById("armingTimer"),
    armToggleBtn: document.getElementById("armToggleBtn"),
    soundToggleBtn: document.getElementById("soundToggleBtn"),
    soundIcon: document.getElementById("soundIcon"),
    soundText: document.getElementById("soundText"),
    logoutBtn: document.getElementById("logoutBtn"),
    graceAlertBanner: document.getElementById("graceAlertBanner"),
    graceAlertText: document.getElementById("graceAlertText"),
    graceDisarmBtn: document.getElementById("graceDisarmBtn"),
    inspectorTitle: document.getElementById("inspectorTitle"),
    inspectorStatusChip: document.getElementById("inspectorStatusChip"),
    inspectorContent: document.getElementById("inspectorContent"),
    inspectorSectorToggle: document.getElementById("inspectorSectorToggle"),
    inspectorPingBtn: document.getElementById("inspectorPingBtn"),
    eventsTableBody: document.getElementById("eventsTableBody"),
    sectorInspectorTpl: document.getElementById("sectorInspectorTpl")?.innerHTML || ""
  };

  async function apiFetch(url, options = {}) {
    const headers = options.headers || {};
    if (state.authToken) {
      headers["X-Auth-Token"] = state.authToken;
    }
    options.headers = headers;
    options.credentials = "same-origin";
    return fetch(url, options);
  }

  async function computeSha256(ascii) {
    if (window.crypto && window.crypto.subtle) {
      const buffer = new TextEncoder().encode(ascii);
      const hashBuffer = await crypto.subtle.digest("SHA-256", buffer);
      return Array.from(new Uint8Array(hashBuffer))
        .map(b => b.toString(16).padStart(2, "0"))
        .join("");
    }
    return ascii;
  }

  async function doLogin(password) {
    try {
      const passwordHash = await computeSha256(password);
      const res = await fetch("/api/login", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: new URLSearchParams({ password: passwordHash })
      });
      const data = await res.json();
      if (res.ok && data.authenticated) {
        state.authenticated = true;
        state.authToken = data.token;
        localStorage.setItem("auth_token", data.token);
        dom.loginModal.removeAttribute("open");
        dom.loginError.hidden = true;
        addEvent("Sistema", "info", "Inicio de sesión exitoso");
        fetchState();
      } else {
        dom.loginError.hidden = false;
      }
    } catch (err) {
      dom.loginError.textContent = "Error de conexión con el gateway.";
      dom.loginError.hidden = false;
    }
  }

  async function doLogout() {
    try {
      await apiFetch("/api/logout", { method: "POST" });
    } catch (_) {}
    state.authenticated = false;
    state.authToken = null;
    localStorage.removeItem("auth_token");
    soundPlayer.stop();
    if (dom.graceAlertBanner) {
      dom.graceAlertBanner.hidden = true;
    }
    dom.loginModal.setAttribute("open", "");
    addEvent("Sistema", "info", "Sesión cerrada por el operador");
  }

  async function setArmSystem(shouldArm) {
    try {
      await apiFetch("/api/system/arm", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: new URLSearchParams({ arm: shouldArm ? "true" : "false" })
      });
      addEvent("Central", "warning", shouldArm ? "Comando de armado enviado" : "Comando de desarmado enviado");
      fetchState();
    } catch (err) {
      console.error("Error al armar/desarmar:", err);
    }
  }

  async function toggleArmSystem() {
    const shouldArm = (state.systemState === "DISARMED");
    return setArmSystem(shouldArm);
  }

  async function toggleSector(sectorId, enabled) {
    try {
      await apiFetch("/api/sector/toggle", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: new URLSearchParams({ sector_id: String(sectorId), enabled: enabled ? "true" : "false" })
      });
      addEvent(`Sector ${sectorId}`, "info", enabled ? "Vigilancia habilitada" : "Vigilancia deshabilitada (Bypass)");
      fetchState();
    } catch (err) {
      console.error("Error al conmutar sector:", err);
    }
  }

  async function pingSector(sectorId) {
    try {
      await apiFetch("/api/sector/ping", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: new URLSearchParams({ sector_id: String(sectorId) })
      });
      addEvent(`Sector ${sectorId}`, "info", "Solicitud de sondeo manual enviada");
      fetchState();
    } catch (err) {
      console.error("Error al enviar ping:", err);
    }
  }

  function addEvent(sector, severity, description) {
    const timeStr = new Date().toLocaleTimeString("es-AR", { hour12: false });
    state.events.unshift({ time: timeStr, sector, severity, description });
    if (state.events.length > 40) state.events.pop();
    renderEvents();
  }

  function renderEvents() {
    if (!dom.eventsTableBody) return;
    if (state.events.length === 0) {
      dom.eventsTableBody.innerHTML = `<tr><td colspan="4" class="text-muted">No hay eventos recientes registrados.</td></tr>`;
      return;
    }
    const html = state.events.map(ev => {
      let badgeClass = "text-muted";
      if (ev.severity === "critical") badgeClass = "text-danger";
      if (ev.severity === "warning") badgeClass = "text-success";
      return `<tr>
        <td><code>${ev.time}</code></td>
        <td><strong>${ev.sector}</strong></td>
        <td><span class="${badgeClass}">${ev.severity.toUpperCase()}</span></td>
        <td>${ev.description}</td>
      </tr>`;
    }).join("");
    dom.eventsTableBody.innerHTML = html;
  }

  function updateTrafficLight(alarmSys) {
    const st = alarmSys.state || "DISARMED";
    const prev = state.systemState;
    state.systemState = st;
    state.armingRemainingS = Math.ceil(alarmSys.arming_remaining_s || 0);
    state.graceRemainingS = Math.ceil(
      alarmSys.grace_remaining_s !== undefined
        ? alarmSys.grace_remaining_s
        : 0
    );
    state.triggerReason = alarmSys.trigger_reason || 0;
    state.triggerSector = alarmSys.trigger_sector || 0;

    dom.trafficLight.className = "traffic-light-pill";
    dom.trafficLightState.textContent = st;

    if (st === "ARMING") {
      dom.trafficLight.classList.add("state-arming");
      dom.armingTimer.hidden = false;
      dom.armingTimer.textContent = `${state.armingRemainingS}s`;
      dom.armToggleBtn.textContent = "Cancelar Armado";
      if (dom.graceAlertBanner) dom.graceAlertBanner.hidden = true;
    } else if (st === "ARMED") {
      dom.trafficLight.classList.add("state-armed");
      dom.armingTimer.hidden = true;
      dom.armToggleBtn.textContent = "Desarmar Sistema";
      if (dom.graceAlertBanner) dom.graceAlertBanner.hidden = true;
    } else if (st === "GRACE") {
      dom.trafficLight.classList.add("state-grace");
      dom.trafficLightState.textContent = "PRE-ALARMA";
      dom.armingTimer.hidden = false;
      dom.armingTimer.textContent = `${state.graceRemainingS}s`;
      dom.armToggleBtn.textContent = "Cancelar / Desarmar";
      if (dom.graceAlertBanner) {
        const trigSec = state.sectors.find(s => s.id === state.triggerSector);
        const secLabel = trigSec ? trigSec.name : (state.triggerSector ? `Sector ${state.triggerSector}` : "sector activo");
        dom.graceAlertText.textContent =
          `Movimiento detectado en ${secLabel}. La sirena se activará en ${state.graceRemainingS}s.`;
        dom.graceAlertBanner.hidden = false;
      }
    } else if (st === "ALARM") {
      dom.trafficLight.classList.add("state-alarm");
      dom.armingTimer.hidden = true;
      dom.armToggleBtn.textContent = "Silenciar / Desarmar";
      if (dom.graceAlertBanner) dom.graceAlertBanner.hidden = true;
    } else {
      dom.trafficLight.classList.add("state-disarmed");
      dom.armingTimer.hidden = true;
      dom.armToggleBtn.textContent = "Armar Sistema";
      if (dom.graceAlertBanner) dom.graceAlertBanner.hidden = true;
    }

    if (st === "ALARM" && state.soundEnabled) {
      soundPlayer.start();
    } else {
      soundPlayer.stop();
    }

    if (prev !== st) {
      let sev = "info";
      if (st === "ALARM") sev = "critical";
      if (st === "ARMED" || st === "GRACE") sev = "warning";
      addEvent("Central", sev, `Estado de alarma transicionado a: ${st}`);
    }
  }

  function updateMapOverlays(sectors) {
    sectors.forEach(sec => {
      const g = document.getElementById(`mapSector-${sec.id}`);
      if (!g) return;

      const poly = g.querySelector(".sector-poly");
      if (!poly) return;

      poly.className = "sector-poly";

      if (!sec.enabled) {
        poly.classList.add("state-disabled");
      } else if (
        state.systemState === "ALARM" &&
        (sec.id === state.triggerSector || sec.motion || sec.panic || !sec.online)
      ) {
        poly.classList.add("state-alarm");
      } else if (
        state.systemState === "GRACE" &&
        (sec.id === state.triggerSector || sec.motion)
      ) {
        poly.classList.add("state-grace");
      } else if (state.systemState === "ARMED" || state.systemState === "GRACE") {
        poly.classList.add("state-armed");
      } else {
        poly.classList.add("state-active");
      }
    });
  }

  function renderInspector() {
    const sec = state.sectors.find(s => s.id === state.selectedSectorId);
    if (!sec) return;

    dom.inspectorTitle.textContent = sec.name;

    let chipClass = "normal";
    let chipText = "Normal";

    if (!sec.enabled) {
      chipClass = "disabled";
      chipText = "Deshabilitado";
    } else if (
      sec.panic ||
      (state.systemState === "ALARM" && (sec.id === state.triggerSector || sec.motion || !sec.online))
    ) {
      chipClass = "alarm";
      chipText = "Disparo Activo";
    } else if (
      state.systemState === "GRACE" &&
      (sec.id === state.triggerSector || sec.motion)
    ) {
      chipClass = "grace";
      chipText = `Pre-alarma (${state.graceRemainingS}s)`;
    } else if (state.systemState === "ARMED" || state.systemState === "GRACE") {
      chipClass = "armed";
      chipText = "Armado";
    }

    dom.inspectorStatusChip.className = `status-chip ${chipClass}`;
    dom.inspectorStatusChip.textContent = chipText;

    dom.inspectorSectorToggle.checked = sec.enabled;

    const subnodes = [
      {
        name: "Sensor de Movimiento / PIR",
        status_label: sec.motion ? "Presencia Detectada" : "Despejado",
        status_class: sec.motion ? "text-danger" : "text-success"
      }
    ];

    if (sec.id === 1 || sec.id === 3 || sec.id === 4) {
      subnodes.push({
        name: "Pulsador de Emergencia / Pánico",
        status_label: sec.panic ? "Activado" : "Normal",
        status_class: sec.panic ? "text-danger" : "text-muted"
      });
    }

    const tplData = {
      online: sec.online,
      motion: sec.motion,
      distance_cm: sec.distance_cm || 0,
      age_text: (sec.age_ms < 5000) ? "Hace instantes" : `${Math.round(sec.age_ms / 1000)} s`,
      subnodes
    };

    if (window.Mustache && dom.sectorInspectorTpl) {
      dom.inspectorContent.innerHTML = window.Mustache.render(dom.sectorInspectorTpl, tplData);
    }
  }

  async function fetchState() {
    try {
      const res = await apiFetch("/api/state");
      if (res.status === 401) {
        state.authenticated = false;
        soundPlayer.stop();
        if (dom.graceAlertBanner) dom.graceAlertBanner.hidden = true;
        dom.loginModal.setAttribute("open", "");
        return;
      }

      if (!res.ok) throw new Error(`HTTP ${res.status}`);

      const data = await res.json();
      state.authenticated = true;
      dom.loginModal.removeAttribute("open");

      dom.netStatusBadge.className = "badge";
      if (data.master_connected) {
        dom.netStatusBadge.textContent = "En Línea";
        dom.netStatusBadge.style.color = "#86efac";
      } else {
        dom.netStatusBadge.textContent = "Maestro Nano Desconectado";
        dom.netStatusBadge.style.color = "#fca5a5";
      }

      if (Array.isArray(data.sectors)) {
        state.sectors = data.sectors;
      }

      if (data.alarm_system) {
        updateTrafficLight(data.alarm_system);
      }

      if (Array.isArray(data.sectors)) {
        updateMapOverlays(data.sectors);
        renderInspector();
      }
    } catch (err) {
      dom.netStatusBadge.textContent = "Error de Enlace";
      dom.netStatusBadge.style.color = "#fca5a5";
    }
  }

  function setupEventListeners() {
    dom.loginForm.addEventListener("submit", ev => {
      ev.preventDefault();
      const val = dom.loginPassword.value.trim();
      if (val) doLogin(val);
    });

    dom.logoutBtn.addEventListener("click", () => doLogout());

    dom.armToggleBtn.addEventListener("click", () => toggleArmSystem());

    if (dom.graceDisarmBtn) {
      dom.graceDisarmBtn.addEventListener("click", () => setArmSystem(false));
    }

    dom.soundToggleBtn.addEventListener("click", () => {
      state.soundEnabled = !state.soundEnabled;
      if (state.soundEnabled) {
        dom.soundIcon.textContent = "🔊";
        dom.soundText.textContent = "Habilitado";
        if (state.systemState === "ALARM") soundPlayer.start();
      } else {
        dom.soundIcon.textContent = "🔇";
        dom.soundText.textContent = "Silenciado";
        soundPlayer.stop();
      }
    });

    document.querySelectorAll(".map-sector-group").forEach(el => {
      const secId = parseInt(el.getAttribute("data-sector-id"), 10);
      el.addEventListener("click", () => {
        state.selectedSectorId = secId;
        renderInspector();
      });
      el.addEventListener("keydown", ev => {
        if (ev.key === "Enter" || ev.key === " ") {
          ev.preventDefault();
          state.selectedSectorId = secId;
          renderInspector();
        }
      });
    });

    dom.inspectorSectorToggle.addEventListener("change", ev => {
      toggleSector(state.selectedSectorId, ev.target.checked);
    });

    dom.inspectorPingBtn.addEventListener("click", () => {
      pingSector(state.selectedSectorId);
    });
  }

  document.addEventListener("DOMContentLoaded", () => {
    setupEventListeners();
    fetchState();
    setInterval(fetchState, 1000);
  });
})();
