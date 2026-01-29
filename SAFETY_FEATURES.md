# Safety Features Documentation

**ESP32 Multi-Output Thermostat** | **Version:** 2.2.0-dev

This document describes all safety mechanisms, fault detection, and protective features implemented in the thermostat firmware. Given the critical nature of heating control for reptile habitats, safety is a primary design consideration.

---

## Table of Contents

1. [Safety Architecture Overview](#safety-architecture-overview)
2. [Sensor Fault Detection](#sensor-fault-detection)
3. [Temperature Safety Limits](#temperature-safety-limits)
4. [Fault Response Modes](#fault-response-modes)
5. [PID Control Safety](#pid-control-safety)
6. [Output Power Protection](#output-power-protection)
7. [Communication Failure Handling](#communication-failure-handling)
8. [Bounds & Validation Checks](#bounds--validation-checks)
9. [Event Logging & Diagnostics](#event-logging--diagnostics)
10. [Configuration Persistence](#configuration-persistence)
11. [Per-Output Isolation](#per-output-isolation)
12. [Future Work / Planned Features](#future-work--planned-features)

---

## Safety Architecture Overview

The thermostat implements a **defense-in-depth** approach with multiple independent safety layers:

```
┌─────────────────────────────────────────────────────────────┐
│                    SAFETY LAYERS                            │
├─────────────────────────────────────────────────────────────┤
│  Layer 1: Sensor Validation (CRC, range, staleness)         │
│  Layer 2: Temperature Hard Limits (max/min cutoffs)         │
│  Layer 3: Fault Mode Response (OFF / HOLD / CAP)            │
│  Layer 4: PID Anti-Windup & Output Clamping                 │
│  Layer 5: Per-Output Isolation (independent fault states)   │
│  Layer 6: Communication Fallback (AP mode, MQTT retry)      │
│  Layer 7: Event Logging (diagnostics & audit trail)         │
└─────────────────────────────────────────────────────────────┘
```

**Key Principle:** Each output operates independently. A sensor failure on Output 1 does not affect Outputs 2 or 3.

---

## Sensor Fault Detection

### Temperature Range Validation
**Location:** [sensor_manager.cpp:271-279](src/hardware/sensor_manager.cpp#L271-L279)

All temperature readings are validated before use:

| Check | Condition | Result |
|-------|-----------|--------|
| Disconnected sensor | `temp == -127.0°C` | Reading rejected |
| Out of range | `temp < -50°C` or `temp > 100°C` | Reading rejected |
| NaN value | `isnan(temp)` | Reading rejected |

```cpp
bool sensor_manager_is_valid_temp(float temp) {
    if (temp == DEVICE_DISCONNECTED_C) return false;  // -127
    if (temp < -50.0f || temp > 100.0f) return false;
    return true;
}
```

### CRC Checksum Verification
**Location:** [sensor_manager.cpp:65-68](src/hardware/sensor_manager.cpp#L65-L68)

OneWire CRC8 validation ensures data integrity from DS18B20 sensors:
- Invalid CRC results in sensor skip
- Prevents corrupted readings from bus noise or interference

### Stale Data Detection
**Location:** [output_manager.cpp:760-772](src/control/output_manager.cpp#L760-L772)

Each output tracks the last valid sensor reading time:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `faultTimeoutSec` | 30 seconds | Time without valid reading before fault |

**Trigger:** If `(currentTime - lastValidReadTime) > faultTimeoutSec`, the output enters `FAULT_SENSOR_STALE` state.

### Sensor Health States
**Location:** [output_manager.h:22-26](include/output_manager.h#L22-L26)

```cpp
typedef enum {
    SENSOR_OK = 0,      // Normal operation
    SENSOR_STALE,       // No update within timeout
    SENSOR_ERROR        // Invalid reading detected
} SensorHealth_t;
```

### Error Count Tracking
**Location:** [sensor_manager.h:28](include/sensor_manager.h#L28)

Per-sensor error counters track read failures, enabling:
- Identification of unreliable sensors
- Diagnostics via API (`GET /api/v1/health`)

---

## Temperature Safety Limits

### Hard Cutoff Protection
**Location:** [output_manager.cpp:790-836](src/control/output_manager.cpp#L790-L836)

Each output has independent temperature boundaries:

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `maxTempC` | 40.0°C | 0-100°C | Over-temperature cutoff |
| `minTempC` | 5.0°C | 0-100°C | Under-temperature warning |

### Over-Temperature Response
**Location:** [output_manager.cpp:799-807](src/control/output_manager.cpp#L799-L807)

When `currentTemp >= maxTempC`:
1. **Immediate power cutoff** - Output forced to 0%
2. **Fault state set** - `FAULT_OVER_TEMP`
3. **Event logged** - Timestamped record created
4. **Cannot auto-clear** - Requires manual reset after temp drops

**Critical:** Over-temperature ALWAYS forces power OFF regardless of fault mode setting.

### Under-Temperature Response
**Location:** [output_manager.cpp:811-820](src/control/output_manager.cpp#L811-L820)

When `currentTemp <= minTempC`:
1. **Fault state set** - `FAULT_UNDER_TEMP`
2. **Configured fault mode applied** (OFF/HOLD/CAP)
3. **Event logged**

### Hysteresis Protection
**Location:** [output_manager.cpp:824-835](src/control/output_manager.cpp#L824-L835)

To prevent rapid on/off cycling at boundaries:
- **Temperature limits:** 1.0°C hysteresis required to clear fault
- **On/Off control:** 0.5°C hysteresis band

Example: If `maxTempC = 40°C` and over-temp triggers, fault won't clear until temp drops to 39°C.

---

## Fault Response Modes

### Configurable Per-Output Behavior
**Location:** [output_manager.h:31-35](include/output_manager.h#L31-L35)

Each output can be configured to respond differently to faults:

```cpp
typedef enum {
    FAULT_MODE_OFF = 0,      // Safest - immediate power cutoff
    FAULT_MODE_HOLD_LAST,    // Maintain last known-good power
    FAULT_MODE_CAP_POWER     // Limit to reduced power level
} FaultMode_t;
```

### FAULT_MODE_OFF (Default)
**Location:** [output_manager.cpp:854-858](src/control/output_manager.cpp#L854-L858)

- **Behavior:** Immediately cuts power to 0%
- **Use case:** Safety-critical outputs, overnight operation
- **Risk:** Habitat may cool if sensor has temporary glitch

### FAULT_MODE_HOLD_LAST
**Location:** [output_manager.cpp:860-865](src/control/output_manager.cpp#L860-L865)

- **Behavior:** Maintains the last power level before fault
- **Use case:** Minimize temperature swings during brief sensor issues
- **Risk:** If heater was at high power, may overheat
- **Mitigation:** Over-temp limit still enforced (when sensor recovers)

### FAULT_MODE_CAP_POWER
**Location:** [output_manager.cpp:867-872](src/control/output_manager.cpp#L867-L872)

- **Behavior:** Limits power to configurable percentage
- **Default cap:** 30% (`capPowerPct`)
- **Use case:** Balance between safety and preventing cold
- **Risk:** May still overheat if cap too high for ambient conditions

### Auto-Recovery Option
**Location:** [output_manager.cpp:774-783](src/control/output_manager.cpp#L774-L783)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `autoResumeOnSensorOk` | false | Auto-clear sensor faults when sensor recovers |

**Rules:**
- Only applies to `FAULT_SENSOR_STALE` and `FAULT_SENSOR_ERROR`
- Temperature-limit faults (`FAULT_OVER_TEMP`, `FAULT_UNDER_TEMP`) require manual reset
- Prevents accidental bypass of safety limits

### Manual Fault Reset
**Location:** [output_manager.cpp:908-932](src/control/output_manager.cpp#L908-L932)

API endpoint: `POST /api/v1/output/{n}/reset-fault`

**Validation before clearing:**
- Cannot clear `FAULT_OVER_TEMP` if still above max temperature
- Cannot clear sensor fault if sensor still reporting errors
- Prevents unsafe state transitions

---

## PID Control Safety

### Integral Anti-Windup
**Location:** [output_manager.cpp:264-272](src/control/output_manager.cpp#L264-L272)

Prevents integral term from accumulating excessively:

```cpp
#define PID_INTEGRAL_MAX 100.0f

output->pidIntegral += error * dt;
if (output->pidIntegral > PID_INTEGRAL_MAX) {
    output->pidIntegral = PID_INTEGRAL_MAX;
} else if (output->pidIntegral < -PID_INTEGRAL_MAX) {
    output->pidIntegral = -PID_INTEGRAL_MAX;
}
```

**Why it matters:** Without clamping, a large setpoint error could cause integral to grow unbounded, leading to massive overshoot when error reverses.

### PID Output Clamping
**Location:** [output_manager.cpp:283-288](src/control/output_manager.cpp#L283-L288)

Final PID output always constrained to valid range:
- **Minimum:** 0%
- **Maximum:** 100%

### Time-Delta Protection
**Location:** [output_manager.cpp:254-256](src/control/output_manager.cpp#L254-L256)

Minimum 100ms between PID calculations:

```cpp
float dt = (now - output->pidLastTime) / 1000.0f;
if (dt < 0.1f) {
    return;  // Too soon, wait for more time
}
```

**Purpose:** Prevents derivative term instability from rapid consecutive calls.

---

## Output Power Protection

### Power Bounds Enforcement
**Location:** [output_manager.cpp:360-382](src/control/output_manager.cpp#L360-L382)

All power values clamped to 0-100%:

```cpp
if (power < 0) power = 0;
if (power > 100) power = 100;
```

Applied at multiple points:
- `setOutputPower()` function
- `output_manager_set_manual_power()`
- All control mode outputs

### Disabled Output Safety
**Location:** [output_manager.cpp:175-178](src/control/output_manager.cpp#L175-L178)

When output is disabled:
- Power forced to 0%
- Heating flag cleared
- Safe state guaranteed

### Hardware Compatibility Enforcement
**Location:** [output_manager.cpp:706-714](src/control/output_manager.cpp#L706-L714)

Prevents incompatible device/hardware configurations:

| Hardware | Allowed Devices |
|----------|-----------------|
| AC Dimmer (Output 0) | Light only |
| SSR (Outputs 1-2) | Heat devices only |

---

## Communication Failure Handling

### WiFi Disconnection Recovery
**Location:** [wifi_manager.cpp:61-90](src/network/wifi_manager.cpp#L61-L90)

| Parameter | Value | Description |
|-----------|-------|-------------|
| Connection timeout | 10 seconds | Max time to establish WiFi |
| Reconnect interval | 30 seconds | Retry period after failure |

### AP Mode Fallback
**Location:** [wifi_manager.cpp:154-171](src/network/wifi_manager.cpp#L154-L171)

If WiFi connection fails:
1. Device creates its own Access Point
2. SSID: `ReptileThermostat`
3. Password: `thermostat123`
4. IP: `192.168.4.1`
5. Full web UI remains accessible
6. Background reconnection attempts continue

**Critical:** Local control is ALWAYS available, even without WiFi.

### MQTT Connection Recovery
**Location:** [mqtt_manager.cpp:80-99](src/network/mqtt_manager.cpp#L80-L99)

- Automatic reconnection every 5 seconds
- Home Assistant entities remain available after reconnect
- No manual intervention required

### Network Independence
**Important:** All heating control operates locally on the ESP32. Network failures do not affect:
- Temperature monitoring
- PID control
- Safety limits
- Fault detection

---

## Bounds & Validation Checks

### Array Index Validation
**Location:** Throughout [output_manager.cpp](src/control/output_manager.cpp)

Every function accessing outputs validates index:

```cpp
if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
    return nullptr;  // or false
}
```

Protected functions include:
- `output_manager_get_output()`
- `output_manager_set_enabled()`
- `output_manager_set_mode()`
- `output_manager_set_safety_limits()`
- 20+ additional functions

### Schedule Slot Validation
**Location:** [output_manager.cpp:527-528](src/control/output_manager.cpp#L527-L528)

```cpp
if (slotIndex < 0 || slotIndex >= MAX_SCHEDULE_SLOTS) {
    return false;
}
```

### Null Pointer Checks
Systematic null checks prevent crashes:
- Hardware object initialization
- Output pointer validation
- String parameter validation

---

## Event Logging & Diagnostics

### Console Event Buffer
**Location:** [console.cpp](src/utils/console.cpp)

Circular buffer stores last 50 events with timestamps:

| Event Type | Example |
|------------|---------|
| Sensor fault | `"Output 1: SENSOR STALE - no valid reading for 30s"` |
| Over-temp | `"Output 2: OVER TEMP! 42.5C >= 40.0C"` |
| Recovery | `"Output 1: Sensor recovered, resuming"` |
| Mode change | `"Output 0 mode: PID"` |

### API Diagnostics
- `GET /api/v1/health` - Sensor status, error counts, fault states
- `GET /api/v1/status` - Full system state including all safety parameters

### Serial Console
All events logged to serial output for development debugging.

---

## Configuration Persistence

### NVS Flash Storage
**Location:** [output_manager.cpp:545-661](src/control/output_manager.cpp#L545-L661)

Safety parameters persist across reboots:

| Parameter | Stored |
|-----------|--------|
| `maxTempC` | Yes |
| `minTempC` | Yes |
| `faultTimeoutSec` | Yes |
| `faultMode` | Yes |
| `capPowerPct` | Yes |
| `autoResumeOnSensorOk` | Yes |

### Safe Defaults
**Location:** [output_manager.cpp:47-125](src/control/output_manager.cpp#L47-L125)

On first boot or factory reset:
- All outputs start in `CONTROL_MODE_OFF`
- Power initialized to 0%
- Conservative safety limits applied
- Fault mode set to `FAULT_MODE_OFF`

---

## Per-Output Isolation

### Independent Fault Tracking
**Location:** [output_manager.h:96-136](include/output_manager.h#L96-L136)

Each output maintains its own:
- Sensor assignment (`sensorAddress`)
- Health state (`sensorHealth`)
- Fault state (`faultState`)
- Fault timestamp (`faultStartTime`)
- Last valid readings (`lastValidTemp`, `lastValidPower`)

**Benefit:** A failed sensor on one zone doesn't affect other zones.

### Fault State Enumeration
**Location:** [output_manager.h:40-48](include/output_manager.h#L40-L48)

```cpp
typedef enum {
    FAULT_NONE = 0,
    FAULT_SENSOR_STALE,      // No reading within timeout
    FAULT_SENSOR_ERROR,      // Invalid reading
    FAULT_OVER_TEMP,         // Above max limit
    FAULT_UNDER_TEMP,        // Below min limit
    FAULT_HEATER_NO_RISE,    // (Reserved - not implemented)
    FAULT_HEATER_RUNAWAY     // (Reserved - not implemented)
} FaultState_t;
```

---

## Future Work / Planned Features

The following safety enhancements are planned but not yet implemented:

### 1. Safety Settings UI Page - IMPLEMENTED
**Priority:** HIGH | **Status:** Implemented in v2.2.0

A dedicated Safety Settings page accessible from the navigation bar, providing:

**Modifiable Parameters (per-output):**
| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `maxTempC` | float | 20-80°C | Over-temperature cutoff |
| `minTempC` | float | 0-30°C | Under-temperature warning |
| `faultTimeoutSec` | uint16 | 10-300s | Sensor stale timeout |
| `faultMode` | enum | OFF/HOLD/CAP | Response to faults |
| `capPowerPct` | uint8 | 0-50% | Power cap when in CAP mode |
| `autoResumeOnSensorOk` | bool | true/false | Auto-recovery option |

**Fault Analysis Display (per-output):**
| Field | Description |
|-------|-------------|
| Current Fault State | NONE/SENSOR_STALE/SENSOR_ERROR/OVER_TEMP/UNDER_TEMP |
| Sensor Health | OK/STALE/ERROR |
| Fault Duration | Time since fault started |
| Last Valid Temperature | Temperature before fault |
| Last Valid Power | Power level before fault |
| Last Valid Reading Time | Timestamp of last good reading |
| Sensor Error Count | Cumulative read failures |

**Page Features:**
- Output selector dropdown (Output 1/2/3)
- Real-time fault status display with color coding
- Manual fault reset button (with validation)
- Save/Load configuration per output
- Global "All Outputs OFF" emergency button
- Fault history log (last 10 events per output)

**API Endpoint:**
```
POST /api/output/{n}/safety
Content-Type: application/json

{
  "maxTempC": 40.0,
  "minTempC": 5.0,
  "faultTimeoutSec": 30,
  "faultMode": "off",
  "capPowerPct": 30,
  "autoResumeOnSensorOk": false
}
```

---

### 2. Hardware Watchdog Timer - IMPLEMENTED
**Priority:** HIGH | **Status:** Implemented in v2.2.0

- ESP32 hardware watchdog enabled with 30 second timeout
- Auto-reset if main loop hangs
- Prevents frozen state with heater stuck ON
- Fed at start of each loop iteration

**Implementation:** [safety_manager.cpp](src/utils/safety_manager.cpp)

---

### 3. Boot Loop Detection & Safe Mode - IMPLEMENTED
**Priority:** HIGH | **Status:** Implemented in v2.2.0

- Track `boot_count` in NVS
- Increment on boot, clear after 60s stable operation
- If count exceeds 3, enter SAFE_MODE:
  - All outputs forced OFF
  - Web UI shows safe mode banner
  - Requires manual intervention to exit via Safety page

**Implementation:** [safety_manager.cpp](src/utils/safety_manager.cpp)

---

### 4. Stuck Heater Detection
**Priority:** MEDIUM

Detect when heater is applying power but temperature isn't responding:

**FAULT_HEATER_NO_RISE:**
- Trigger: Output at >50% power for >10 minutes with <0.5°C rise
- Possible causes: Heater failure, sensor placement, thermal mass
- Response: Alert via MQTT/web UI, optionally reduce power

**FAULT_HEATER_RUNAWAY:**
- Trigger: Temperature rising >1°C/minute unexpectedly
- Possible causes: Stuck relay, sensor error, external heat source
- Response: Immediate power cutoff

### 5. Configuration Rollback
**Priority:** MEDIUM

- Maintain `config_active` and `config_last_known_good`
- Mark configuration as "good" after 10 minutes stable operation
- On boot loop or crash, auto-restore last known good config

---

### 6. Rate-of-Change Monitoring
**Priority:** MEDIUM

- Track `dT/dt` (temperature change rate)
- Alert if temperature changing faster than physically expected
- Detect sensor errors that pass range validation but show impossible physics

---

### 7. Power Failure Recovery
**Priority:** LOW

- Store last operating state before power loss (requires capacitor for clean shutdown)
- On reboot, optionally restore previous mode/setpoint
- Configurable: resume vs. start in safe mode

---

### 8. Redundant Sensor Support
**Priority:** LOW

- Allow secondary sensor assignment per output
- Automatic failover if primary sensor fails
- Cross-validation between sensors (alert if disagreement)

---

### 9. Hardware Output Verification
**Priority:** LOW

- Read back relay/SSR state where hardware supports it
- Verify commanded output matches actual output
- Detect relay welding or driver failure

---

## Safety Feature Summary

| Feature | Status | Criticality |
|---------|--------|-------------|
| Sensor CRC Validation | Implemented | CRITICAL |
| Temperature Range Validation | Implemented | CRITICAL |
| Stale Sensor Detection | Implemented | CRITICAL |
| Temperature Hard Limits | Implemented | CRITICAL |
| Over-Temperature Cutoff | Implemented | CRITICAL |
| Power Bounds (0-100%) | Implemented | CRITICAL |
| Per-Output Fault Isolation | Implemented | CRITICAL |
| Fault Mode Response (OFF/HOLD/CAP) | Implemented | HIGH |
| PID Anti-Windup | Implemented | HIGH |
| PID Output Clamping | Implemented | HIGH |
| WiFi Fallback (AP Mode) | Implemented | HIGH |
| Manual Fault Reset Validation | Implemented | HIGH |
| Hardware Compatibility Check | Implemented | HIGH |
| Hysteresis Protection | Implemented | MEDIUM |
| Auto-Recovery Option | Implemented | MEDIUM |
| MQTT Reconnection | Implemented | MEDIUM |
| Event Logging | Implemented | MEDIUM |
| Configuration Persistence | Implemented | MEDIUM |
| Error Count Tracking | Implemented | MEDIUM |
| Safety Settings UI Page | **Implemented** | HIGH |
| Hardware Watchdog Timer | **Implemented** | HIGH |
| Boot Loop Detection | **Implemented** | HIGH |
| Safe Mode | **Implemented** | HIGH |
| Stuck Heater Detection | **Planned** | MEDIUM |
| Configuration Rollback | **Planned** | MEDIUM |
| Rate-of-Change Monitoring | **Planned** | MEDIUM |
| Power Failure Recovery | **Planned** | LOW |
| Redundant Sensor Support | **Planned** | LOW |
| Hardware Output Verification | **Planned** | LOW |

---

## API Reference for Safety Features

### Get Safety Status
```
GET /api/v1/health
```
Returns sensor health, fault states, error counts.

### Get Output Safety Parameters
```
GET /api/v1/output/{n}
```
Includes `maxTempC`, `minTempC`, `faultMode`, `faultState`, `sensorHealth`.

### Set Safety Limits
```
POST /api/v1/output/{n}/safety
Content-Type: application/json

{
  "maxTempC": 40.0,
  "minTempC": 5.0,
  "faultTimeoutSec": 30,
  "faultMode": "off",      // "off" | "hold" | "cap"
  "capPowerPct": 30,
  "autoResumeOnSensorOk": false
}
```

### Reset Fault
```
POST /api/v1/output/{n}/reset-fault
```
Clears fault if safe to do so.

---

*Document generated for ESP32 Multi-Output Thermostat v2.2.0-dev*