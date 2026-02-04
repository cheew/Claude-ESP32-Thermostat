# CLAUDE BRIEF: ESP32 thermostat web server security + MQTT safe control

You are implementing security and control hardening for an ESP32 thermostat that:
- already publishes telemetry to MQTT every 30 seconds
- receives control commands via MQTT (or will)
- also exposes a local web server (Tasmota-like) for UI and local API control

Primary goals:
1) Local web UI/API must NOT become a backdoor that bypasses cloud security.
2) Device must be safe even if credentials leak or attacker is on LAN.
3) Prepare architecture for product scale (per-device credentials, auditability, OTA safety).

Constraints:
- ESP32 (ESP-IDF or Arduino acceptable, prefer ESP-IDF)
- Web server exists already
- MQTT functionality exists already
- The Android app connects to device over Wi-Fi for setup and local control.

Deliverables:
- A concrete set of security features and enforcement rules implemented on the ESP32 web server and local API.
- Command schema and validation shared by both local API and MQTT command handler.
- Hard requirements for what endpoints exist, what they do, and how they are protected.
- Minimal but robust provisioning flow for MQTT credentials (no secrets exposed in UI).

## 1) Device modes and state machine
Implement explicit device security states:
- UNPROVISIONED
- PAIRING_WINDOW
- PROVISIONED

Rules:
- UNPROVISIONED: local AP or discovery allowed, but only exposes pairing endpoints and a minimal read-only status page.
- PAIRING_WINDOW: enabled only after physical action (button long press). Times out (e.g. 10 minutes). Only in this state may provisioning endpoints accept writes.
- PROVISIONED: pairing endpoints disabled. Config write endpoints require authentication and additional physical confirmation for sensitive actions.

Factory reset:
- Requires physical action.
- Clears stored credentials and returns to UNPROVISIONED.

## 2) Authentication for local web UI and local API
Local UI/API must require authentication in PROVISIONED state.

Implementation requirements:
- Default credentials are not allowed. Enforce "set password" during first provisioning.
- Store password hash (bcrypt if feasible, otherwise PBKDF2) and salt in NVS.
- Session auth via secure cookie:
  - HttpOnly
  - SameSite=Strict
  - short TTL (e.g. 1 hour)
- Add brute force mitigation:
  - lockout after N failed attempts with exponential backoff
- Provide a “local API token” for the Android app if needed:
  - mint a random token during pairing
  - store in NVS
  - token can be revoked
  - token never reveals MQTT credentials

If HTTPS is not feasible on device:
- Assume LAN attacker exists. Add defenses: auth, CSRF, physical gating for sensitive ops, no secrets in responses.

## 3) CSRF protection for all state-changing HTTP routes
Implement CSRF tokens for:
- all POST/PUT/DELETE routes
- any GET route that changes state (avoid these)

CSRF design:
- token bound to session
- include in headers `X-CSRF-Token` or form field
- reject missing/invalid token

CORS:
- default deny
- allow only same-origin
- do not allow wildcard origins

## 4) Endpoint hardening: what MUST and MUST NOT exist
### Required endpoints
Read-only (no auth in pairing mode, auth in provisioned):
- GET /api/status
  - device_id, firmware_version, uptime, wifi_rssi, mqtt_connected, last_telemetry_ts, current_temp, setpoint, mode, relay_state
- GET /healthz
  - simple ok

Auth endpoints:
- POST /api/login
- POST /api/logout

Provisioning endpoints (ONLY active in PAIRING_WINDOW):
- POST /api/pair/start
  - begins pairing handshake
- POST /api/pair/commit
  - writes provisioned values (see section 5)
- POST /api/pair/end
  - exits pairing window early

Control endpoints (PROVISIONED only; require auth + CSRF):
- POST /api/control/setpoint
- POST /api/control/mode
- POST /api/control/schedule
- POST /api/control/boost
All control endpoints must use the shared command schema and validation in section 7.

Sensitive admin endpoints (PROVISIONED only; require auth + CSRF + physical confirm):
- POST /api/admin/rotate_local_token
- POST /api/admin/enter_pairing_window (optional; if allowed, must require physical confirm)
- POST /api/admin/factory_reset (must require physical confirm)
- POST /api/admin/ota_start (if OTA via web)
- POST /api/admin/log_level (non-sensitive)

### Explicitly forbidden behavior
- Never expose MQTT username/password, tokens, or client cert material via any HTTP endpoint.
- Never allow changing MQTT broker host/creds via a normal UI route. Only allow via PAIRING_WINDOW + physical action.
- Never return Wi-Fi password.
- Never include secrets in logs in release builds.

## 5) Provisioning: storing MQTT credentials securely
Provisioning writes must be restricted to PAIRING_WINDOW.

Provisioned data model:
- broker_host
- broker_port (8883)
- broker_tls_ca_cert (or hash / pinned SPKI strategy)
- mqtt_client_id (device_id)
- auth method:
  - either mTLS (client cert + key)
  - or token username/password

Storage:
- store all secrets in NVS with encryption enabled
- enable flash encryption and secure boot for product-grade builds
- provide compile-time flags for dev vs prod (dev may skip secure boot)

Rotation:
- implement ability to rotate broker creds only via pairing window or a special secure admin flow requiring physical confirm.

## 6) MQTT connection security requirements (device side)
Even if MQTT exists, enforce:
- TLS on 8883
- validate server cert
- no insecure fallback
- unique credentials per device
- LWT configured:
  - publish offline status to /status topic

Topic model:
- base = `v1/tenants/<tenant_id>/devices/<device_id>`
- publish telemetry: `<base>/telemetry`
- publish status: `<base>/status` (retain latest status)
- subscribe commands: `<base>/cmd`
- publish responses: `<base>/rsp`

Device must only subscribe to its own cmd topic and only publish to its own topics.

## 7) Shared command schema and validation (HTTP + MQTT)
Implement a single internal command handler used by both:
- local HTTP API control endpoints
- MQTT cmd topic handler

Command envelope (JSON):
- schema_version: int
- command_id: string UUID
- timestamp_utc: int (epoch seconds)
- expires_at_utc: int (epoch seconds)
- sender_id: string ("local:<session_user>" or "cloud:<user_id/service>")
- type: string enum ("setpoint","mode","schedule","boost")
- nonce: string (random) OR monotonic counter
- payload: object

Validation rules:
- reject if expires_at_utc < now
- reject if timestamp skew too large (configurable) unless time sync reliable
- reject duplicate command_id (store recent IDs ring buffer)
- enforce safety rails:
  - setpoint min/max
  - max delta per minute
  - minimum time between relay toggles
  - safe mode transitions
- rate limit:
  - max X commands per minute per sender
  - max Y commands per minute total

Ack:
- publish response JSON to `<base>/rsp`:
  - command_id
  - result: "applied"|"rejected"|"error"
  - reason
  - applied_state snapshot
  - device_time_utc

QoS:
- commands: QoS 1
- telemetry: QoS 0 or 1 depending on requirements

Retained messages:
- telemetry retained optional for last-known dashboard
- commands must never be retained

## 8) Physical confirmation gate
For sensitive operations, require physical confirmation:
- factory reset
- entering pairing window (if possible)
- rotating local tokens
- changing MQTT configuration
- OTA start (optional)

Implementation:
- require user to press device button within a short time window after the HTTP request begins.
- server returns 202 "pending physical confirm"
- device enters "confirm needed" state for N seconds
- button press completes operation; timeout cancels.

## 9) Local network exposure controls
Implement:
- bind web server to LAN only. No port forwarding assumptions.
- UPnP must be disabled.
- optional: allow user to disable local API entirely after setup.

Discovery:
- If using mDNS, do not advertise in PROVISIONED unless enabled.
- Provide option to disable discovery.

## 10) Logging and debug
- Provide structured logs but never log secrets.
- Build modes:
  - DEBUG: verbose
  - RELEASE: minimal, no secrets, no stack traces to HTTP responses

HTTP errors:
- do not expose internal details
- use generic error codes and messages

## 11) OTA security (if OTA present)
- signed firmware only
- verify signature before applying
- rollback protection if possible
- HTTPS download and integrity check
- OTA endpoint must be protected by auth + CSRF + physical confirm

## 12) Test plan (must implement)
Create tests or a checklist for:
- cannot call provisioning endpoints outside PAIRING_WINDOW
- cannot setpoint without auth in PROVISIONED
- CSRF blocks state changes without token
- brute force lockout works
- factory reset requires button confirm
- MQTT creds never appear in HTTP responses
- replay command_id is rejected for both HTTP and MQTT
- expired commands rejected
- relay toggle rate limiting enforced
- LWT marks device offline

## 13) Output expectations
Produce:
- Endpoint list and JSON schemas
- State machine logic
- Pseudocode for auth, CSRF, physical confirm
- Concrete implementation approach for ESP-IDF (esp_http_server, mbedTLS, NVS encryption)
- Recommended compile-time flags and config constants
