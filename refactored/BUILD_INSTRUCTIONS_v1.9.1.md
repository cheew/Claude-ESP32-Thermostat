# Build Instructions - v1.9.1

**WiFi Reconnection Fix**
**Date:** January 11, 2026

---

## Summary of Changes

### Files Modified:
1. **[src/network/wifi_manager.cpp](src/network/wifi_manager.cpp)**
   - Fixed `wifi_task()` function to attempt reconnection when in AP mode
   - Now tries every 30 seconds instead of never

2. **[src/main.cpp](src/main.cpp)**
   - Updated version to 1.9.1
   - Added changelog notes

### Files Created:
1. **[CHANGELOG.md](CHANGELOG.md)** - Complete project changelog
2. **[WIFI_FIX_TEST.md](WIFI_FIX_TEST.md)** - Comprehensive testing guide
3. **[BUILD_INSTRUCTIONS_v1.9.1.md](BUILD_INSTRUCTIONS_v1.9.1.md)** - This file

---

## Build Steps

### 1. Open Project in VS Code

```bash
cd c:\Users\admin\Documents\PlatformIO\Projects\Claude-ESP32-Thermostat\refactored
code .
```

### 2. Verify PlatformIO Extension is Active

- Look for PlatformIO icon in left sidebar
- Ensure project is recognized

### 3. Build Firmware

**Option A: Using PlatformIO Toolbar**
- Click PlatformIO icon (alien head) in left sidebar
- Expand "PROJECT TASKS" → "esp32dev"
- Click "Build"

**Option B: Using Command Palette**
- Press `Ctrl+Shift+P` (Windows) or `Cmd+Shift+P` (Mac)
- Type: "PlatformIO: Build"
- Press Enter

**Option C: Using Terminal**
- Open terminal in VS Code
- Run: `platformio run`

### 4. Expected Build Output

```
Processing esp32dev (platform: espressif32; board: esp32dev; framework: arduino)
...
Compiling .pio/build/esp32dev/src/network/wifi_manager.cpp.o
Compiling .pio/build/esp32dev/src/main.cpp.o
...
Linking .pio/build/esp32dev/firmware.elf
Building .pio/build/esp32dev/firmware.bin
...
RAM:   [==        ]  19.8% (used 64952 bytes from 327680 bytes)
Flash: [========= ]  85.7% (used 1113521 bytes from 1310720 bytes)
========================= [SUCCESS] Took X.XX seconds =========================
```

**Memory Expectations:**
- Flash: ~85.7% (no change from v1.9.0)
- RAM: ~19.8% (no change from v1.9.0)

### 5. Upload to ESP32

**Prerequisites:**
- ESP32 connected via USB
- Correct COM port selected in VS Code

**Option A: Using PlatformIO Toolbar**
- Click PlatformIO icon
- Expand "PROJECT TASKS" → "esp32dev"
- Click "Upload"

**Option B: Using Command Palette**
- Press `Ctrl+Shift+P`
- Type: "PlatformIO: Upload"
- Press Enter

**Option C: Using Terminal**
- Run: `platformio run -t upload`

### 6. Open Serial Monitor

**Option A: Using PlatformIO Toolbar**
- Click PlatformIO icon
- Expand "PROJECT TASKS" → "esp32dev"
- Click "Monitor"

**Option B: Using Command Palette**
- Press `Ctrl+Shift+P`
- Type: "PlatformIO: Monitor"
- Press Enter

**Option C: Using Terminal**
- Run: `platformio device monitor`

**Expected Output:**
```
=== ESP32 Reptile Thermostat v1.9.1 ===
[WiFi] Initializing WiFi manager
[WiFi] Connecting with saved credentials
[WiFi] Connecting to: mesh
..................
[WiFi] Connected successfully
[WiFi] IP address: 192.168.1.236
[WiFi] Connection established
[WiFi] Setting up mDNS: havoc.local
[WiFi] mDNS responder started successfully
[MQTT] Connecting to MQTT broker...
[MQTT] Connected to MQTT broker
...
```

---

## Verification Steps

### 1. Check Version

- Open web interface: http://192.168.1.236
- Navigate to Info page
- Verify version shows: **1.9.1**

### 2. Check Console Logging

- Navigate to Console page: http://192.168.1.236/console
- Should see recent system events
- Should see "System boot - v1.9.1" message

### 3. Quick WiFi Test

1. Disconnect WiFi router (or disable WiFi)
2. Watch Serial Monitor
3. Should see: `[WiFi] Connection lost`
4. Should see: `[WiFi] Starting Access Point mode`
5. Wait 30 seconds
6. Re-enable WiFi
7. Should see: `[WiFi] AP mode: Attempting to reconnect to WiFi`
8. Should see: `[WiFi] Connected successfully`

**If this works, the fix is successful!**

---

## If Build Fails

### Common Issues:

**Issue: "PlatformIO: Command not found"**
- **Solution:** Install PlatformIO extension in VS Code
  - Open Extensions (Ctrl+Shift+X)
  - Search "PlatformIO IDE"
  - Click Install

**Issue: "Multiple definition errors"**
- **Solution:** Clean build directory
  - Run: `platformio run -t clean`
  - Then build again

**Issue: "Cannot find 'wifi_manager.h'"**
- **Solution:** Check file paths
  - Ensure [include/wifi_manager.h](include/wifi_manager.h) exists
  - Ensure [src/network/wifi_manager.cpp](src/network/wifi_manager.cpp) exists

**Issue: "Port not found / Cannot upload"**
- **Solution:** Check USB connection
  - Ensure ESP32 is connected via USB
  - Check Device Manager (Windows) for COM port
  - Update platformio.ini with correct port if needed

---

## Rollback Instructions

If v1.9.1 has issues and you need to rollback:

### Option 1: Git Rollback
```bash
git checkout v1.9.0
platformio run -t upload
```

### Option 2: Manual Revert
1. Open [src/network/wifi_manager.cpp](src/network/wifi_manager.cpp)
2. In `wifi_task()` function, change:
   ```cpp
   // In AP mode: periodically try to reconnect to saved WiFi
   if (apMode) {
       if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
           Serial.println("[WiFi] AP mode: Attempting to reconnect to WiFi");
           wifi_connect(NULL, NULL);
       }
       return;
   }
   ```
   To:
   ```cpp
   // Don't reconnect if in AP mode
   if (apMode) {
       return;
   }
   ```
3. Update version in [src/main.cpp](src/main.cpp) back to "1.9.0"
4. Rebuild and upload

---

## Next Steps After Successful Build

1. **Complete Testing** (see [WIFI_FIX_TEST.md](WIFI_FIX_TEST.md))
   - Test WiFi reconnection
   - Test console logging
   - Test MQTT reconnection
   - Verify all existing features work

2. **Commit Changes**
   ```bash
   git add .
   git commit -m "v1.9.1 - Fix WiFi reconnection bug"
   git tag v1.9.1
   git push origin main
   git push origin v1.9.1
   ```

3. **Update Documentation**
   - Mark WiFi reconnection issue as FIXED in README
   - Update SCOPE.md to reflect completed item

4. **Proceed with Multi-Output Implementation**
   - See [NEXT_STEPS.md](NEXT_STEPS.md) for planning
   - See [SCOPE.md](SCOPE.md) for detailed specifications

---

## Build Checklist

- [ ] Project opened in VS Code with PlatformIO
- [ ] Build completed successfully
- [ ] No compilation errors or warnings
- [ ] Memory usage within acceptable limits (Flash <95%, RAM <25%)
- [ ] Firmware uploaded to ESP32
- [ ] Serial monitor shows v1.9.1 boot message
- [ ] Web interface accessible
- [ ] Info page shows version 1.9.1
- [ ] Console page accessible and showing events
- [ ] Basic WiFi reconnection test passed
- [ ] All existing features working

---

**Build Status:** ⏳ Ready to build

**Built By:** ___________

**Build Date:** ___________

**Build Result:** [ ] SUCCESS  [ ] FAILED

**Upload Result:** [ ] SUCCESS  [ ] FAILED

**Notes:**
