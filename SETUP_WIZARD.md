# Setup Wizard - Feature Checklist

**Purpose:** Living document tracking all features/settings to include in the first-run setup wizard. Add to this list as new features are implemented.

**Entry conditions:**
- Auto-start if no valid config exists on first boot
- User can re-run from Settings page at any time

---

## Wizard Steps

### 1. Device Identity
- [ ] Device name (thermostat name displayed in header, MQTT, mDNS)
- [ ] Device location (optional, e.g. "Vivarium 1")
- [ ] UI mode preference (Simple / Advanced)

### 2. Network
- [ ] WiFi SSID + password (already exists, integrate into wizard flow)
- [ ] Timezone selection
- [ ] NTP server (default or custom)
- [ ] mDNS hostname

### 3. Security
- [ ] PIN protection (enable/disable, set 4-8 digit PIN)
  - Explain: "Protects the web interface with a PIN code. Recommended if your thermostat is accessible from outside your home network."
- [ ] HTTPS (future - self-signed certificate generation)
  - Explain: "Encrypts traffic between your browser and the thermostat. Requires accepting a browser warning on first use."
- [ ] LAN-only control option (`allow_control_from_wan`)
  - Explain: "When enabled, only devices on your local network can control the thermostat. External access is read-only."
- [ ] Session timeout (e.g. 1h, 8h, 24h, never)

### 4. Sensor Discovery
- [ ] Auto-scan OneWire bus for DS18B20 sensors
- [ ] Show live readings for each discovered sensor
- [ ] Assign friendly names to each sensor (e.g. "Hot End", "Cool End", "Ambient")
- [ ] Calibration offset per sensor (optional)

### 5. Output Configuration (per channel, x3)
- [ ] Output name (e.g. "Heat Mat", "Ceramic Heater", "Heat Lamp")
- [ ] Output type (AC Dimmer / SSR Pulse / Relay On-Off)
- [ ] Control mode (PID / Manual / On-Off / Schedule / Time-Proportional)
- [ ] Assign sensor to output
- [ ] Default target temperature
- [ ] Safe limits: max temp, min temp
- [ ] Fault mode (OFF / HOLD_LAST / CAP_POWER)
- [ ] Cap power percentage (if CAP_POWER selected)
- [ ] Quick test: pulse output for 2 seconds (with safety warning)

### 6. Schedule (optional, per output)
- [ ] Enable/disable scheduling
- [ ] Day/night temperature schedule
- [ ] Time slots configuration

### 7. MQTT Configuration
- [ ] Enable/disable MQTT
- [ ] **Local broker** settings:
  - Broker address (IP/hostname)
  - Port (default 1883)
  - Username + password
  - Base topic
- [ ] **Cloud broker** settings (future):
  - Cloud server address
  - Authentication token
  - User account linking
- [ ] Home Assistant auto-discovery (enable/disable)
- [ ] MQTT topic prefix

### 8. Display (TFT)
- [ ] Screen brightness
- [ ] Screen timeout / sleep
- [ ] Default display page
- [ ] Temperature unit (Celsius / Fahrenheit)

### 9. Notifications & Alerts (future)
- [ ] MQTT alert topics
- [ ] Temperature fault notifications
- [ ] Sensor disconnect alerts

### 10. Summary & Apply
- [ ] Review all settings on summary page
- [ ] "Apply" button saves config and reboots
- [ ] "Export config" option to download JSON backup
- [ ] "Import config" option to restore from backup

---

## Settings Available Outside Wizard (Settings Page)

These exist in the Settings page but should also be configurable in the wizard where appropriate:

- Dark mode preference (persisted in browser localStorage)
- Advanced/Simple UI mode toggle
- OTA firmware update
- Factory reset
- Diagnostics bundle download
- Event log viewing

---

## Notes
- Wizard should work on mobile browsers (responsive design)
- Each step should validate before allowing "Next"
- "Back" button to revisit previous steps
- Progress indicator showing current step
- Skip options for non-essential steps (Schedule, MQTT, Display)
