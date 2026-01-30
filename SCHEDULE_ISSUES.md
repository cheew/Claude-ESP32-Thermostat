# Schedule Page Issues and Suggested Fixes

Analysis performed: 2026-01-29

## Summary

The schedule functionality has several interconnected bugs preventing the `days` field from being saved, loaded, or used in schedule logic. The UI allows selecting days, but they are never persisted or applied.

---

## Issue 1: `days` Field Not Saved to Preferences

**Location:** `src/control/output_manager.cpp` lines 802-816

**Problem:** The `output_manager_save_config()` function saves schedule slots but omits the `days` field entirely.

**Current Code:**
```cpp
// Save schedule
for (int j = 0; j < MAX_SCHEDULE_SLOTS; j++) {
    char key[16];
    snprintf(key, sizeof(key), "sch%d_en", j);
    prefs.putBool(key, outputs[i].schedule[j].enabled);
    // ... saves hour, minute, targetTemp
    // MISSING: days field not saved
}
```

**Suggested Fix:** Add after line 815:
```cpp
snprintf(key, sizeof(key), "sch%d_days", j);
prefs.putString(key, outputs[i].schedule[j].days);
```

---

## Issue 2: `days` Field Not Loaded from Preferences

**Location:** `src/control/output_manager.cpp` lines 741-755

**Problem:** The `output_manager_load_config()` function loads schedule slots but omits the `days` field.

**Suggested Fix:** Add after line 754:
```cpp
snprintf(key, sizeof(key), "sch%d_days", j);
String days = prefs.getString(key, "");
strncpy(outputs[i].schedule[j].days, days.c_str(), sizeof(outputs[i].schedule[j].days) - 1);
```

---

## Issue 3: `output_manager_set_schedule_slot()` Missing `days` Parameter

**Location:** `src/control/output_manager.cpp` lines 674-692, `include/output_manager.h` lines 255-256

**Problem:** The function signature doesn't include a `days` parameter, so the web server cannot pass it.

**Current Signature:**
```cpp
bool output_manager_set_schedule_slot(int outputIndex, int slotIndex,
                                      bool enabled, uint8_t hour, uint8_t minute, float targetTemp);
```

**Suggested Fix:** Update to:
```cpp
bool output_manager_set_schedule_slot(int outputIndex, int slotIndex,
                                      bool enabled, uint8_t hour, uint8_t minute,
                                      float targetTemp, const char* days);
```

And add in the implementation:
```cpp
strncpy(outputs[outputIndex].schedule[slotIndex].days, days ? days : "",
        sizeof(outputs[outputIndex].schedule[slotIndex].days) - 1);
```

---

## Issue 4: `updateSchedule()` Ignores Day-of-Week

**Location:** `src/control/output_manager.cpp` lines 423-474

**Problem:** The schedule logic only checks time (hour:minute) but completely ignores the `days` field. A schedule set for "weekends only" will run every day.

**Current Logic (simplified):**
```cpp
for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
    if (!output->schedule[i].enabled) continue;
    // Only checks time, never checks days!
    int slotTotalMinutes = output->schedule[i].hour * 60 + output->schedule[i].minute;
    // ...
}
```

**Suggested Fix:** Add day-of-week check before time comparison:
```cpp
// Get current day character
const char dayChars[] = "SMTWTFS";  // Sunday=0 through Saturday=6
char currentDayChar = dayChars[timeinfo.tm_wday];

for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
    if (!output->schedule[i].enabled) continue;

    // Check if current day is in the slot's days string
    if (strlen(output->schedule[i].days) > 0 &&
        strchr(output->schedule[i].days, currentDayChar) == NULL) {
        continue;  // This slot doesn't apply today
    }

    // ... rest of time comparison logic
}
```

---

## Issue 5: API Handler Ignores `days` Field When Saving

**Location:** `src/network/web_server.cpp` lines 2236-2247

**Problem:** The `handleOutputConfig()` POST handler reads schedule data but doesn't extract or pass the `days` field.

**Current Code:**
```cpp
JsonObject slot = schedule[i];
bool enabled = slot["enabled"];
int hour = slot["hour"];
int minute = slot["minute"];
float target = slot["target"];
output_manager_set_schedule_slot(outputIndex, i, enabled, hour, minute, target);
// MISSING: days field not read
```

**Suggested Fix:**
```cpp
JsonObject slot = schedule[i];
bool enabled = slot["enabled"];
int hour = slot["hour"];
int minute = slot["minute"];
float target = slot["targetTemp"];  // Also note: UI sends "targetTemp" not "target"
const char* days = slot["days"] | "";
output_manager_set_schedule_slot(outputIndex, i, enabled, hour, minute, target, days);
```

---

## Issue 6: JSON Field Name Mismatch

**Location:**
- JavaScript (web_server.cpp ~line 1626): sends `targetTemp`
- API handler (web_server.cpp line 2244): reads `target`

**Problem:** The UI JavaScript sends `targetTemp` but the backend expects `target`.

**JavaScript sends:**
```javascript
schedule.push({enabled:enabled, hour:hour, minute:minute, targetTemp:targetTemp, days:days});
```

**API reads:**
```cpp
float target = slot["target"];  // Wrong key!
```

**Suggested Fix:** Change line 2244 to:
```cpp
float target = slot["targetTemp"];
```

---

## Issue 7: API Response Truncation (Possible Buffer Issue)

**Observation:** The `/api/output/{id}` endpoint returns only 5 schedule slots instead of 8. The last entries appear as empty `{}` objects.

**Example Response:**
```json
"schedule":[
  {"enabled":false,"hour":0,"minute":0,"targetTemp":25.0,"days":""},
  {"enabled":false,"hour":0,"minute":0,"targetTemp":25.0,"days":""},
  {"enabled":false,"hour":0,"minute":0,"targetTemp":25.0,"days":""},
  {"enabled":false,"hour":0,"minute":0,"targetTemp":25.0,"days":""},
  {"enabled":false}  // Truncated!
]
```

**Location:** `src/network/web_server.cpp` line ~2087-2100

**Likely Cause:** The `StaticJsonDocument` buffer size may be too small for 8 full schedule slot objects.

**Suggested Fix:** Check/increase the JSON document size:
```cpp
// If currently using StaticJsonDocument<2048>, try:
StaticJsonDocument<4096> doc;  // Or use DynamicJsonDocument
```

---

## Issue 8: Schedule Link Hidden in Simple Mode

**Location:** `src/network/web_server.cpp` lines 2462-2465

**Problem:** The Schedule page link only appears when `advancedMode` is true, but schedules are a core feature that should be accessible in Simple mode too.

**Current Code:**
```cpp
if (advancedMode) {
    // ... other advanced links
    nav += "<a href='/schedule' ...>📅 Schedule</a>";
}
```

**Suggested Fix:** Move Schedule link outside the `advancedMode` block, perhaps after the Home link:
```cpp
nav += "<a href='/' ...>🏠 Home</a>";
nav += "<a href='/schedule' ...>📅 Schedule</a>";  // Always visible

if (advancedMode) {
    // ... other advanced-only links
}
```

---

## Priority Order for Fixes

1. **Issue 3 & 5**: Add `days` parameter to `output_manager_set_schedule_slot()` and update API handler - enables data flow
2. **Issue 1 & 2**: Add save/load for `days` field - enables persistence
3. **Issue 4**: Update `updateSchedule()` to check day-of-week - makes schedules actually work
4. **Issue 6**: Fix JSON field name mismatch (`targetTemp` vs `target`)
5. **Issue 7**: Fix JSON buffer truncation
6. **Issue 8**: Make Schedule link visible in Simple mode

---

## Testing Checklist

After implementing fixes:

- [ ] Create a schedule slot with specific days (e.g., "MWF")
- [ ] Save and verify no errors
- [ ] Refresh page - days should persist
- [ ] Reboot ESP32 - days should still be saved
- [ ] Verify schedule only activates on selected days
- [ ] Check API returns all 8 slots with complete data
- [ ] Verify Schedule link visible in both Simple and Advanced modes
