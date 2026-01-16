# Additions (Firmware + Web Server) — Professional/efficient upgrades

This file is a **build plan** for the thermostat firmware + web server so it feels professional, reliable, and easy to use. It is written to be actionable in VS Code.

## Goals
- Reliability under failure (sensor dropouts, WiFi/MQTT issues, reboot loops)
- Clear first-run setup (wizard) and guardrails (limits, validation)
- “App-like” UI (instant updates) without heavy polling
- Observability (health, events, diagnostics bundle)
- API stability (versioned endpoints, consistent responses) so the Android app can integrate cleanly
- Security baseline (rate limiting, sessions, local-only control option)

## Non-goals (for now)
- Public internet exposure (no DIY remote port-forwarding)
- Full enterprise auth (OAuth, JWT, etc.)
- Complex multi-user accounts (but we structure for it)

---

## A) Safety + reliability (must-have)
### A1. Sensor fault handling (per output) ✅ IMPLEMENTED v2.2.0
Implement a per-output sensor health state:
- `OK`
- `STALE` (no update within threshold)
- `ERROR` (invalid reading: -127, NaN, out-of-range, CRC failures, disconnected)

**Config (per output):**
- `fault_timeout_sec` (e.g. 10–30s)
- `fault_mode` (OFF / HOLD_LAST / CAP_POWER)
- `cap_power_pct` (if CAP_POWER)
- `max_temp_c` and `min_temp_c` hard limits
- `max_rise_rate_c_per_min` (optional)
- `min_rise_rate_c_per_min` (optional, for stuck heater detection)

**Behavior:**
- If sensor becomes `STALE`/`ERROR`, the output transitions to a **failsafe state**.
- Failsafe state is surfaced everywhere (TFT + web UI + API + events).
- Outputs should **not** resume automatically unless configured (`auto_resume_on_sensor_ok`) or user explicitly clears fault.

### A2. Independent hard cutoffs (per output) ✅ IMPLEMENTED v2.2.0
Hard cutoffs must override manual/PID/schedule:
- If `temp >= max_temp_c`: force output OFF and raise `OVER_TEMP` fault.
- If `temp <= min_temp_c` and configured as critical low: raise `UNDER_TEMP` warning or fault.
- These should work even if schedule wants heating.

### A3. Stuck heater detection (per output)
Detect two failure modes:
1) **No rise**: output is applying power but temp isn’t rising over N minutes.
2) **Runaway**: temp rises too fast or continues rising after power stops (thermal inertia threshold).

Create faults:
- `HEATER_NO_RISE`
- `HEATER_RUNAWAY`

### A4. Output ramping + anti-flicker
- Rate limit target changes (slider spam).
- Soft-start ramp: cap delta power per second (esp. SSR pulse / PWM).
- For on/off control: minimum on/off time to protect relays.

### A5. Watchdog + boot loop protection
- Enable watchdog reset.
- Track `boot_fail_count` and `last_reset_reason`.
- If N boots within short window → enter “SAFE MODE”:
  - networking minimal
  - outputs forced OFF
  - web UI indicates safe mode
  - allow config restore/rollback

### A6. Config persistence with rollback
- Persist `config_active.json` + `config_last_known_good.json`.
- On successful boot and stable run for X seconds, mark active as “known good”.
- If crash/reboot loop, restore last known good automatically.

---

## B) Setup wizard (first run experience)
### B1. Wizard state machine
Wizard steps (minimal):
1) Device identity (name, optional location)
2) WiFi join (already exists) + timezone + NTP status
3) Sensor discovery
   - list sensors with live readings
   - assign friendly names
4) Output configuration per channel
   - choose control type: PID / MANUAL / ON_OFF / SCHEDULED
   - assign sensor(s)
   - safe limits: min/max temp, fault mode
   - quick test: “pulse output 2s” (with prominent warning)
5) Optional MQTT/HA settings
6) Summary + “Apply” + “Export config”

**Entry conditions:**
- auto-start if no valid config
- user can re-run from Settings

**APIs required for wizard (see API section):**
- `GET /api/v1/wizard/status`
- `POST /api/v1/wizard/step`
- `POST /api/v1/wizard/apply`
- `POST /api/v1/wizard/reset`

---

## C) Observability (the “professional” multiplier)
### C1. Structured event log
Add a ring buffer event store (e.g., last 500–2000 events). Each event:
- `ts` (unix ms)
- `type` (WIFI, MQTT, SENSOR, OUTPUT, AUTH, SCHEDULE, SYSTEM)
- `code` (e.g., `WIFI_DISCONNECT`, `SENSOR_STALE`, `OVER_TEMP`)
- `severity` (INFO/WARN/ERROR)
- `output_id` (optional)
- `msg` (short)
- `data` (optional small JSON)

### C2. Health endpoint + health page ✅ PARTIALLY IMPLEMENTED v2.2.0
Expose:
- uptime
- heap / min heap
- loop timing / task overrun count
- sensor last update age
- WiFi RSSI, reconnect counts
- MQTT connected + reconnect counts
- NTP synced + last sync age
- flash usage + build info
- safe mode flag

### C3. Diagnostics bundle download (one click)
Generate a JSON bundle:
- `device_info`
- `status`
- `health`
- `config_active` (redact secrets)
- `events_tail` (last 200)
- `logs_tail` (optional, last N lines)

The web UI should provide a “Download diagnostics” button.

---

## D) Web UI polish + realtime updates
### D1. Realtime telemetry stream (replace heavy polling)
Implement Server-Sent Events (SSE) or WebSocket.

Recommended: **SSE** for simplicity.

- `GET /api/v1/stream` returns `text/event-stream`
- Push events:
  - `status` (lightweight snapshot or diff)
  - `telemetry` (temps/power, per output)
  - `event` (new structured log events)
  - `fault` (fault transitions)

**Client behavior:**
- connect on page load
- if disconnected, exponential backoff reconnect
- fall back to polling if SSE unsupported

### D2. UI guardrails
- Validate target changes against safe limits client-side + server-side.
- Show warnings, not cryptic errors.
- Provide quick actions: OFF, BOOST 30m, RESUME SCHEDULE.
- Show “fault chips” per output (Heating/Idle/Fault/Scheduled).

### D3. Performance basics
- Cache headers for static assets.
- Optional gzip for assets if feasible.
- Avoid full-page rerenders for minor updates.

---

## E) API upgrades (versioned, consistent, app-friendly)
### E1. Versioned routes
Add `v1` namespace:
- Keep existing `/api/...` for backward compatibility.
- Implement new endpoints under `/api/v1/...`.

### E2. Consistent JSON envelopes
For all JSON responses:
- Success: `{ "ok": true, "data": ... }`
- Error: `{ "ok": false, "error": { "code": "SOME_CODE", "message": "Human text", "detail": {...} } }`

### E3. Core endpoints (v1)
#### Status/health/telemetry
- `GET /api/v1/status`
- `GET /api/v1/health`
- `GET /api/v1/device`
- `GET /api/v1/events?limit=200&since=<ts_ms>`
- `GET /api/v1/stream` (SSE)

#### Outputs
- `GET /api/v1/outputs`
- `GET /api/v1/outputs/{id}`
- `POST /api/v1/outputs/{id}/control`
  - body: `{ "mode": "PID|MANUAL|OFF|SCHEDULE", "target_c": 28.0, "power_pct": 40 }`
  - server validates mode/fields by mode
- `POST /api/v1/outputs/{id}/config`
  - body includes: name, sensor assignment, PID params, limits, fault policies

#### Sensors
- `GET /api/v1/sensors`
- `POST /api/v1/sensors/{id}/config` (name, calibration offsets, smoothing)

#### Schedule
- `GET /api/v1/schedules`
- `POST /api/v1/schedules/{output_id}`
- `POST /api/v1/schedules/{output_id}/enable`

#### Wizard
- `GET /api/v1/wizard/status`
- `POST /api/v1/wizard/step`
- `POST /api/v1/wizard/apply`
- `POST /api/v1/wizard/reset`

#### Diagnostics + config portability
- `GET /api/v1/diagnostics` (returns JSON bundle)
- `GET /api/v1/config/export` (downloads config JSON)
- `POST /api/v1/config/import` (uploads config JSON, validates, applies, reboots if needed)

#### Auth/security
- `POST /api/v1/auth/login`
- `POST /api/v1/auth/logout`
- `GET /api/v1/auth/session`
- Rate limiting for login attempts.

### E4. Redaction rules
Any endpoint returning config must redact:
- WiFi password
- MQTT password/token
- any future secrets

---

## F) Security baseline
### F1. Rate limiting + cooldown
- e.g., 5 failed logins → 60s cooldown.
- Log auth events to structured events.

### F2. Session handling
- Session token stored in cookie or header.
- Session expiry (e.g., 24h) with sliding refresh.

### F3. LAN-only control option
Config: `allow_control_from_wan=false`
- If enabled, reject control endpoints unless request is from LAN subnet.
- Always allow status/health read-only, optionally.

---

## G) Flash pressure (practical constraint)
Current flash usage is high, so add build profiles:
- `MINIMAL`: no history graphs, no wizard, no diagnostics
- `STANDARD`: wizard + diagnostics + SSE
- `FULL`: history graphs + extra pages

Also:
- Reduce log verbosity in release builds.
- Consider serving assets from LittleFS and gzip them offline.

---

## Acceptance checklist (definition of done)
- [x] Output enters failsafe on sensor stale/error, visible on TFT + web + API *(v2.2.0)*
- [x] Over-temp cutoff overrides all modes *(v2.2.0)*
- [ ] Stuck heater faults trigger and are logged
- [ ] Boot loop triggers safe mode with outputs off
- [ ] Wizard can configure device end-to-end without editing JSON
- [x] `/api/v1/health` implemented *(v2.2.0)*
- [ ] `/api/v1/events`, `/api/v1/diagnostics`, `/api/v1/stream` implemented
- [ ] Android app can: live view, control outputs, show faults, download diagnostics
- [ ] Config export/import works and validates schema
- [x] Login rate limiting + session expiry works *(basic in v2.1.1)*
