/**
 * safety_manager.cpp
 * System-level Safety Management
 *
 * Implements hardware watchdog, boot loop detection, and safe mode
 */

#include "safety_manager.h"
#include "output_manager.h"
#include "console.h"
#include <Preferences.h>
#include <esp_task_wdt.h>

// NVS namespace for safety data
#define SAFETY_NAMESPACE "safety"

// Keys for NVS storage
#define KEY_BOOT_COUNT "boot_cnt"
#define KEY_LAST_BOOT "last_boot"
#define KEY_SAFE_MODE "safe_mode"
#define KEY_SAFE_REASON "safe_reason"
#define KEY_WDT_RESET "wdt_reset"

// Internal state
static SafetyState_t safetyState = {
    .safeMode = false,
    .safeModeReason = SAFE_MODE_NONE,
    .bootCount = 0,
    .lastBootTime = 0,
    .stableTime = 0,
    .watchdogEnabled = false,
    .lastWatchdogFeed = 0
};

// Forward declarations
static void loadSafetyState(void);
static void saveSafetyState(void);
static void checkBootLoop(void);
static void initWatchdog(void);
static void enterSafeMode(SafeModeReason_t reason);

/**
 * Initialize safety manager
 */
bool safety_manager_init(void) {
    Serial.println("[SafetyMgr] Initializing...");

    // Load previous state from NVS
    loadSafetyState();

    // Check if watchdog triggered last boot
    Preferences prefs;
    prefs.begin(SAFETY_NAMESPACE, false);
    bool wdtReset = prefs.getBool(KEY_WDT_RESET, false);
    if (wdtReset) {
        Serial.println("[SafetyMgr] WARNING: Previous boot ended by watchdog!");
        prefs.putBool(KEY_WDT_RESET, false);  // Clear flag
        console_add_event(CONSOLE_EVENT_SYSTEM, "WATCHDOG: Previous boot timed out");

        // Increment boot count for watchdog-caused reboot
        safetyState.bootCount++;
    }
    prefs.end();

    // Check for boot loop
    checkBootLoop();

    // Check if already in safe mode (persisted)
    if (safetyState.safeMode) {
        Serial.printf("[SafetyMgr] SAFE MODE ACTIVE - Reason: %s\n",
                     safety_manager_get_reason_name(safetyState.safeModeReason));
        console_add_event_f(CONSOLE_EVENT_SYSTEM, "SAFE MODE: %s",
                           safety_manager_get_reason_name(safetyState.safeModeReason));

        // Force all outputs OFF in safe mode
        safety_manager_emergency_stop();
        return false;
    }

    // Initialize hardware watchdog
    initWatchdog();

    // Record this boot
    safetyState.lastBootTime = millis();
    saveSafetyState();

    Serial.printf("[SafetyMgr] Initialized (boot count: %d)\n", safetyState.bootCount);
    return true;
}

/**
 * Feed the watchdog
 */
void safety_manager_feed_watchdog(void) {
    if (safetyState.watchdogEnabled) {
        esp_task_wdt_reset();
        safetyState.lastWatchdogFeed = millis();
    }
}

/**
 * Mark boot as stable
 */
void safety_manager_mark_stable(void) {
    if (safetyState.stableTime == 0) {
        safetyState.stableTime = millis();
        safetyState.bootCount = 0;  // Reset boot counter
        saveSafetyState();

        Serial.println("[SafetyMgr] Boot marked as stable - boot counter reset");
        console_add_event(CONSOLE_EVENT_SYSTEM, "Boot stable - safety counters reset");
    }
}

/**
 * Check if in safe mode
 */
bool safety_manager_is_safe_mode(void) {
    return safetyState.safeMode;
}

/**
 * Get safe mode reason
 */
SafeModeReason_t safety_manager_get_safe_mode_reason(void) {
    return safetyState.safeModeReason;
}

/**
 * Get safe mode reason as string
 */
const char* safety_manager_get_reason_name(SafeModeReason_t reason) {
    switch (reason) {
        case SAFE_MODE_NONE:           return "None";
        case SAFE_MODE_BOOT_LOOP:      return "Boot Loop Detected";
        case SAFE_MODE_WATCHDOG:       return "Watchdog Reset";
        case SAFE_MODE_USER_REQUESTED: return "User Requested";
        case SAFE_MODE_CRITICAL_FAULT: return "Critical Fault";
        default:                       return "Unknown";
    }
}

/**
 * Request safe mode
 */
void safety_manager_request_safe_mode(void) {
    Preferences prefs;
    prefs.begin(SAFETY_NAMESPACE, false);
    prefs.putBool(KEY_SAFE_MODE, true);
    prefs.putUChar(KEY_SAFE_REASON, (uint8_t)SAFE_MODE_USER_REQUESTED);
    prefs.end();

    console_add_event(CONSOLE_EVENT_SYSTEM, "Safe mode requested - will activate on reboot");
    Serial.println("[SafetyMgr] Safe mode requested for next boot");
}

/**
 * Exit safe mode
 */
bool safety_manager_exit_safe_mode(void) {
    if (!safetyState.safeMode) {
        return true;  // Not in safe mode
    }

    // Clear safe mode state
    safetyState.safeMode = false;
    safetyState.safeModeReason = SAFE_MODE_NONE;
    safetyState.bootCount = 0;
    saveSafetyState();

    // Re-enable watchdog
    initWatchdog();

    console_add_event(CONSOLE_EVENT_SYSTEM, "Exited safe mode");
    Serial.println("[SafetyMgr] Exited safe mode");

    return true;
}

/**
 * Emergency all outputs OFF
 */
void safety_manager_emergency_stop(void) {
    Serial.println("[SafetyMgr] EMERGENCY STOP - All outputs OFF");
    console_add_event(CONSOLE_EVENT_SYSTEM, "EMERGENCY STOP - All outputs disabled");

    // Disable all outputs via output manager
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        output_manager_set_mode(i, CONTROL_MODE_OFF);
        output_manager_set_manual_power(i, 0);
    }
}

/**
 * Get current safety state
 */
const SafetyState_t* safety_manager_get_state(void) {
    return &safetyState;
}

/**
 * Get boot count
 */
uint8_t safety_manager_get_boot_count(void) {
    return safetyState.bootCount;
}

/**
 * Check if watchdog is enabled
 */
bool safety_manager_is_watchdog_enabled(void) {
    return safetyState.watchdogEnabled;
}

/**
 * Get time since last watchdog feed
 */
unsigned long safety_manager_get_watchdog_margin(void) {
    if (!safetyState.watchdogEnabled || safetyState.lastWatchdogFeed == 0) {
        return 0;
    }
    return millis() - safetyState.lastWatchdogFeed;
}

// ===== INTERNAL FUNCTIONS =====

/**
 * Load safety state from NVS
 */
static void loadSafetyState(void) {
    Preferences prefs;
    prefs.begin(SAFETY_NAMESPACE, true);  // Read-only

    safetyState.bootCount = prefs.getUChar(KEY_BOOT_COUNT, 0);
    safetyState.lastBootTime = prefs.getULong(KEY_LAST_BOOT, 0);
    safetyState.safeMode = prefs.getBool(KEY_SAFE_MODE, false);
    safetyState.safeModeReason = (SafeModeReason_t)prefs.getUChar(KEY_SAFE_REASON, 0);

    prefs.end();
}

/**
 * Save safety state to NVS
 */
static void saveSafetyState(void) {
    Preferences prefs;
    prefs.begin(SAFETY_NAMESPACE, false);  // Read-write

    prefs.putUChar(KEY_BOOT_COUNT, safetyState.bootCount);
    prefs.putULong(KEY_LAST_BOOT, safetyState.lastBootTime);
    prefs.putBool(KEY_SAFE_MODE, safetyState.safeMode);
    prefs.putUChar(KEY_SAFE_REASON, (uint8_t)safetyState.safeModeReason);

    prefs.end();
}

/**
 * Check for boot loop condition
 */
static void checkBootLoop(void) {
    // Increment boot count
    safetyState.bootCount++;

    // Check if we've exceeded threshold
    if (safetyState.bootCount >= BOOT_LOOP_THRESHOLD) {
        Serial.printf("[SafetyMgr] Boot loop detected! Count: %d\n", safetyState.bootCount);
        enterSafeMode(SAFE_MODE_BOOT_LOOP);
    }

    // Save updated count
    saveSafetyState();
}

/**
 * Initialize hardware watchdog
 */
static void initWatchdog(void) {
    // Don't enable watchdog in safe mode
    if (safetyState.safeMode) {
        Serial.println("[SafetyMgr] Watchdog disabled in safe mode");
        return;
    }

    // Configure ESP32 task watchdog
    esp_err_t err = esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);  // true = panic on timeout
    if (err == ESP_OK) {
        err = esp_task_wdt_add(NULL);  // Add current task (main loop)
        if (err == ESP_OK) {
            safetyState.watchdogEnabled = true;
            safetyState.lastWatchdogFeed = millis();
            Serial.printf("[SafetyMgr] Watchdog enabled (%d sec timeout)\n", WATCHDOG_TIMEOUT_SEC);
        } else {
            Serial.printf("[SafetyMgr] Failed to add task to watchdog: %d\n", err);
        }
    } else if (err == ESP_ERR_INVALID_STATE) {
        // Watchdog already initialized (e.g., by ESP-IDF), just add our task
        err = esp_task_wdt_add(NULL);
        if (err == ESP_OK || err == ESP_ERR_INVALID_ARG) {  // Already added is OK
            safetyState.watchdogEnabled = true;
            safetyState.lastWatchdogFeed = millis();
            Serial.println("[SafetyMgr] Watchdog already active, task registered");
        }
    } else {
        Serial.printf("[SafetyMgr] Failed to init watchdog: %d\n", err);
    }

    // Set flag so we can detect watchdog reset on next boot
    Preferences prefs;
    prefs.begin(SAFETY_NAMESPACE, false);
    prefs.putBool(KEY_WDT_RESET, true);  // Will be cleared if normal shutdown
    prefs.end();
}

/**
 * Enter safe mode
 */
static void enterSafeMode(SafeModeReason_t reason) {
    safetyState.safeMode = true;
    safetyState.safeModeReason = reason;
    saveSafetyState();

    Serial.printf("[SafetyMgr] ENTERING SAFE MODE: %s\n",
                 safety_manager_get_reason_name(reason));

    // Force all outputs OFF
    safety_manager_emergency_stop();
}
