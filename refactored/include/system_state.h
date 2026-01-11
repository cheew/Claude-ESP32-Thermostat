/**
 * system_state.h
 * Centralized System State Management
 * 
 * Single source of truth for all thermostat state:
 * - Current/target temperature
 * - Operating mode
 * - Heating state
 * - Power output
 */

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Preferences.h>

// Operating modes
#define MODE_AUTO "auto"
#define MODE_ON "on"
#define MODE_OFF "off"

// System state structure
typedef struct {
    float currentTemp;      // Current temperature reading
    float targetTemp;       // Target temperature setpoint
    bool heating;           // true if actively heating
    char mode[8];          // Operating mode (auto/on/off)
    int power;             // Power output percentage (0-100)
} SystemState_t;

// PID tuning parameters
typedef struct {
    float Kp;
    float Ki;
    float Kd;
} PIDGains_t;

/**
 * Initialize system state manager
 * Loads saved settings from preferences
 */
void state_init(void);

/**
 * Get pointer to current system state
 * @return Pointer to state structure (read-only recommended)
 */
SystemState_t* state_get(void);

/**
 * Update current temperature
 * @param temp Current temperature in °C
 */
void state_set_current_temp(float temp);

/**
 * Update target temperature
 * @param temp Target temperature in °C
 * @param save If true, save to preferences
 */
void state_set_target_temp(float temp, bool save);

/**
 * Update operating mode
 * @param mode Mode string (auto/on/off)
 * @param save If true, save to preferences
 */
void state_set_mode(const char* mode, bool save);

/**
 * Update heating state
 * @param heating true if heating active
 */
void state_set_heating(bool heating);

/**
 * Update power output
 * @param power Power percentage (0-100)
 */
void state_set_power(int power);

/**
 * Get device name
 * @param buffer Output buffer for device name
 * @param maxLen Maximum buffer length
 */
void state_get_device_name(char* buffer, size_t maxLen);

/**
 * Set device name
 * @param name New device name
 */
void state_set_device_name(const char* name);

/**
 * Load PID gains from preferences
 * @param gains Output structure for gains
 */
void state_load_pid_gains(PIDGains_t* gains);

/**
 * Save PID gains to preferences
 * @param gains PID gain values to save
 */
void state_save_pid_gains(const PIDGains_t* gains);

/**
 * Save all current state to preferences
 * (target temp and mode)
 */
void state_save_to_preferences(void);

/**
 * Load all state from preferences
 * (target temp, mode, device name, PID gains)
 */
void state_load_from_preferences(void);

/**
 * Reset all settings to defaults
 */
void state_reset_to_defaults(void);

#endif // SYSTEM_STATE_H
