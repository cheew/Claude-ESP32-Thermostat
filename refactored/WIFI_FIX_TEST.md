# WiFi Reconnection Fix - Testing Guide

**Version:** 1.9.1
**Date:** January 11, 2026

---

## What Was Fixed

**Issue:** Device would not automatically reconnect to WiFi after connection loss. It would stay in AP mode indefinitely, requiring a manual power cycle.

**Root Cause:** The `wifi_task()` function in [wifi_manager.cpp](src/network/wifi_manager.cpp) would return immediately when in AP mode, never attempting to reconnect.

**Fix:** Modified `wifi_task()` to periodically attempt reconnection every 30 seconds even when in AP mode.

---

## Code Changes

### Before (Broken):
```cpp
void wifi_task(void) {
    // Don't reconnect if in AP mode
    if (apMode) {
        return;  // ← BUG: Never attempts to reconnect!
    }
    // Reconnection logic never executes
}
```

### After (Fixed):
```cpp
void wifi_task(void) {
    // In AP mode: periodically try to reconnect to saved WiFi
    if (apMode) {
        if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
            Serial.println("[WiFi] AP mode: Attempting to reconnect to WiFi");
            wifi_connect(NULL, NULL);  // Will switch out of AP mode if successful
        }
        return;
    }
    // Normal reconnection logic continues...
}
```

---

## Testing Procedure

### Test 1: Normal WiFi Loss and Recovery

**Setup:**
1. Build and upload firmware v1.9.1 to ESP32
2. Open Serial Monitor (115200 baud)
3. Ensure ESP32 is connected to WiFi normally
4. Verify web interface is accessible at normal IP (e.g., http://192.168.1.236)

**Test Steps:**
1. **Disable WiFi on your router** (or disconnect router from power)
2. **Observe Serial Monitor:**
   - Should see: `[WiFi] Connection lost`
   - Should see: `[WiFi] No saved credentials, starting AP mode` or `[WiFi] Connection failed, starting AP mode`
   - Should see: `[WiFi] AP SSID: ReptileThermostat`
   - Should see: `[WiFi] AP IP: 192.168.4.1`

3. **Wait 30 seconds**

4. **Re-enable WiFi on your router**

5. **Continue watching Serial Monitor:**
   - Should see: `[WiFi] AP mode: Attempting to reconnect to WiFi`
   - Should see: `[WiFi] Connecting to: mesh` (or your SSID)
   - Should see: `[WiFi] Connected successfully`
   - Should see: `[WiFi] IP address: 192.168.1.236` (or your assigned IP)
   - Should see: `[WiFi] Connection established`

6. **Verify web interface is accessible again** at original IP address

**Expected Result:** ✅ Device automatically reconnects within 30-60 seconds of WiFi becoming available.

**Pass Criteria:**
- Device enters AP mode when WiFi is lost ✓
- Device automatically reconnects when WiFi returns ✓
- No manual power cycle required ✓
- Web interface accessible after reconnection ✓

---

### Test 2: Console Event Logging

**Setup:**
1. Ensure firmware is running
2. Navigate to http://192.168.1.236/console (or your device IP)

**Test Steps:**
1. **Disable WiFi on router**
2. **Watch Console page:**
   - Should see: `[WIFI] Connection lost`
   - Should see: `[SYSTEM] Starting Access Point mode`

3. **Wait 30 seconds**

4. **Re-enable WiFi on router**

5. **Watch Console page (auto-refreshes every 2 seconds):**
   - Should see: `[WIFI] AP mode: Attempting to reconnect to WiFi`
   - Should see: `[WIFI] Connected successfully`
   - Should see: `[WIFI] IP address: 192.168.1.236`

**Expected Result:** ✅ Console shows WiFi reconnection events in real-time.

---

### Test 3: Multiple Reconnection Attempts

**Setup:**
1. Device running in AP mode (WiFi unavailable)
2. Serial Monitor open

**Test Steps:**
1. **Leave WiFi disabled** (router off or SSID changed)
2. **Watch Serial Monitor for 2 minutes**
3. **Observe reconnection attempts every 30 seconds:**
   - t=0s: `[WiFi] AP mode: Attempting to reconnect to WiFi`
   - t=30s: `[WiFi] AP mode: Attempting to reconnect to WiFi`
   - t=60s: `[WiFi] AP mode: Attempting to reconnect to WiFi`
   - t=90s: `[WiFi] AP mode: Attempting to reconnect to WiFi`

**Expected Result:** ✅ Device continuously attempts reconnection every 30 seconds without giving up.

---

### Test 4: MQTT Reconnection After WiFi Recovery

**Setup:**
1. MQTT broker configured and running (e.g., Home Assistant)
2. Device connected to WiFi with MQTT working
3. Serial Monitor open

**Test Steps:**
1. **Disable WiFi on router**
2. **Wait 30 seconds** (device enters AP mode)
3. **Re-enable WiFi on router**
4. **Wait for WiFi reconnection** (~30-60 seconds)
5. **Observe Serial Monitor:**
   - Should see: `[MQTT] Connecting to MQTT broker...`
   - Should see: `[MQTT] Connected to MQTT broker`
   - Should see: `[MQTT] Sending Home Assistant discovery...`

6. **Check Home Assistant:**
   - Thermostat entity should show as "Available" (not "Unavailable")
   - Temperature readings should resume

**Expected Result:** ✅ MQTT automatically reconnects after WiFi is restored.

---

### Test 5: Web Server Accessibility

**Setup:**
1. Device running in AP mode
2. Computer connected to ReptileThermostat WiFi network

**Test Steps:**
1. **Navigate to http://192.168.4.1** (AP mode IP)
2. **Verify web interface loads**
3. **Enable WiFi on router**
4. **Wait for device to reconnect** (watch for WiFi disconnect from ReptileThermostat AP)
5. **Reconnect computer to normal WiFi**
6. **Navigate to http://192.168.1.236** (or original IP)
7. **Verify web interface loads**

**Expected Result:** ✅ Web interface accessible in both AP mode and after reconnection.

---

## Troubleshooting

### Issue: Device doesn't reconnect after WiFi returns

**Check:**
1. Are WiFi credentials saved in preferences?
   - Navigate to Settings page → WiFi section
   - Verify SSID is displayed
2. Is the SSID name correct?
   - Router might have changed SSID
3. Is WiFi password correct?
   - May need to re-enter in Settings
4. Check Serial Monitor for error messages
5. Check Console page for WiFi events

**Solution:** If credentials are wrong, manually update in Settings page or connect to AP mode and reconfigure.

---

### Issue: Takes longer than 60 seconds to reconnect

**Explanation:** Reconnection attempts occur every 30 seconds. If the attempt happens just before WiFi becomes available, it may take up to 60 seconds (30s wait + 30s next attempt).

**This is normal behavior.**

---

### Issue: Device reconnects but MQTT doesn't

**Check:**
1. Is MQTT broker running and accessible?
2. Check MQTT broker logs for connection attempts
3. Verify MQTT credentials haven't changed
4. Check Serial Monitor for MQTT connection errors

**Solution:** MQTT reconnection happens automatically in the main loop. If broker is accessible, it should reconnect within 10-15 seconds of WiFi restoration.

---

## Regression Testing

Ensure existing features still work after WiFi fix:

- [ ] Normal WiFi connection on boot (no AP mode)
- [ ] Web interface loads correctly
- [ ] Temperature readings display
- [ ] Dark mode toggle works
- [ ] Schedule page loads and saves
- [ ] MQTT publishes to Home Assistant
- [ ] Console page shows events
- [ ] Temperature graph displays
- [ ] Settings page saves preferences

---

## Success Criteria Summary

✅ All tests pass
✅ Device automatically reconnects to WiFi after connection loss
✅ No manual power cycle required
✅ Serial Monitor shows reconnection attempts
✅ Console page logs WiFi events
✅ MQTT reconnects automatically
✅ Web interface accessible after reconnection
✅ No regression in existing features

---

## Next Steps After Testing

If all tests pass:
1. Commit changes to git with message: `v1.9.1 - Fix WiFi reconnection bug`
2. Tag release: `git tag v1.9.1`
3. Update README.md to reflect known issue is fixed
4. Proceed with multi-output implementation

If tests fail:
1. Document failure in issue tracker
2. Review wifi_manager.cpp changes
3. Check for timing issues (CONNECTION_RETRY_INTERVAL)
4. Consider adding more debug logging

---

**Test Status:** ⏳ Awaiting testing

**Tester:** ___________

**Date Tested:** ___________

**Result:** [ ] PASS  [ ] FAIL

**Notes:**
