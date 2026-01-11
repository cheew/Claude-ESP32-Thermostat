# Multi-Output Implementation Status

**Version:** 2.0.0 (in progress)
**Date:** January 11, 2026
**Status:** Core modules complete, ready for compilation testing

---

## ✅ Completed Components

### 1. Sensor Manager Module
**Files:** [include/sensor_manager.h](include/sensor_manager.h), [src/hardware/sensor_manager.cpp](src/hardware/sensor_manager.cpp)

**Features:**
- Discovers up to 6 DS18B20 sensors on OneWire bus
- Auto-generates unique ROM addresses (64-bit)
- User-friendly sensor naming (persistent in preferences)
- Individual sensor reading or bulk read all
- Error tracking per sensor
- Default names: "Temperature Sensor 1", "Temperature Sensor 2", etc.

**API:**
```cpp
sensor_manager_init(ONE_WIRE_BUS);
int count = sensor_manager_scan();
const SensorInfo_t* sensor = sensor_manager_get_sensor(index);
sensor_manager_read_all();
sensor_manager_set_name(index, "Basking Spot");
```

---

### 2. Output Manager Module
**Files:** [include/output_manager.h](include/output_manager.h), [src/control/output_manager.cpp](src/control/output_manager.cpp)

**Features:**
- 3 independent outputs with full control
- Hardware enforcement:
  - Output 1 (GPIO 5): AC Dimmer only → Lights
  - Output 2 (GPIO 14): SSR only → Heat devices
  - Output 3 (GPIO 32): SSR only → Heat devices
- Device type restrictions enforced:
  - Lights → Must use dimmer (Output 1)
  - Heat devices → Must use SSR (Outputs 2 & 3)
- 5 control modes per output:
  - Off, Manual, PID, On/Off thermostat, Schedule
- Per-output PID control with tunable parameters
- 6 schedule slots per output
- Sensor assignment (auto or manual)
- Persistent configuration (Preferences)

**Hardware Pin Assignments:**
```
Output 1: GPIO 5  (AC Dimmer PWM)
Output 2: GPIO 14 (SSR control)
Output 3: GPIO 32 (SSR control)
Zero-cross: GPIO 27 (shared for dimmer)
OneWire: GPIO 4 (DS18B20 sensors)
```

**API:**
```cpp
output_manager_init();
OutputConfig_t* output = output_manager_get_output(index);
output_manager_set_mode(index, CONTROL_MODE_PID);
output_manager_set_target(index, 30.0f);
output_manager_set_sensor(index, sensorAddress);
output_manager_update();  // Call in loop
```

---

### 3. Main.cpp Integration
**File:** [src/main.cpp](src/main.cpp)

**Changes:**
- Removed old single-output code (temp_sensor, dimmer_control, pid_controller, system_state)
- Integrated sensor_manager and output_manager
- Auto-assigns sensors to outputs on boot
- Legacy compatibility layer for TFT display (uses Output 1)
- Legacy compatibility for web interface (uses Output 1)
- MQTT callbacks updated (apply to Output 1)
- TFT touch callbacks updated (control Output 1)
- New helper functions:
  - `readSensors()` - Reads all DS18B20 sensors
  - `updateOutputs()` - Updates all 3 output controls
  - `updateLegacyState()` - Syncs Output 1 to legacy state

**Sensor Auto-Assignment:**
On boot, if sensors are discovered:
- Sensor 1 → Output 1
- Sensor 2 → Output 2
- Sensor 3 → Output 3

User can override via web interface (coming next).

---

## 🔧 Pending Implementation

### Next Steps (Priority Order):

#### 1. **Compilation Testing** (10 mins)
- Build project in VS Code/PlatformIO
- Fix any compilation errors
- Verify memory usage is acceptable

#### 2. **Web Interface Updates** (2-3 hours)
- **Home Page:**
  - Grid layout (3 columns on desktop, stack on mobile)
  - Each output shows: Name, temp, target, mode, power %
  - Independent controls per output
- **New: Sensor Management Page**
  - List all discovered sensors
  - Show current readings
  - Rename sensors
  - View ROM addresses
- **New: Output Configuration Page**
  - Enable/disable outputs
  - Set output names
  - Assign sensors (dropdown)
  - Set device type (lights, heat mat, etc.)
  - PID tuning per output
- **Schedule Page:**
  - Dropdown to select output (1, 2, or 3)
  - 6 time slots per output
  - "Copy to other outputs" button

#### 3. **API Endpoints** (1 hour)
```
GET  /api/sensors               - List all sensors
POST /api/sensor/{id}/name      - Rename sensor

GET  /api/outputs               - All outputs status
GET  /api/output/{id}           - Single output details
POST /api/output/{id}/config    - Configure output
POST /api/output/{id}/control   - Control output (mode, target, power)
POST /api/output/{id}/schedule  - Update schedule
```

#### 4. **MQTT Multi-Output** (1-2 hours)
- 3 separate Home Assistant climate entities
- Per-output topics:
  ```
  thermostat/output1/temperature
  thermostat/output1/setpoint
  thermostat/output1/mode
  thermostat/output1/power

  thermostat/output2/...
  thermostat/output3/...
  ```
- Auto-discovery for all 3 outputs
- Diagnostic sensors per output

#### 5. **Testing** (1-2 hours)
- Test sensor discovery
- Test output control (all modes)
- Test PID control per output
- Test scheduling per output
- Test web interface
- Test MQTT integration
- Test persistence (save/load config)

---

## 📊 Technical Details

### Memory Impact Estimates

**Current (v1.9.1):**
- Flash: 85.7% (1.11 MB)
- RAM: 19.8% (64 KB)

**Projected (v2.0.0):**
- Flash: +15 KB (sensor_manager + output_manager) = ~87%
- RAM: +8 KB (output configs + sensor array) = ~21%
- ✅ **Still safe** - plenty of headroom

### Data Structures

**SensorInfo_t** (per sensor, 6 max):
```cpp
typedef struct {
    bool discovered;
    uint8_t address[8];           // ROM address
    char addressString[17];       // Hex string
    char name[32];                // User name
    float lastReading;
    unsigned long lastReadTime;
    int errorCount;
} SensorInfo_t;
```

**OutputConfig_t** (per output, 3 total):
```cpp
typedef struct {
    bool enabled;
    char name[32];
    HardwareType_t hardwareType;
    DeviceType_t deviceType;
    uint8_t controlPin;
    ControlMode_t controlMode;
    char sensorAddress[17];
    float targetTemp;
    float currentTemp;
    int manualPower;
    int currentPower;
    bool heating;

    // PID
    float pidKp, pidKi, pidKd;
    float pidIntegral, pidLastError;
    unsigned long pidLastTime;

    // Schedule
    ScheduleSlot_t schedule[6];
} OutputConfig_t;
```

### Control Loop Timing

```
Every 2s:   Read all sensors
Every 100ms: Update all outputs (PID calculations)
Every 30s:   Publish MQTT status
```

Responsive control with 100ms update rate ensures smooth PID performance.

---

## 🧪 Testing Plan

### Phase 1: Basic Functionality
1. **Sensor Discovery**
   - Connect 1-3 DS18B20 sensors
   - Power on, check serial output
   - Verify sensors found and named
   - Check console page shows sensor events

2. **Single Output Test**
   - Test Output 1 (dimmer) only first
   - Set manual mode, verify dimmer responds
   - Set PID mode, verify heating control
   - Check temperature readings update

3. **Multi-Output Test**
   - Configure Output 2 and 3
   - Assign different sensors
   - Set different target temps
   - Verify all outputs control independently

### Phase 2: Web Interface
1. **Home Page**
   - Verify grid layout shows all 3 outputs
   - Test controls on each output
   - Verify status updates in real-time

2. **Sensor Management**
   - Verify sensor list displays
   - Test sensor renaming
   - Verify persistence after reboot

3. **Output Configuration**
   - Test output naming
   - Test sensor assignment
   - Test device type selection
   - Verify restrictions enforced (lights on dimmer only)

### Phase 3: Advanced Features
1. **Scheduling**
   - Configure schedule on Output 1
   - Verify activation at scheduled times
   - Test schedule on all 3 outputs
   - Verify independent operation

2. **PID Tuning**
   - Test PID parameter adjustment
   - Verify different PIDs per output
   - Monitor stability and overshoot

3. **MQTT Integration**
   - Verify 3 climate entities in Home Assistant
   - Test control from HA
   - Test status updates
   - Verify diagnostic sensors

### Phase 4: Edge Cases
1. **Sensor Failures**
   - Disconnect a sensor
   - Verify affected output handles gracefully
   - Verify other outputs continue normally

2. **Configuration Persistence**
   - Configure all outputs
   - Reboot ESP32
   - Verify all settings restored

3. **Memory Stability**
   - Run for extended period (24+ hours)
   - Monitor heap fragmentation
   - Verify no memory leaks

---

## 🐛 Known Limitations (Pre-Testing)

### Current Limitations:

1. **TFT Display:**
   - Only shows Output 1 status
   - Touch buttons only control Output 1
   - Multi-output display not yet implemented

2. **Web Interface:**
   - Legacy pages still show single output
   - New multi-output pages not yet created
   - Grid layout not yet implemented

3. **MQTT:**
   - Only publishes Output 1 status
   - Multi-output topics not yet created
   - Only 1 HA climate entity (not 3)

4. **Scheduling:**
   - Old scheduler module still referenced (unused)
   - Need to remove old scheduler code
   - Per-output schedules functional but not exposed in UI

### To Be Addressed:

- Remove old modules: pid_controller, system_state, scheduler, temp_sensor, dimmer_control
- Update platformio.ini if needed (should auto-detect new files)
- Test with actual hardware (AC dimmer + SSR)

---

## 📁 File Changes Summary

### New Files Created:
```
include/sensor_manager.h
src/hardware/sensor_manager.cpp
include/output_manager.h
src/control/output_manager.cpp
```

### Modified Files:
```
src/main.cpp (major refactor)
```

### Files to be Modified Next:
```
src/network/web_server.cpp (add multi-output pages & APIs)
src/network/mqtt_manager.cpp (add multi-output topics)
```

### Files to be Removed (deprecated):
```
include/temp_sensor.h
src/hardware/temp_sensor.cpp
include/dimmer_control.h
src/hardware/dimmer_control.cpp
include/pid_controller.h
src/control/pid_controller.cpp
include/system_state.h
src/control/system_state.cpp
include/scheduler.h
src/control/scheduler.cpp
```
*Note: Don't delete yet - wait until web interface updated*

---

## 🚀 Next Session Prep

### Quick Start Commands:

**Build:**
```bash
# In VS Code with PlatformIO
PlatformIO: Build
```

**Expected Output:**
```
RAM:   [==        ]  ~21% (used ~69 KB from 327 KB)
Flash: [========= ]  ~87% (used ~1.14 MB from 1.31 MB)
========================= [SUCCESS] =========================
```

**Upload and Monitor:**
```
PlatformIO: Upload
PlatformIO: Monitor
```

**Expected Serial Output:**
```
=== ESP32 Reptile Thermostat v2.0.0 ===
=== Multi-Output Environmental Control ===
[SensorMgr] Initialized
[SensorMgr] Scanning for DS18B20 sensors...
[SensorMgr] Sensor 0: 28FF1A2B3C4D5E6F (Temperature Sensor 1)
[SensorMgr] Found 1 sensors
[OutputMgr] Initializing...
[OutputMgr] Initialized 3 outputs
[SYSTEM] Output 1 auto-assigned to sensor: Temperature Sensor 1
[WiFi] Connecting to: mesh
[WiFi] Connected successfully
[WiFi] IP address: 192.168.1.236
=== Initialization Complete ===
Free heap: ~280000
```

### If Compilation Fails:

**Common Issues:**
1. **Missing headers:** Ensure all new files created
2. **Linker errors:** Check function names match between .h and .cpp
3. **Namespace conflicts:** Check no duplicate symbols
4. **Memory overflow:** Reduce buffer sizes if needed

**Quick Fixes:**
- Clean build: `PlatformIO: Clean`
- Rebuild: `PlatformIO: Build`
- Check platformio.ini (should auto-detect new files)

---

## 💡 Design Decisions Made

### 1. Hardware Restrictions
**Decision:** Enforce device types via hardware compatibility
**Rationale:** Prevents user error (e.g., plugging heat mat into dimmer)

### 2. Auto-Sensor Assignment
**Decision:** Auto-assign sensors 1:1 with outputs on boot
**Rationale:** Simplifies initial setup, still allows manual override

### 3. Legacy Compatibility Layer
**Decision:** Keep Output 1 compatible with existing TFT/web code
**Rationale:** Incremental migration, existing features keep working

### 4. Control Loop Frequency
**Decision:** 100ms update rate for outputs
**Rationale:** Fast enough for PID responsiveness, not too CPU-intensive

### 5. Per-Output Scheduling
**Decision:** Independent schedules, not global
**Rationale:** Maximum flexibility for different environmental needs

---

## 📝 Changelog Draft (v2.0.0)

```markdown
## [2.0.0] - 2026-01-11

### Added
- **Multi-Output Control**: 3 independent outputs
  - Output 1: Lights (AC dimmer)
  - Output 2 & 3: Heat devices (SSR)
- **Multi-Sensor Support**: Up to 6 DS18B20 sensors
- **Sensor Manager**: Auto-discovery and naming
- **Output Manager**: Per-output PID and scheduling
- **Hardware Restrictions**: Device type enforcement
- **Auto-Sensor Assignment**: Sensors assigned to outputs on boot

### Changed
- Refactored main.cpp to use output_manager
- Removed old single-output modules (PID, state, temp_sensor, dimmer)
- Control loop now updates all outputs every 100ms
- Sensor reading now handles multiple sensors

### Breaking Changes
- Old system_state API no longer available
- Old pid_controller API no longer available
- Preferences structure changed (will reset on upgrade)

### Migration Notes
- Existing settings will be lost on upgrade to v2.0.0
- Reconfigure device name, WiFi, MQTT after upgrade
- Reassign sensors to outputs
```

---

**Status:** Ready for compilation testing
**Next:** Build, test, fix any errors, then proceed with web interface updates

