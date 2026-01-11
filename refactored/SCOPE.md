# ESP32 Thermostat - Future Features & Roadmap

**Document Version:** 1.0
**Last Updated:** January 11, 2026
**Current Project Version:** v1.9.0

---

## 🎯 Project Vision

Transform the ESP32 thermostat from a single-output heating controller into a **comprehensive reptile habitat environmental control system** supporting:
- Multiple heating/lighting outputs
- Multiple temperature sensors
- Flexible output types (AC dimming, SSR pulse, relay on/off)
- Guided setup wizard for first-time configuration
- Professional Android app for remote management

---

## 🔧 Priority 1: Multi-Output Environmental Control

### Overview
Expand from single heating output to **3 independent outputs**, each with:
- Dedicated temperature sensor assignment
- Flexible control hardware (AC dimmer, SSR, or relay)
- Appropriate control modes based on hardware type
- Independent scheduling per output

### Hardware Support Matrix

| Output | Hardware Options | Control Modes Available | Typical Use Cases |
|--------|------------------|------------------------|-------------------|
| **Output 1** | AC Dimmer | PID, Pulse Width, On/Off, Schedule | Ceramic heat emitter, heat cable |
| **Output 2** | SSR (pulse control) | Pulse Width, On/Off, Schedule | Heat mat, under-tank heater |
| **Output 3** | SSR (on/off) | On/Off, Schedule | Heat lamp, basking light |

### Sensor Configuration
- **3x DS18B20 sensors supported**
- User assigns sensor to output during setup
- Default mapping: Sensor 1 → Output 1, Sensor 2 → Output 2, Sensor 3 → Output 3
- Alternative: Sensor selection dropdown per output
- Support for shared sensor (multiple outputs use same sensor)

### Technical Requirements

**Hardware Interface:**
```
Output 1 (AC Dimmer):
- PWM Pin: GPIO 5 (current)
- Zero-cross Pin: GPIO 27 (shared)
- Control: 0-100% AC phase control

Output 2 (SSR Pulse):
- Control Pin: GPIO 14
- PWM: Slow pulse (1-60 second cycle)
- Example: 50% = 30s ON, 30s OFF

Output 3 (SSR/Relay On/Off):
- Control Pin: GPIO 32
- Digital: HIGH/LOW only
- Example: Thermostat mode (on when temp < target)
```

**Temperature Sensors:**
```
DS18B20 OneWire Bus (GPIO 4):
- Supports multiple sensors on same bus
- Each sensor has unique 64-bit ROM address
- Scan and enumerate on startup
- Store sensor addresses in preferences
- User-friendly naming: "Basking Spot", "Cool Hide", "Ambient"
```

**Configuration Storage:**
```cpp
// Per-output configuration (stored in Preferences)
struct OutputConfig {
    bool enabled;
    char name[32];                    // "Basking Lamp", "Heat Mat"
    uint8_t hardwareType;             // DIMMER, SSR_PULSE, SSR_ONOFF
    uint8_t controlPin;
    uint8_t controlMode;              // PID, PULSE, ONOFF, SCHEDULE
    char sensorAddress[17];           // DS18B20 ROM address (hex string)
    float targetTemp;
    // PID/Pulse parameters
    // Schedule data
};
```

### Web Interface Changes

**Home Page:**
- Split into 3 output sections (tabs or accordion)
- Each shows: Current temp, target, mode, power %
- Independent control per output

**Schedule Page:**
- Dropdown to select output (1, 2, or 3)
- 6 time slots per output (stored separately)
- Copy schedule between outputs button

**Settings Page:**
- New "Output Configuration" section
- Per-output settings:
  - Enable/disable output
  - Name output
  - Hardware type dropdown
  - Assign sensor (dropdown of discovered sensors)
  - PID tuning parameters (if applicable)

**New: Sensor Management Page**
- List all discovered DS18B20 sensors
- Show current temperature from each
- Assign friendly names
- Test individual sensor readings
- Diagnostic info (ROM address, last read time, error count)

### MQTT Extensions

**New Topics:**
```
homeassistant/climate/thermostat_output1/config
homeassistant/climate/thermostat_output2/config
homeassistant/climate/thermostat_output3/config

thermostat/output1/temperature
thermostat/output1/setpoint
thermostat/output1/mode
thermostat/output1/power

(repeat for output2, output3)
```

**Home Assistant Integration:**
- 3 separate climate entities
- Each with own temperature sensor
- Independent mode control
- Diagnostic sensors per output (power %, uptime, errors)

---

## 🧙 Priority 2: Setup Wizard

### Overview
Guided first-run setup wizard to configure:
1. Device name and WiFi credentials
2. MQTT broker settings (optional)
3. Sensor discovery and naming
4. Output hardware configuration
5. Initial temperature targets
6. Verification and test

### Implementation Approach

**Trigger Conditions:**
- No saved WiFi credentials (first boot)
- User manually enters setup mode (web button)
- Factory reset command

**Wizard Flow:**

```
┌─────────────────────────────────────┐
│  Step 1: Welcome & Device Name      │
│  - Enter friendly name              │
│  - Brief overview of wizard         │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 2: WiFi Configuration         │
│  - Scan for networks                │
│  - Select SSID or enter manually    │
│  - Enter password                   │
│  - Test connection                  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 3: MQTT Setup (Optional)      │
│  - Skip or configure                │
│  - Broker address                   │
│  - Credentials (if required)        │
│  - Test connection                  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 4: Sensor Discovery           │
│  - Scan OneWire bus                 │
│  - Display found sensors            │
│  - Name each sensor                 │
│  - Verify readings                  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 5: Output Configuration       │
│  - For each output (1, 2, 3):       │
│    - Enable/disable                 │
│    - Name output                    │
│    - Select hardware type           │
│    - Assign sensor                  │
│    - Set initial target temp        │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 6: Verification & Test        │
│  - Summary of configuration         │
│  - Test each output (manual trigger)│
│  - Confirm and save                 │
│  - Option to restart wizard         │
└──────────────┬──────────────────────┘
               │
               ▼
          [ Complete! ]
```

**Technical Implementation:**

```cpp
// Setup wizard state machine
enum WizardState {
    WIZARD_WELCOME,
    WIZARD_WIFI,
    WIZARD_MQTT,
    WIZARD_SENSORS,
    WIZARD_OUTPUTS,
    WIZARD_VERIFY,
    WIZARD_COMPLETE
};

// Web routes
server.on("/setup", handleSetupWizard);
server.on("/api/setup/networks", handleWiFiScan);
server.on("/api/setup/sensors", handleSensorScan);
server.on("/api/setup/test-output", handleOutputTest);
server.on("/api/setup/save", handleSetupSave);
```

**User Experience:**
- Progress indicator (Step 1 of 6)
- Previous/Next navigation
- Skip optional steps (MQTT, unused outputs)
- Validation on each step before proceeding
- Ability to go back and change settings
- Auto-save to preferences on completion
- Automatic reboot and connection test

---

## 📱 Priority 3: Android App Development

### Overview
Professional Android app for remote thermostat management:
- **Device Discovery:** mDNS/Bonjour auto-discovery on local network
- **Multi-Device Support:** Manage multiple thermostats (multiple reptile enclosures)
- **Real-Time Monitoring:** Live temperature readings and status
- **Quick Controls:** Adjust targets, change modes
- **Notifications:** Alerts for temperature warnings
- **Dark Mode:** Match system theme

### Features

**Core Functionality:**
1. **Auto-Discovery**
   - Scan local network for thermostats using mDNS
   - Display list of found devices with names
   - Add manually by IP address if discovery fails
   - Save discovered devices for quick access

2. **Dashboard**
   - Grid/list view of all managed devices
   - Current temp, target temp, mode
   - Status indicators (heating, connected, errors)
   - Quick tap to open device detail

3. **Device Detail View**
   - Real-time temperature graph (last 24 hours)
   - Current status and power output
   - Quick adjust target temperature (slider or +/- buttons)
   - Mode switcher (Auto, Manual On/Off, Schedule)
   - Access to full schedule editor
   - View logs and console

4. **Settings Management**
   - Edit device name
   - Configure WiFi and MQTT from app
   - PID tuning interface
   - Access to all web interface features

5. **Notifications**
   - Temperature alerts (too high/low)
   - Connection loss warnings
   - Schedule activation reminders
   - Option to configure per-device

### Technical Stack

**Android (Kotlin):**
```
Dependencies:
- Jetpack Compose (UI)
- Retrofit + OkHttp (REST API calls)
- Paho MQTT Android (real-time updates)
- JmDNS (mDNS discovery)
- MPAndroidChart (temperature graphing)
- Room Database (local device storage)
- WorkManager (background updates)
```

**Communication Methods:**
1. **REST API (Primary):**
   - Use existing `/api/*` endpoints
   - Polling for status updates (every 5 seconds)
   - Control commands via POST

2. **MQTT (Optional):**
   - Subscribe to status topics for real-time updates
   - Requires MQTT broker accessible from phone (local or cloud)

3. **WebSocket (Future Enhancement):**
   - Add WebSocket endpoint to ESP32
   - Push updates to app instantly
   - More efficient than polling

**mDNS Discovery:**
```kotlin
// Android mDNS service discovery
val serviceType = "_http._tcp.local."
val txtQuery = "type=reptile-thermostat"

// ESP32 advertises:
// Service: _http._tcp
// Port: 80
// TXT Records:
//   - type=reptile-thermostat
//   - version=1.9.0
//   - name=Havoc's Thermostat
//   - outputs=3
```

### Development Phases

**Phase 1: MVP (Minimum Viable Product)**
- [x] mDNS device discovery
- [x] Add device manually by IP
- [x] Dashboard showing all devices
- [x] Device detail with current status
- [x] Adjust target temperature
- [x] View temperature graph
- [x] Basic error handling

**Phase 2: Enhanced Controls**
- [x] Mode switching (Auto/Manual/Schedule)
- [x] Schedule viewer
- [x] Schedule editor
- [x] Settings access
- [x] Dark mode support

**Phase 3: Advanced Features**
- [x] Push notifications
- [x] MQTT real-time updates
- [x] Multi-output support (3 outputs)
- [x] Sensor management from app
- [x] Background monitoring
- [x] Widget for home screen

**Phase 4: Polish**
- [x] Material Design 3
- [x] Animations and transitions
- [x] Offline mode (cache last known state)
- [x] App settings (update frequency, notification preferences)
- [x] Help/tutorial screens

### API Requirements for App

The ESP32 firmware must provide:

**Current APIs (Already Available):**
- ✅ `/api/info` - Device information
- ✅ `/api/logs` - System logs
- ✅ `/api/history` - Temperature history
- ✅ `/api/control` - Control endpoint (mode, target, power)

**New APIs Needed:**
```
GET /api/status - Quick status (all outputs)
{
  "outputs": [
    {
      "id": 1,
      "name": "Basking Lamp",
      "enabled": true,
      "temp": 32.5,
      "target": 35.0,
      "mode": "auto",
      "power": 75,
      "sensor": "Basking Spot"
    },
    // ... output 2, 3
  ]
}

POST /api/output/{id}/control - Per-output control
{
  "target": 35.0,
  "mode": "auto"
}

GET /api/sensors - List all sensors
POST /api/sensors/{address}/name - Rename sensor
```

---

## 🐛 Known Issues to Address

### 1. WiFi Reconnection Bug

**Issue:** WiFi does not automatically reconnect after connection loss. Device stays in AP mode even when WiFi becomes available.

**Current Behavior:**
- WiFi disconnects (e.g., router reboot)
- Device switches to AP mode
- When WiFi returns, device stays in AP mode
- Requires power cycle to reconnect

**Root Cause:**
Looking at [wifi_manager.cpp:60-86](wifi_manager.cpp#L60-L86), the `wifi_task()` function has a flaw:

```cpp
void wifi_task(void) {
    // Don't reconnect if in AP mode
    if (apMode) {
        return;  // ← PROBLEM: Never exits AP mode!
    }
    // ... reconnection logic
}
```

**Fix Required:**
```cpp
void wifi_task(void) {
    // In AP mode: periodically check if saved WiFi is available
    if (apMode) {
        if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
            // Try to reconnect to saved WiFi
            Serial.println("[WiFi] AP mode: Attempting to reconnect to WiFi");
            wifi_connect(NULL, NULL);  // Will switch out of AP mode if successful
        }
        return;
    }

    // Normal reconnection logic when not in AP mode
    if (WiFi.status() != WL_CONNECTED) {
        if (currentState != WIFI_STATE_DISCONNECTED) {
            Serial.println("[WiFi] Connection lost");
            currentState = WIFI_STATE_DISCONNECTED;
        }

        if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
            Serial.println("[WiFi] Attempting reconnection...");
            wifi_connect(NULL, NULL);
        }
    }
    // ...
}
```

**Additional Improvements:**
- Add user setting: "AP mode timeout" (default: stay in AP permanently, or set minutes)
- Add web button in AP mode: "Retry WiFi Connection"
- Add visual indicator on home page showing AP mode vs normal WiFi
- Log WiFi reconnection attempts to console

**Testing Plan:**
1. Connect device to WiFi
2. Disable WiFi router
3. Wait for device to enter AP mode
4. Re-enable WiFi router
5. Verify device automatically reconnects within 30 seconds
6. Check console log for reconnection attempts

---

## 📊 Technical Considerations

### Memory Usage Estimates

**Current Usage (v1.9.0):**
- Flash: 85.7% (~1.11 MB / 1.31 MB)
- RAM: 19.8% (~64 KB / 327 KB)

**Multi-Output Addition (estimated):**
- Flash: +10 KB (duplicate control logic × 3)
- RAM: +2 KB per output (state data × 3) = +6 KB total
- **Projected:** Flash 89%, RAM 21% ✅ **Safe**

**Setup Wizard (estimated):**
- Flash: +15 KB (HTML pages, wizard state machine)
- RAM: +1 KB (wizard state)
- **Projected:** Flash 92%, RAM 22% ✅ **Safe**

**Android App:**
- No impact on ESP32 (uses existing APIs)

**Total Projected:**
- Flash: ~92% (safe, allows room for future updates)
- RAM: ~22% (plenty of headroom)

### Performance Impact

**Temperature Reading:**
- Current: 1 sensor every 2 seconds
- Multi-sensor: 3 sensors every 2 seconds
- DS18B20 read time: ~750ms per sensor (sequential)
- **Solution:** Stagger reads (sensor 1 at 0s, sensor 2 at 1s, sensor 3 at 2s)

**PID Calculation:**
- Current: 1 PID loop @ 1 Hz
- Multi-output: 3 PID loops @ 1 Hz
- PID calc time: <1ms each
- **Impact:** Negligible

**Web Interface:**
- Current: Single output status update
- Multi-output: 3× data in JSON response
- **Impact:** Minimal (gzipped JSON, still <1 KB)

### Development Complexity

**Low Complexity:**
- ✅ WiFi reconnection fix (1 hour)
- ✅ Sensor management page (2-3 hours)

**Medium Complexity:**
- 🟡 Multi-output control (8-10 hours)
  - Duplicate existing logic × 3
  - Refactor web pages for tabs/accordion
  - Update MQTT topics
  - Preferences storage schema

**High Complexity:**
- 🔴 Setup wizard (20-25 hours)
  - Multi-step state machine
  - Network scanning APIs
  - Sensor enumeration
  - Comprehensive testing

- 🔴 Android app (80-120 hours)
  - mDNS discovery
  - REST API integration
  - UI/UX design and implementation
  - Testing on multiple devices

---

## 🗓️ Recommended Implementation Order

### Phase 1: Foundation Fixes
1. **Fix WiFi reconnection bug** (1 hour)
   - High user impact
   - Critical for reliability
   - Simple fix

2. **Add sensor management page** (3 hours)
   - Required for multi-output
   - Useful even with single output
   - Lays groundwork for setup wizard

### Phase 2: Multi-Output Support
3. **Implement 3-output system** (10 hours)
   - Core feature expansion
   - Enables professional reptile setups
   - Most requested feature

4. **Update MQTT for multi-output** (2 hours)
   - Maintain Home Assistant integration
   - 3 separate climate entities

### Phase 3: User Experience
5. **Setup wizard** (25 hours)
   - Dramatically improves first-run experience
   - Reduces support burden
   - Makes product more professional

### Phase 4: Remote Management
6. **Android app development** (100+ hours)
   - Significant time investment
   - High user value
   - Enables remote management
   - Opens door to monetization (premium features)

### Phase 5: Polish & Optimization
7. **Performance tuning** (5 hours)
   - Optimize multi-sensor reads
   - Reduce memory usage where possible
   - Improve web page load times

8. **Documentation & guides** (8 hours)
   - User manual
   - Setup guide with photos
   - Troubleshooting guide
   - Video tutorials

---

## 💡 Future Enhancements (Post-Roadmap)

### Hardware Additions
- **Humidity Control:**
  - DHT22 or SHT31 sensors
  - Misting system control (relay or SSR)
  - Humidity scheduling

- **Lighting Control:**
  - RGB LED strips (PWM control)
  - Sunrise/sunset simulation
  - UVB timer management

- **Remote Sensors:**
  - ESP-NOW for wireless sensors
  - Multiple enclosure monitoring from single controller

### Software Features
- **Cloud Integration:**
  - Remote access without local network
  - Data logging to cloud service
  - Mobile app notifications over internet

- **Machine Learning:**
  - Adaptive PID tuning based on thermal mass
  - Predictive scheduling (adjust heat before scheduled time)
  - Anomaly detection (alert if temp doesn't stabilize)

- **Data Export:**
  - CSV export of temperature logs
  - PDF reports (daily/weekly/monthly)
  - Integration with Google Sheets

### Advanced Features
- **Multi-Zone System:**
  - 6+ outputs for commercial breeders
  - Hierarchical control (room ambient + individual enclosures)
  - Modbus or RS485 expansion

- **Voice Control:**
  - Amazon Alexa integration (via HA or custom skill)
  - Google Assistant support
  - Custom voice commands

---

## 📞 Support & Collaboration

**Questions or suggestions?**
- Open an issue on GitHub
- Discussion board for feature requests
- Pull requests welcome!

**Contributors:**
- Looking for Android developers
- Looking for UI/UX designers
- Documentation writers needed

---

**Document Status:** Living document - updated as features are implemented and new ideas emerge.

**Next Review:** After Phase 2 completion (multi-output system)
