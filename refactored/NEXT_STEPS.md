# ESP32 Thermostat - Next Steps Discussion

**Date:** January 11, 2026
**Current Version:** v1.9.0
**Status:** Modular refactoring complete

---

## 📋 Summary of Updates

### Documentation Updated
- ✅ [README.md](README.md) - Updated to reflect v1.9.0 status and current features
- ✅ [SCOPE.md](SCOPE.md) - Comprehensive future features roadmap created
- ✅ [NEXT_STEPS.md](NEXT_STEPS.md) - This file (discussion document)

### Key Information Added to SCOPE.md

1. **Multi-Output Environmental Control**
   - 3 independent outputs (AC dimmer, SSR pulse, SSR on/off)
   - 3 DS18B20 sensors with assignment per output
   - Hardware support matrix and configuration details
   - Web interface mockups and MQTT extensions

2. **Setup Wizard**
   - Complete 6-step wizard flow diagram
   - Technical implementation details
   - State machine and web routes

3. **Android App Development**
   - Feature requirements and phases
   - mDNS discovery implementation
   - API requirements (existing and new)
   - Development stack recommendations

4. **Known Issues**
   - WiFi reconnection bug detailed analysis
   - Root cause identified in [wifi_manager.cpp:60-86](src/network/wifi_manager.cpp#L60-L86)
   - Complete fix with code examples
   - Testing plan provided

5. **Implementation Roadmap**
   - Recommended order of features
   - Time estimates for each phase
   - Memory and performance analysis

---

## 🚨 Critical Issue: WiFi Reconnection Bug

### Current Behavior
- WiFi connection lost (e.g., router reboot)
- Device enters AP mode
- WiFi returns, but device **stays in AP mode**
- Requires manual power cycle to reconnect

### Root Cause
In [wifi_manager.cpp:60-64](src/network/wifi_manager.cpp#L60-L64):

```cpp
void wifi_task(void) {
    // Don't reconnect if in AP mode
    if (apMode) {
        return;  // ← PROBLEM: Never attempts to reconnect!
    }
    // ... reconnection logic below never executes
}
```

### The Fix
The `wifi_task()` function should periodically attempt to reconnect to saved WiFi even when in AP mode:

```cpp
void wifi_task(void) {
    // In AP mode: periodically check if saved WiFi is available
    if (apMode) {
        if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
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
    } else {
        if (currentState != WIFI_STATE_CONNECTED) {
            Serial.println("[WiFi] Connection established");
            currentState = WIFI_STATE_CONNECTED;
            updateIPAddress();
        }
    }
}
```

### Implementation Time
- **Estimated:** 1 hour (including testing)
- **Complexity:** Low
- **Impact:** High (critical reliability fix)

---

## 💬 Discussion: Moving Forward

### Option 1: Fix WiFi Reconnection Bug (Recommended First)

**Pros:**
- Critical reliability issue
- Quick fix (1 hour)
- High user impact
- Should be fixed before adding new features

**Steps:**
1. Edit [wifi_manager.cpp](src/network/wifi_manager.cpp)
2. Update `wifi_task()` function as shown above
3. Build and test:
   - Connect to WiFi
   - Disable router WiFi
   - Wait for AP mode
   - Re-enable router WiFi
   - Verify automatic reconnection within 30 seconds
4. Test console log shows reconnection attempts
5. Commit as v1.9.1

**Effort:** 1 hour
**Priority:** Critical

---

### Option 2: Multi-Output Environmental Control

**Overview:** Expand system to support 3 independent outputs with flexible hardware.

**What this enables:**
- **Output 1:** Ceramic heat emitter (AC dimmer, PID control)
- **Output 2:** Under-tank heat mat (SSR pulse control)
- **Output 3:** Basking lamp (SSR on/off control)

Each with:
- Dedicated temperature sensor
- Independent scheduling
- Separate web controls
- Individual MQTT entities for Home Assistant

**Implementation Phases:**

1. **Phase A: Sensor Management (3-4 hours)**
   - Add DS18B20 enumeration on OneWire bus
   - Create sensor management web page
   - Allow naming sensors
   - Store sensor ROM addresses in preferences

2. **Phase B: Multi-Output Control (8-10 hours)**
   - Expand hardware abstraction for 3 outputs
   - Update web interface (tabs or accordion per output)
   - Duplicate PID/schedule logic × 3
   - Store per-output configuration

3. **Phase C: MQTT Multi-Output (2-3 hours)**
   - Create 3 separate climate entities
   - Publish per-output status
   - Handle per-output commands

**Total Time:** 13-17 hours
**Priority:** High (major feature expansion)

**Memory Impact:**
- Flash: +10 KB (estimated 89% total)
- RAM: +6 KB (estimated 21% total)
- ✅ Safe within limits

---

### Option 3: Setup Wizard

**Overview:** Guided first-run configuration for new users.

**What this provides:**
- Professional onboarding experience
- Eliminates need for manual WiFi/MQTT configuration
- Sensor discovery and naming
- Output configuration with hardware selection
- Reduces support burden

**Implementation Complexity:** High (20-25 hours)

**Should be done AFTER:**
- WiFi reconnection fix
- Multi-output control (so wizard can configure all outputs)

**Priority:** Medium (nice-to-have, but requires multi-output first)

---

### Option 4: Android App

**Overview:** Native Android app for remote monitoring and control.

**What this provides:**
- mDNS device discovery (no IP address needed)
- Multi-device management (multiple enclosures)
- Push notifications for alerts
- Real-time temperature graphs
- Professional presentation

**Implementation Complexity:** Very High (80-120 hours)

**Should be done AFTER:**
- All other features stabilized
- APIs finalized
- Multi-output control complete

**Priority:** Low (future enhancement)

---

## 🎯 Recommended Implementation Order

### Immediate (Today/This Week)
1. **Fix WiFi reconnection bug** (1 hour)
   - Critical reliability issue
   - Quick win
   - Test and commit as v1.9.1

### Short-term (Next 2-3 Sessions)
2. **Multi-output environmental control** (13-17 hours)
   - Major feature expansion
   - High user value for reptile keepers
   - Foundation for setup wizard
   - Split across multiple sessions:
     - Session 1: Sensor management (4 hours)
     - Session 2: Output configuration & web UI (10 hours)
     - Session 3: MQTT integration & testing (3 hours)

### Medium-term (After multi-output complete)
3. **Setup wizard** (20-25 hours)
   - Professional onboarding
   - Configure all 3 outputs during setup
   - Sensor discovery integration
   - Split across 2-3 sessions

### Long-term (Future project)
4. **Android app** (80-120 hours)
   - Separate project
   - Can be developed in parallel by another developer
   - Requires stable API endpoints

---

## 📊 Resource Considerations

### Current Memory Usage (v1.9.0)
- Flash: 85.7% (1.11 MB / 1.31 MB)
- RAM: 19.8% (64 KB / 327 KB)

### Projected After Multi-Output
- Flash: ~89% (+10 KB)
- RAM: ~21% (+6 KB)
- ✅ Still safe with headroom

### Projected After Setup Wizard
- Flash: ~92% (+15 KB)
- RAM: ~22% (+1 KB)
- ✅ Approaching limit but acceptable

### Optimization Opportunities
If memory becomes tight:
- Remove temperature history (saves ~2.3 KB RAM, 5 KB flash)
- Reduce console buffer from 50 to 25 events (saves ~1.5 KB RAM)
- Compress web HTML (save ~10 KB flash)

---

## ❓ Questions for Discussion

### 1. Priority Confirmation
**Question:** Should we fix the WiFi reconnection bug before adding new features?

**Recommendation:** Yes - it's a critical reliability issue that should be addressed first. Only takes 1 hour.

### 2. Multi-Output Scope
**Question:** Should we implement all 3 outputs at once, or start with 2 outputs as a proof-of-concept?

**Options:**
- **A) All 3 outputs** - Complete feature, more work upfront
- **B) 2 outputs first** - Faster to test, easier to refactor if needed
- **C) 1 additional output** - Minimal change, proves the concept

**Recommendation:** Option A (all 3) - The architecture is the same regardless, and users will want all 3 for complete environmental control.

### 3. Sensor Assignment
**Question:** How should sensors be assigned to outputs?

**Options:**
- **A) Auto-assign by discovery order** - Sensor 1 → Output 1, etc.
- **B) Dropdown selection per output** - User manually assigns
- **C) Auto-assign with option to override** - Best of both worlds

**Recommendation:** Option C - Auto-assign for simplicity, allow override for flexibility.

### 4. Hardware Configuration
**Question:** Should hardware type (dimmer/SSR) be auto-detected or manually configured?

**Options:**
- **A) Auto-detect** - Try to sense hardware type (complex, may not work)
- **B) Manual selection** - User chooses from dropdown (simple, reliable)
- **C) Fixed assignment** - Output 1 = dimmer, Output 2-3 = SSR (inflexible)

**Recommendation:** Option B (manual selection) - Most reliable and flexible. Setup wizard can guide the user.

### 5. Testing Approach
**Question:** How should we test multi-output without physical hardware for outputs 2 and 3?

**Options:**
- **A) Breadboard simulation** - Use LEDs to represent outputs
- **B) Software-only testing** - Trust the logic, test web interface
- **C) Buy additional hardware** - SSRs are cheap (~$10)
- **D) Test output 1 thoroughly, trust duplication** - Pragmatic approach

**Recommendation:** Option D initially, then Option C if budget allows. The control logic is duplicated, so if output 1 works, outputs 2-3 should work identically.

### 6. Web Interface Layout
**Question:** How should multiple outputs be displayed on the home page?

**Options:**
- **A) Tabs** - Click tab to switch between outputs
- **B) Accordion** - Expand/collapse sections per output
- **C) Stacked cards** - All outputs visible, scroll down
- **D) Grid layout** - 3 columns (desktop), stacked (mobile)

**Recommendation:** Option D (grid layout) - Most efficient use of space, responsive design already in place.

---

## 🛠️ Technical Details for Multi-Output Implementation

### Data Structures

```cpp
// Maximum number of outputs
#define MAX_OUTPUTS 3

// Hardware types
typedef enum {
    HARDWARE_DIMMER_AC,    // AC dimmer (RobotDyn)
    HARDWARE_SSR_PULSE,    // SSR with pulse width control
    HARDWARE_SSR_ONOFF     // SSR with simple on/off
} HardwareType_t;

// Control modes
typedef enum {
    CONTROL_MODE_OFF,      // Output disabled
    CONTROL_MODE_PID,      // PID control (dimmer/pulse)
    CONTROL_MODE_ONOFF,    // Simple thermostat (on when temp < target)
    CONTROL_MODE_SCHEDULE  // Schedule-based control
} ControlMode_t;

// Output configuration
typedef struct {
    bool enabled;
    char name[32];
    HardwareType_t hardwareType;
    uint8_t controlPin;
    ControlMode_t controlMode;
    char sensorAddress[17];  // DS18B20 ROM address (hex string)
    float targetTemp;
    float currentTemp;
    int currentPower;
    // PID parameters
    float pidKp, pidKi, pidKd;
    // Schedule data (6 slots)
    ScheduleSlot_t schedule[6];
} OutputConfig_t;

// Global array
static OutputConfig_t outputs[MAX_OUTPUTS];
```

### Web API Extensions

```cpp
// New endpoints needed
GET  /api/outputs            // List all outputs with status
GET  /api/output/{id}        // Get specific output details
POST /api/output/{id}/config // Configure output
POST /api/output/{id}/control// Control output (target, mode)

GET  /api/sensors            // List all discovered sensors
POST /api/sensor/{address}/name  // Rename sensor
```

### MQTT Topics

```
thermostat/output1/temperature
thermostat/output1/setpoint
thermostat/output1/mode
thermostat/output1/power

thermostat/output2/temperature
thermostat/output2/setpoint
...

homeassistant/climate/thermostat_output1/config
homeassistant/climate/thermostat_output2/config
homeassistant/climate/thermostat_output3/config
```

### Preferences Storage

```cpp
// Per-output storage namespace
prefs.begin("output1", false);
prefs.putBool("enabled", outputs[0].enabled);
prefs.putString("name", outputs[0].name);
prefs.putUChar("hwType", outputs[0].hardwareType);
prefs.putString("sensor", outputs[0].sensorAddress);
// ... etc
prefs.end();

// Repeat for output2, output3
```

---

## 📝 Next Session Prep

### If Proceeding with WiFi Fix

**Session goal:** Fix WiFi reconnection bug (v1.9.1)

**Files to edit:**
- [src/network/wifi_manager.cpp](src/network/wifi_manager.cpp) (wifi_task function)

**Testing:**
- Disconnect/reconnect WiFi router
- Verify automatic reconnection
- Check console logs

**Time:** 1 hour

### If Proceeding with Multi-Output

**Session goal:** Phase A - Sensor management

**Files to create/edit:**
- Create: `sensor_manager.h` and `sensor_manager.cpp`
- Edit: [src/network/web_server.cpp](src/network/web_server.cpp) (add /sensors page)
- Edit: [src/main.cpp](src/main.cpp) (enumerate sensors on boot)

**Testing:**
- Connect multiple DS18B20 sensors
- Verify all sensors discovered
- Test naming and identification

**Time:** 3-4 hours

---

## 🎯 Decision Time

**Which option would you like to proceed with?**

1. **Fix WiFi reconnection bug** (1 hour, critical)
2. **Start multi-output implementation** (Phase A: sensor management, 3-4 hours)
3. **Discuss multi-output architecture further** (clarify questions above)
4. **Something else** (specify)

**My recommendation:** Option 1 (WiFi fix) followed by Option 2 (multi-output) in a future session. The WiFi bug affects current reliability, so it should be fixed first.

---

**Document prepared for:** Next session planning and feature prioritization
**Last updated:** January 11, 2026
