# ESP32 Thermostat Refactoring - Migration Checklist

**Project:** ESP32 Reptile Thermostat  
**Version:** 1.3.3 → 1.4.0 (Modular)  
**Date Started:** [Fill in date]  
**Completed:** [ ]

---

## Overview

This checklist guides the migration from a single 2300-line main.cpp file to a clean, modular architecture with separate hardware, network, control, storage, and UI modules.

---

## Phase 1: Hardware Drivers ✓ COMPLETED

### 1.1 Setup New Project Structure
- [ ] Create new directory: `refactored/`
- [ ] Copy `platformio.ini` to new directory
- [ ] Create folder structure:
  ```
  refactored/
  ├── include/
  │   ├── config.h
  │   └── hardware/
  ├── src/
  │   ├── hardware/
  │   └── main.cpp
  ```
- [ ] Verify PlatformIO recognizes new project structure

### 1.2 Create Configuration Module
- [✓] Create `include/config.h`
- [ ] Move all `#define` constants from old main.cpp
- [ ] Organize into logical sections (pins, limits, network, etc.)
- [ ] Test compilation with config.h included

### 1.3 Temperature Sensor Module
- [✓] Create `include/hardware/sensor.h`
- [✓] Create `src/hardware/sensor.cpp`
- [ ] Extract sensor initialization code from old main.cpp
- [ ] Extract `readTemperature()` function
- [ ] Test sensor independently:
  ```cpp
  TemperatureSensor sensor(ONE_WIRE_BUS);
  sensor.begin();
  float temp;
  if (sensor.readTemperature(temp)) {
      Serial.println(temp);
  }
  ```
- [ ] Verify sensor error handling works

### 1.4 AC Dimmer Module
- [✓] Create `include/hardware/dimmer.h`
- [✓] Create `src/hardware/dimmer.cpp`
- [ ] Extract dimmer initialization from old main.cpp
- [ ] Test single dimmer control:
  ```cpp
  ACDimmer dimmer(DIMMER_HEAT_PIN, ZEROCROSS_PIN, "Heat");
  dimmer.begin();
  dimmer.setPower(50);  // Test 50% power
  ```
- [ ] Test power ramping (0→100% in steps)
- [ ] Verify zero-cross detection working

### 1.5 Display Module
- [✓] Create `include/hardware/display.h`
- [✓] Create `src/hardware/display.cpp`
- [✓] Create `src/hardware/display_screens.cpp`
- [✓] Create `src/hardware/display_touch.cpp`
- [ ] Extract display initialization from old main.cpp
- [ ] Extract screen drawing functions
- [ ] Extract touch handling code
- [ ] Test display independently:
  ```cpp
  Display display;
  display.begin();
  DisplayState state = {28.5, 30.0, true, 65, "auto", "Test", true, true, false};
  display.update(state);
  ```
- [ ] Test all three screens (Main, Settings, Simple)
- [ ] Test touch buttons on each screen
- [ ] Verify touch coordinate mapping

### 1.6 Update platformio.ini
- [ ] Verify all library dependencies present:
  ```ini
  lib_deps = 
      paulstoffregen/OneWire@^2.3.7
      milesburton/DallasTemperature@^3.11.0
      https://github.com/RobotDynOfficial/RBDDimmer.git
      bodmer/TFT_eSPI@^2.5.43
  ```
- [ ] Ensure TFT_eSPI build flags correct

### 1.7 Create Minimal main.cpp for Phase 1
- [ ] Create new main.cpp with Phase 1 modules only:
  ```cpp
  #include <Arduino.h>
  #include "config.h"
  #include "hardware/sensor.h"
  #include "hardware/dimmer.h"
  #include "hardware/display.h"
  
  TemperatureSensor sensor(ONE_WIRE_BUS);
  ACDimmer dimmer(DIMMER_HEAT_PIN, ZEROCROSS_PIN);
  Display display;
  
  void setup() {
      Serial.begin(SERIAL_BAUD_RATE);
      sensor.begin();
      dimmer.begin();
      display.begin();
  }
  
  void loop() {
      float temp;
      if (sensor.readTemperature(temp)) {
          DisplayState state = {temp, 28.0, false, 0, "auto", "Test", false, false, false};
          display.update(state);
      }
      delay(1000);
  }
  ```
- [ ] Compile and verify no errors
- [ ] Upload to ESP32
- [ ] Test functionality:
  - [ ] Temperature reading works
  - [ ] Display shows temperature
  - [ ] Touch buttons respond
  - [ ] Dimmer can be controlled

### 1.8 Phase 1 Verification Checklist
- [ ] All hardware modules compile without errors
- [ ] Sensor reads temperature correctly
- [ ] Display shows all three screens
- [ ] Touch input works on all screens
- [ ] Dimmer controls power output
- [ ] Serial logging shows clear module prefixes
- [ ] No memory leaks (check free heap)
- [ ] System runs stable for 5+ minutes

**Phase 1 Status:** [ ] Complete / [ ] Issues (list below)

**Issues Found:**
```
(List any problems encountered and their resolutions)
```

---

## Phase 2: Network Stack

### 2.1 WiFi Manager Module
- [ ] Create `include/network/wifi_manager.h`
- [ ] Create `src/network/wifi_manager.cpp`
- [ ] Extract WiFi connection code from old main.cpp
- [ ] Extract AP mode code
- [ ] Extract mDNS setup
- [ ] Test WiFi connection:
  ```cpp
  WiFiManager wifi;
  wifi.begin();
  if (wifi.connect()) {
      Serial.println(wifi.getIP());
  }
  ```
- [ ] Test AP mode fallback
- [ ] Test mDNS hostname resolution

### 2.2 MQTT Client Module
- [ ] Create `include/network/mqtt_client.h`
- [ ] Create `src/network/mqtt_client.cpp`
- [ ] Add PubSubClient and ArduinoJson to dependencies
- [ ] Extract MQTT connection code
- [ ] Extract Home Assistant discovery
- [ ] Extract publish/subscribe logic
- [ ] Test MQTT connection
- [ ] Test publishing status
- [ ] Test receiving commands

### 2.3 Web Server Module
- [ ] Create `include/network/webserver.h`
- [ ] Create `src/network/webserver.cpp`
- [ ] Extract WebServer setup
- [ ] Extract route handlers
- [ ] Keep HTML generation for now (move to UI phase)
- [ ] Test web server:
  - [ ] Home page loads
  - [ ] API endpoints respond
  - [ ] Settings page works
  - [ ] Mobile responsive

### 2.4 OTA Updater Module
- [ ] Create `include/network/ota_updater.h`
- [ ] Create `src/network/ota_updater.cpp`
- [ ] Add HTTPClient to dependencies
- [ ] Extract manual upload handler
- [ ] Extract GitHub auto-update
- [ ] Test manual firmware upload
- [ ] Test GitHub update check
- [ ] Test auto-update download

### 2.5 Update main.cpp for Phase 2
- [ ] Add network modules to main.cpp
- [ ] Initialize WiFi in setup()
- [ ] Initialize MQTT in setup()
- [ ] Initialize web server in setup()
- [ ] Call network loop() functions
- [ ] Compile and test

### 2.6 Phase 2 Verification Checklist
- [ ] WiFi connects automatically
- [ ] AP mode works if WiFi fails
- [ ] mDNS hostname resolves
- [ ] MQTT connects to broker
- [ ] MQTT publishes status
- [ ] MQTT receives commands
- [ ] Web server accessible
- [ ] All web pages load
- [ ] OTA update works
- [ ] System stable for 10+ minutes

**Phase 2 Status:** [ ] Complete / [ ] Issues

---

## Phase 3: Control Logic

### 3.1 PID Controller Module
- [ ] Create `include/control/pid_controller.h`
- [ ] Create `src/control/pid_controller.cpp`
- [ ] Extract PID algorithm from old main.cpp
- [ ] Make PID testable (pure function)
- [ ] Test PID calculations offline:
  ```cpp
  PIDController pid(10.0, 0.5, 5.0);
  int output = pid.compute(30.0, 28.5, 1.0);
  Serial.println(output);  // Should be reasonable
  ```
- [ ] Verify anti-windup works

### 3.2 Scheduler Module
- [ ] Create `include/control/scheduler.h`
- [ ] Create `src/control/scheduler.cpp`
- [ ] Extract schedule data structures
- [ ] Extract schedule checking logic
- [ ] Extract load/save functions
- [ ] Test schedule system:
  - [ ] Create test schedule
  - [ ] Verify time slot detection
  - [ ] Check day-specific logic

### 3.3 Thermostat Coordinator
- [ ] Create `include/control/thermostat.h`
- [ ] Create `src/control/thermostat.cpp`
- [ ] Create high-level control logic
- [ ] Coordinate: sensor → PID → dimmer
- [ ] Handle mode switching (auto/on/off)
- [ ] Handle schedule integration
- [ ] Test full control loop

### 3.4 Update main.cpp for Phase 3
- [ ] Add control modules
- [ ] Replace PID code with controller
- [ ] Replace schedule code with scheduler
- [ ] Simplify loop() - just call thermostat.update()
- [ ] Compile and test

### 3.5 Phase 3 Verification Checklist
- [ ] PID control maintains temperature
- [ ] Mode switching works (auto/on/off)
- [ ] Schedule activates at correct times
- [ ] Temperature changes smoothly
- [ ] System stable for 30+ minutes
- [ ] No temperature oscillations

**Phase 3 Status:** [ ] Complete / [ ] Issues

---

## Phase 4: Storage & UI

### 4.1 Settings Manager Module
- [ ] Create `include/storage/preferences.h`
- [ ] Create `src/storage/preferences.cpp`
- [ ] Add Preferences library
- [ ] Extract all preferences operations
- [ ] Centralize load/save logic
- [ ] Test settings persistence

### 4.2 Logger Module
- [ ] Create `include/storage/logger.h`
- [ ] Create `src/storage/logger.cpp`
- [ ] Extract logging system
- [ ] Implement circular buffer
- [ ] Add timestamp formatting
- [ ] Test log retrieval

### 4.3 Web Pages Module
- [ ] Create `include/ui/web_pages.h`
- [ ] Create `src/ui/web_pages.cpp`
- [ ] Extract HTML generation functions
- [ ] Separate presentation from logic
- [ ] Test all pages render correctly

### 4.4 TFT Screens Module (Already Done)
- [✓] Screen rendering in display_screens.cpp
- [ ] Verify integration with Display class

### 4.5 Update main.cpp for Phase 4
- [ ] Add storage modules
- [ ] Add UI modules
- [ ] Connect web pages to web server
- [ ] Simplify main.cpp further
- [ ] Compile and test

### 4.6 Phase 4 Verification Checklist
- [ ] Settings save/load correctly
- [ ] Logs appear in web interface
- [ ] All web pages display correctly
- [ ] Settings persist across reboots
- [ ] System stable for 1+ hour

**Phase 4 Status:** [ ] Complete / [ ] Issues

---

## Phase 5: Final Integration

### 5.1 Slim Down main.cpp
- [ ] Review main.cpp - should be ~100 lines
- [ ] Move any remaining logic to modules
- [ ] Ensure setup() just initializes
- [ ] Ensure loop() just calls update functions
- [ ] Add clear comments

### 5.2 Documentation
- [ ] Update README with new structure
- [ ] Document each module's purpose
- [ ] Create API documentation for each class
- [ ] Add wiring diagrams if needed
- [ ] Document build/upload process

### 5.3 Code Quality Check
- [ ] Run static analysis (if available)
- [ ] Check for memory leaks
- [ ] Verify no global variables (except in main)
- [ ] Check all Serial.print have module prefixes
- [ ] Ensure consistent naming conventions

### 5.4 Full System Test
- [ ] Fresh upload to clean ESP32
- [ ] Test all features end-to-end:
  - [ ] Temperature sensing
  - [ ] PID control
  - [ ] Display (all screens)
  - [ ] Touch input
  - [ ] Web interface (all pages)
  - [ ] MQTT integration
  - [ ] Schedule system
  - [ ] OTA updates
  - [ ] Settings persistence
  - [ ] Logging system
- [ ] Run for 24 hours continuous
- [ ] Check for memory leaks
- [ ] Verify WiFi reconnection
- [ ] Verify MQTT reconnection

### 5.5 Performance Validation
- [ ] Measure boot time: _____ seconds
- [ ] Measure free heap: _____ KB
- [ ] Measure loop() execution time: _____ ms
- [ ] Temperature reading latency: _____ ms
- [ ] Display update time: _____ ms
- [ ] Web page load time: _____ ms

### 5.6 Comparison with Original
| Metric | Old (v1.3.3) | New (v1.4.0) | Improvement |
|--------|--------------|--------------|-------------|
| Lines of code (main.cpp) | 2300 | ~100 | ✓ |
| Number of files | 1 | ~15 | Better |
| Compile time | ___ s | ___ s | ___ |
| Binary size | ___ KB | ___ KB | ___ |
| Free heap | ___ KB | ___ KB | ___ |
| Boot time | ___ s | ___ s | ___ |

---

## Phase 6: Advanced Features (Future)

### 6.1 Dual Dimmer Support
- [ ] Extend ACDimmer to support light control
- [ ] Implement DualDimmerController
- [ ] Add light scheduling
- [ ] Add transition/ramping logic
- [ ] Update display to show both heat and light

### 6.2 Unit Testing
- [ ] Create test framework
- [ ] Write unit tests for PID controller
- [ ] Write unit tests for scheduler
- [ ] Write integration tests
- [ ] Set up CI/CD pipeline

### 6.3 Additional Enhancements
- [ ] Temperature graphing
- [ ] Data export (CSV)
- [ ] Email/push notifications
- [ ] Multiple devices dashboard
- [ ] Android app integration

---

## Rollback Plan

If critical issues arise during migration:

1. **Backup Original Code:**
   - [ ] Old code saved as `ESP32_Thermostat_v1.3.3_BACKUP.cpp`
   - [ ] Git commit before starting refactor

2. **Quick Rollback Steps:**
   - [ ] Copy backup file back to src/main.cpp
   - [ ] Remove new include/ and src/ directories
   - [ ] Restore original platformio.ini
   - [ ] Rebuild and upload

3. **Partial Rollback:**
   - [ ] Identify problematic module
   - [ ] Replace with old implementation
   - [ ] Continue with other modules

---

## Common Issues & Solutions

### Issue: Compilation Errors
**Symptoms:** Undefined references, missing headers  
**Solution:**
- Check #include paths match file locations
- Verify platformio.ini has correct library dependencies
- Ensure all .cpp files in src/ are being compiled

### Issue: Display Not Working
**Symptoms:** Blank screen, garbled graphics  
**Solution:**
- Check TFT_eSPI build flags in platformio.ini
- Verify pin definitions in config.h
- Test display initialization separately

### Issue: Touch Not Responding
**Symptoms:** No touch detected, wrong coordinates  
**Solution:**
- Check touch coordinate mapping in display.cpp
- Verify TOUCH_CS pin definition
- Add debug prints in handleTouch()

### Issue: Memory Issues
**Symptoms:** Crashes, reboots, heap corruption  
**Solution:**
- Check for memory leaks with Serial.println(ESP.getFreeHeap())
- Reduce buffer sizes if needed
- Use String carefully (prefer char arrays)

### Issue: WiFi Won't Connect
**Symptoms:** Stuck in connection loop  
**Solution:**
- Check WiFi credentials in config.h
- Verify 2.4GHz network (ESP32 doesn't support 5GHz)
- Test with WiFi.begin() directly

---

## Testing Checklist Template

For each phase, copy and fill out:

**Test Date:** ____________  
**Phase:** _____  
**Tester:** ____________

| Test Case | Expected | Actual | Pass/Fail | Notes |
|-----------|----------|--------|-----------|-------|
| Compile | No errors | | | |
| Upload | Success | | | |
| Boot | Serial output OK | | | |
| Feature 1 | ___ | | | |
| Feature 2 | ___ | | | |
| ... | | | | |

---

## Sign-Off

### Phase 1 - Hardware Drivers
**Completed by:** ____________  
**Date:** ____________  
**Verified by:** ____________  
**Notes:** ____________

### Phase 2 - Network Stack
**Completed by:** ____________  
**Date:** ____________  
**Verified by:** ____________  
**Notes:** ____________

### Phase 3 - Control Logic
**Completed by:** ____________  
**Date:** ____________  
**Verified by:** ____________  
**Notes:** ____________

### Phase 4 - Storage & UI
**Completed by:** ____________  
**Date:** ____________  
**Verified by:** ____________  
**Notes:** ____________

### Phase 5 - Final Integration
**Completed by:** ____________  
**Date:** ____________  
**Verified by:** ____________  
**Notes:** ____________

---

## Final Approval

**Project Refactoring Complete:** [ ] Yes / [ ] No  
**All Tests Passed:** [ ] Yes / [ ] No  
**Documentation Updated:** [ ] Yes / [ ] No  
**Production Ready:** [ ] Yes / [ ] No  

**Approved by:** ____________  
**Date:** ____________  
**Signature:** ____________

---

## Next Steps After Completion

1. [ ] Tag release as v1.4.0 in Git
2. [ ] Update GitHub README
3. [ ] Create release notes
4. [ ] Build and attach firmware.bin to release
5. [ ] Test auto-update from v1.3.3 → v1.4.0
6. [ ] Announce new modular architecture
7. [ ] Plan Phase 6 advanced features

---

**End of Migration Checklist**
