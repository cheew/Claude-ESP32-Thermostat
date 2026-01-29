/**
 * safety_manager.h
 * System-level Safety Management
 *
 * Provides:
 * - Hardware watchdog timer
 * - Boot loop detection
 * - Safe mode operation
 * - Emergency shutdown capability
 */

#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include <Arduino.h>

// Watchdog timeout in seconds (must be fed within this time)
#define WATCHDOG_TIMEOUT_SEC 30

// Boot loop detection thresholds
#define BOOT_LOOP_THRESHOLD 3          // Number of rapid reboots before safe mode
#define BOOT_STABLE_TIME_SEC 60        // Time before boot is considered stable
#define BOOT_WINDOW_SEC 300            // Time window to count rapid reboots (5 min)

/**
 * Safe mode reasons
 */
typedef enum {
    SAFE_MODE_NONE = 0,
    SAFE_MODE_BOOT_LOOP,       // Too many rapid reboots
    SAFE_MODE_WATCHDOG,        // Watchdog triggered (detected on next boot)
    SAFE_MODE_USER_REQUESTED,  // User requested via web UI
    SAFE_MODE_CRITICAL_FAULT   // Critical fault (e.g., all sensors failed)
} SafeModeReason_t;

/**
 * Safety manager state
 */
typedef struct {
    bool safeMode;                    // Currently in safe mode
    SafeModeReason_t safeModeReason;  // Why we're in safe mode
    uint8_t bootCount;                // Boot count within window
    unsigned long lastBootTime;       // Timestamp of last boot
    unsigned long stableTime;         // When boot became stable (0 if not yet)
    bool watchdogEnabled;             // Watchdog is active
    unsigned long lastWatchdogFeed;   // Last watchdog feed time
} SafetyState_t;

/**
 * Initialize safety manager
 * Call early in setup() before other initialization
 * @return true if normal boot, false if entering safe mode
 */
bool safety_manager_init(void);

/**
 * Feed the watchdog
 * Call regularly in main loop to prevent reset
 */
void safety_manager_feed_watchdog(void);

/**
 * Mark boot as stable
 * Call after successful initialization to clear boot counter
 */
void safety_manager_mark_stable(void);

/**
 * Check if in safe mode
 * @return true if in safe mode
 */
bool safety_manager_is_safe_mode(void);

/**
 * Get safe mode reason
 * @return Reason for safe mode, or SAFE_MODE_NONE
 */
SafeModeReason_t safety_manager_get_safe_mode_reason(void);

/**
 * Get safe mode reason as string
 * @param reason Safe mode reason
 * @return Human-readable string
 */
const char* safety_manager_get_reason_name(SafeModeReason_t reason);

/**
 * Request safe mode (from web UI or other source)
 * Forces all outputs OFF on next boot
 */
void safety_manager_request_safe_mode(void);

/**
 * Exit safe mode
 * Clears safe mode flag and resets boot counter
 * @return true if exited, false if conditions don't allow
 */
bool safety_manager_exit_safe_mode(void);

/**
 * Emergency all outputs OFF
 * Immediately disables all outputs (does not require reboot)
 */
void safety_manager_emergency_stop(void);

/**
 * Get current safety state
 * @return Pointer to safety state structure
 */
const SafetyState_t* safety_manager_get_state(void);

/**
 * Get boot count
 * @return Number of boots in current window
 */
uint8_t safety_manager_get_boot_count(void);

/**
 * Check if watchdog is enabled
 * @return true if watchdog is active
 */
bool safety_manager_is_watchdog_enabled(void);

/**
 * Get time since last watchdog feed
 * @return Milliseconds since last feed
 */
unsigned long safety_manager_get_watchdog_margin(void);

#endif // SAFETY_MANAGER_H
