# ESP32 Thermostat - Next Session Prep Document
**Date Created:** January 10, 2026  
**For Session:** January 11, 2026  
**Project:** ESP32 Reptile Thermostat Modular Refactoring

---

## 🎯 SESSION GOALS

**Primary Objective:** Complete remaining modular refactoring phases and test full system

**Phases Remaining:**
- Phase 5: Complete hardware abstraction (temp sensor, dimmer control)
- Phase 6: Utilities (logging system, preferences wrapper)
- Testing & validation of all modules
- Documentation updates

**Success Criteria:**
- All code compiles without errors
- System boots and runs correctly
- All features working (WiFi, MQTT, web, TFT, scheduling)
- Clean, modular architecture complete

---

## 📊 CURRENT STATUS (End of Session Jan 10)

### ✅ Completed Phases

**Phase 1: Hardware Drivers** (Partial)
- Status: Only DS18B20 and dimmer code in main.cpp
- Remaining: Extract to modules

**Phase 2: Network Stack** (Complete ✅)
- Files: wifi_manager.cpp/h, mqtt_manager.cpp/h, web_server.cpp/h
- Location: `src/network/`
- Status: All working, includes GitHub auto-update

**Phase 3: Control & Logic** (Complete ✅)
- Files: pid_controller.cpp/h, system_state.cpp/h, scheduler.cpp/h
- Location: `src/control/`
- Status: All working, schedule page fully functional

**Phase 4: TFT Display** (Complete ✅)
- Files: tft_display.cpp/h
- Location: `src/hardware/`
- Status: All screens working, touch callbacks implemented

### 📁 Current File Structure

```
include/
├── wifi_manager.h
├── mqtt_manager.h
├── web_server.h
├── pid_controller.h
├── system_state.h
├── scheduler.h
└── tft_display.h

src/
├── main.cpp (375 lines - down from 2166!)
├── network/
│   ├── wifi_manager.cpp
│   ├── mqtt_manager.cpp
│   └── web_server.cpp
├── control/
│   ├── pid_controller.cpp
│   ├── system_state.cpp
│   └── scheduler.cpp
└── hardware/
    └── tft_display.cpp
```

### 🔧 What's Still in main.cpp

1. **Hardware objects:**
   - `OneWire oneWire(ONE_WIRE_BUS);`
   - `DallasTemperature sensors(&oneWire);`
   - `dimmerLamp dimmer(DIMMER_PIN, ZEROCROSS_PIN);`

2. **Functions:**
   - `readTemperature()` - DS18B20 reading
   - `addLog()` - Logging helper
   - Callback functions (MQTT, web, touch)
   - `setup()` and `loop()`

3. **Timing variables:**
   - `lastTempRead`, `lastPidTime`, `lastMqttPublish`, `bootTime`

---

## 🚀 PHASE 5: HARDWARE ABSTRACTION

### Task: Extract Temperature Sensor Module

**Create: `temp_sensor.h` and `temp_sensor.cpp`**

**Functions needed:**
```cpp
void temp_sensor_init(void);
float temp_sensor_read(void);
bool temp_sensor_is_valid(float temp);
```

**What to extract from main.cpp:**
- OneWire and DallasTemperature objects
- `readTemperature()` function
- Temperature validation logic

**Benefits:**
- Hardware independence (easy to swap DS18B20 for another sensor)
- Testable temperature reading
- Error handling centralized

---

### Task: Extract Dimmer Control Module

**Create: `dimmer_control.h` and `dimmer_control.cpp`**

**Functions needed:**
```cpp
void dimmer_init(void);
void dimmer_set_power(int percent);
int dimmer_get_power(void);
```

**What to extract from main.cpp:**
- `dimmerLamp dimmer` object
- Dimmer initialization
- Power control logic

**Benefits:**
- Hardware independence (easy to swap dimmer types)
- Prepare for dual dimmer system (Phase 7)
- Clean power control interface

---

## 🛠️ PHASE 6: UTILITIES

### Task: Create Logging Module

**Create: `logger.h` and `logger.cpp`**

**Why:** Logging is currently split between main.cpp and web_server

**Functions needed:**
```cpp
void logger_init(void);
void logger_add(const char* message);
const char* logger_get_entry(int index);
int logger_get_count(void);
void logger_clear(void);
```

**What to extract:**
- `addLog()` function from main.cpp
- Log storage array
- Timestamp formatting

**Benefits:**
- Centralized logging
- Consistent timestamps
- Multiple log outputs (Serial, web, file)

---

## 📝 EFFICIENCY TIPS FOR TOMORROW

### 1. Start with Quick Wins
**Order of tasks by speed:**
1. ✅ **Logging module** (30 min) - Simple extraction, no complex logic
2. ✅ **Temp sensor module** (45 min) - Straightforward hardware wrapper
3. ✅ **Dimmer module** (45 min) - Similar to temp sensor
4. ⏱️ **Testing** (60 min) - Compile, upload, verify all features
5. 📚 **Documentation** (30 min) - Update README, create API docs

**Total estimated:** ~3 hours

### 2. Token Management Strategy

**To save tokens:**
- Start with: "I have the ESP32 thermostat project. Read /mnt/project/PROJECT_STATUS_v1_3_1.md and this session prep file for context."
- Use: "Create [module_name].h and .cpp based on the pattern from [similar_module]"
- Avoid: Long explanations of what you're doing
- Use: `view` tool to check specific functions before extraction
- Create files incrementally, test compile frequently

**Token-efficient prompts:**
```
"Extract temp_sensor module following the pattern from pid_controller"
"Fix compilation errors in main.cpp" (Claude will find and fix)
"Update main.cpp to use temp_sensor module instead of direct OneWire calls"
```

### 3. Testing Checklist (Keep This Handy)

After compiling, verify:
- [ ] WiFi connects
- [ ] Web interface loads (all pages: Home, Schedule, Info, Logs, Settings)
- [ ] TFT displays correctly (Main, Settings, Simple screens)
- [ ] Touch buttons work (+/-, Settings, Simple, mode buttons)
- [ ] Temperature reads correctly
- [ ] PID control works (heating on/off based on temp)
- [ ] Schedule can be edited and saved
- [ ] MQTT publishes status
- [ ] Settings save and persist after restart

### 4. Common Issues & Quick Fixes

**Issue:** "undefined reference to..."
**Fix:** Missing function implementation or header include

**Issue:** "was not declared in this scope"
**Fix:** Missing forward declaration or header

**Issue:** Multiple definition errors
**Fix:** Remove duplicate code, ensure header guards

**Issue:** Display not updating
**Fix:** Call `tft_request_update()` after state changes

---

## 🗂️ FILES IN /mnt/project/ (Available for Reference)

**Use these for context:**
- `ESP32_Thermostat_v1_3_3_mobile_fix.cpp` - Original monolithic code (2166 lines)
- `PROJECT_STATUS_v1_3_1.md` - Full project documentation
- `DUAL_DIMMER_DESIGN.md` - Future dual dimmer specifications
- `Scheduling_Guide.md` - User guide for scheduling
- `ESP32_Reptile_Thermostat_-_Project_README.md` - Hardware info

**Use these for code reference when creating new modules:**
- Check original v1.3.3 for how functions worked
- Copy hardware pin definitions
- Reference callback patterns

---

## 💾 FILES IN /mnt/user-data/outputs/ (Current Work)

**All completed modules:**
- Network: wifi_manager.cpp/h, mqtt_manager.cpp/h, web_server.cpp/h
- Control: pid_controller.cpp/h, system_state.cpp/h, scheduler.cpp/h
- Hardware: tft_display.cpp/h
- Main: main.cpp (current version)
- Docs: PROJECT_FILE_STRUCTURE.md

**These are ready to upload to ESP32!**

---

## 🎬 SUGGESTED SESSION START

**Optimal opening message:**

```
Continue ESP32 thermostat modular refactoring.

Project context: Read /mnt/user-data/outputs/PROJECT_FILE_STRUCTURE.md

Status: Phases 2-4 complete (network, control, display modules done).

Today's goal: Complete Phase 5 (hardware abstraction) - extract temp_sensor 
and dimmer_control modules from main.cpp.

Files available in /mnt/user-data/outputs/

Start by creating temp_sensor module following the pattern from pid_controller.
Use view tool to check main.cpp readTemperature() function first.

Let's maximize coding time - create files directly, minimal explanations.
```

This will:
- ✅ Load all context efficiently
- ✅ Set clear goals
- ✅ Point to existing patterns to follow
- ✅ Encourage action over explanation
- ✅ Save ~500 tokens vs explaining everything

---

## 📋 PHASE 7 PREVIEW (For Later Sessions)

**After Phase 5 & 6 complete, next features:**

1. **Dual Dimmer System** (v1.5.0)
   - Add second dimmer for lighting control
   - Implement Day/Night scheduling
   - Smooth transitions (sunrise/sunset simulation)
   - See: `DUAL_DIMMER_DESIGN.md`

2. **Android App Development**
   - mDNS device discovery (ESP32 already supports this)
   - Live temperature display
   - Multi-device management
   - User requested this feature

3. **Temperature History/Graphing**
   - Re-implement with optimization
   - Chart.js visualization
   - Export to CSV

---

## 🔑 KEY INFORMATION QUICK REFERENCE

**Hardware:**
- ESP32 WROOM
- DS18B20 temp sensor (GPIO 4)
- RobotDyn AC Dimmer (GPIO 5 PWM, GPIO 27 Z-Cross)
- JC2432S028 TFT 2.8" (ILI9341, touchscreen)

**Network:**
- WiFi SSID: mesh
- MQTT Broker: 192.168.1.123
- Device IP: 192.168.1.236
- mDNS: havoc.local

**Current Version:** v1.4.0 (modular refactor in progress)

**GitHub:** https://github.com/cheew/Claude-ESP32-Thermostat

**Development:**
- Platform: PlatformIO in VS Code
- Serial: 115200 baud
- Framework: Arduino

---

## ⚡ SPEED OPTIMIZATION TIPS

**For Claude:**
1. Use `str_replace` instead of recreating entire files
2. Reference existing modules as templates
3. Create .h and .cpp together in one response
4. Use `bash_tool` to verify no duplicate code
5. Test compile incrementally

**For User:**
1. Have VS Code and Serial Monitor open
2. Keep platformio.ini unchanged (auto-detects new files)
3. Organize files into correct directories as you go
4. Quick test: Upload → Check serial → Verify one feature
5. Save full testing for end of session

---

## 📊 SUCCESS METRICS

**End of session goals:**
- [ ] All phases 1-6 complete
- [ ] main.cpp under 300 lines
- [ ] All modules in correct directories
- [ ] System compiles without warnings
- [ ] Full system test passed
- [ ] Documentation updated

**Code quality:**
- [ ] No global variables in main.cpp (except timing)
- [ ] All hardware access through modules
- [ ] Consistent error handling
- [ ] Clean separation of concerns

---

## 🎓 LESSONS LEARNED (Apply Tomorrow)

**What worked well:**
- Creating modules in pairs (controller + state together)
- Using existing modules as templates
- Incremental testing of features
- Clear file structure documentation

**What slowed us down:**
- Over-explaining changes (minimize this)
- Recreating entire files (use str_replace more)
- Not checking for leftover references (use grep first)
- C99 initializer syntax errors (learned to avoid)

**Apply tomorrow:**
- Start coding immediately after context load
- Use patterns from completed modules
- Check for references before/after extraction
- Test compile every 2-3 file changes

---

## 🚦 GO/NO-GO CHECKLIST

**Before starting Phase 5, verify:**
- [ ] All Phase 2-4 files in /mnt/user-data/outputs/
- [ ] main.cpp compiles with current modules
- [ ] File structure document available
- [ ] Project status document loaded

**If any NO → Load that file first!**

---

## 📞 EMERGENCY REFERENCES

**If something breaks:**
1. Check `/mnt/project/ESP32_Thermostat_v1_3_3_mobile_fix.cpp` for original working code
2. Use `grep -n "function_name"` to find where it was
3. Compare with current module implementation
4. Check for missing includes or forward declarations

**Common fixes:**
- Missing `#include` → Add to top of .cpp file
- Undefined reference → Check function is in .h file
- Duplicate definition → Remove old code from main.cpp
- Wrong namespace → Check Preferences, OneWire, etc.

---

## 🎯 FINAL REMINDER

**Goal:** Finish modular refactoring efficiently

**Strategy:** 
- Maximize coding time
- Minimize explanation time
- Use existing patterns
- Test incrementally
- Document at end

**Expected outcome:**
- Professional, maintainable codebase
- All features working
- Ready for dual dimmer feature
- Easy to extend and test

---

**Ready to code!** 🚀

*Session prep complete. Copy this file to project directory and reference it at start of next session for maximum efficiency.*
