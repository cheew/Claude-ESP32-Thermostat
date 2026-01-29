/**
 * system_state.cpp
 * Centralized System State Management Implementation
 */

#include "system_state.h"
#include <Arduino.h>

// Default values
#define DEFAULT_TARGET_TEMP 28.0
#define DEFAULT_MODE MODE_AUTO
#define DEFAULT_DEVICE_NAME "Thermostat"
#define DEFAULT_KP 10.0
#define DEFAULT_KI 0.5
#define DEFAULT_KD 5.0

// Global state
static SystemState_t systemState;

// Initialize in state_init() instead

// Preferences namespace
static const char* PREFS_NAMESPACE = "thermostat";

/**
 * Initialize state manager
 */
void state_init(void) {
    Serial.println("[State] Initializing system state manager");
    
    // Initialize state with defaults
    systemState.currentTemp = 0.0;
    systemState.targetTemp = DEFAULT_TARGET_TEMP;
    systemState.heating = false;
    strncpy(systemState.mode, DEFAULT_MODE, sizeof(systemState.mode) - 1);
    systemState.mode[sizeof(systemState.mode) - 1] = '\0';
    systemState.power = 0;
    
    // Load saved state from preferences
    state_load_from_preferences();
    
    Serial.printf("[State] Target: %.1f°C, Mode: %s\n", 
                  systemState.targetTemp, systemState.mode);
}

/**
 * Get current state
 */
SystemState_t* state_get(void) {
    return &systemState;
}

/**
 * Set current temperature
 */
void state_set_current_temp(float temp) {
    systemState.currentTemp = temp;
}

/**
 * Set target temperature
 */
void state_set_target_temp(float temp, bool save) {
    // Constrain to valid range
    systemState.targetTemp = constrain(temp, 15.0, 45.0);
    
    if (save) {
        Preferences prefs;
        prefs.begin(PREFS_NAMESPACE, false);
        prefs.putFloat("target", systemState.targetTemp);
        prefs.end();
        
        Serial.printf("[State] Target saved: %.1f°C\n", systemState.targetTemp);
    }
}

/**
 * Set operating mode
 */
void state_set_mode(const char* mode, bool save) {
    if (mode == NULL) return;
    
    // Validate mode
    if (strcmp(mode, MODE_AUTO) == 0 || 
        strcmp(mode, MODE_ON) == 0 || 
        strcmp(mode, MODE_OFF) == 0) {
        
        strncpy(systemState.mode, mode, sizeof(systemState.mode) - 1);
        systemState.mode[sizeof(systemState.mode) - 1] = '\0';
        
        if (save) {
            Preferences prefs;
            prefs.begin(PREFS_NAMESPACE, false);
            prefs.putString("mode", systemState.mode);
            prefs.end();
            
            Serial.printf("[State] Mode saved: %s\n", systemState.mode);
        }
    }
}

/**
 * Set heating state
 */
void state_set_heating(bool heating) {
    systemState.heating = heating;
}

/**
 * Set power output
 */
void state_set_power(int power) {
    systemState.power = constrain(power, 0, 100);
}

/**
 * Get device name
 */
void state_get_device_name(char* buffer, size_t maxLen) {
    if (buffer == NULL || maxLen == 0) return;
    
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    String name = prefs.getString("device_name", DEFAULT_DEVICE_NAME);
    prefs.end();
    
    strncpy(buffer, name.c_str(), maxLen - 1);
    buffer[maxLen - 1] = '\0';
}

/**
 * Set device name
 */
void state_set_device_name(const char* name) {
    if (name == NULL) return;
    
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putString("device_name", name);
    prefs.end();
    
    Serial.printf("[State] Device name saved: %s\n", name);
}

/**
 * Load PID gains
 */
void state_load_pid_gains(PIDGains_t* gains) {
    if (gains == NULL) return;
    
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    gains->Kp = prefs.getFloat("Kp", DEFAULT_KP);
    gains->Ki = prefs.getFloat("Ki", DEFAULT_KI);
    gains->Kd = prefs.getFloat("Kd", DEFAULT_KD);
    prefs.end();
    
    Serial.printf("[State] PID gains loaded - Kp:%.2f Ki:%.2f Kd:%.2f\n",
                  gains->Kp, gains->Ki, gains->Kd);
}

/**
 * Save PID gains
 */
void state_save_pid_gains(const PIDGains_t* gains) {
    if (gains == NULL) return;
    
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putFloat("Kp", gains->Kp);
    prefs.putFloat("Ki", gains->Ki);
    prefs.putFloat("Kd", gains->Kd);
    prefs.end();
    
    Serial.printf("[State] PID gains saved - Kp:%.2f Ki:%.2f Kd:%.2f\n",
                  gains->Kp, gains->Ki, gains->Kd);
}

/**
 * Save current state to preferences
 */
void state_save_to_preferences(void) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putFloat("target", systemState.targetTemp);
    prefs.putString("mode", systemState.mode);
    prefs.end();
    
    Serial.println("[State] State saved to preferences");
}

/**
 * Load state from preferences
 */
void state_load_from_preferences(void) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    
    systemState.targetTemp = prefs.getFloat("target", DEFAULT_TARGET_TEMP);
    String modeStr = prefs.getString("mode", DEFAULT_MODE);
    strncpy(systemState.mode, modeStr.c_str(), sizeof(systemState.mode) - 1);
    systemState.mode[sizeof(systemState.mode) - 1] = '\0';
    
    prefs.end();
    
    Serial.println("[State] State loaded from preferences");
}

/**
 * Reset to defaults
 */
void state_reset_to_defaults(void) {
    systemState.targetTemp = DEFAULT_TARGET_TEMP;
    strncpy(systemState.mode, DEFAULT_MODE, sizeof(systemState.mode) - 1);
    systemState.currentTemp = 0.0;
    systemState.heating = false;
    systemState.power = 0;
    
    // Save defaults
    state_save_to_preferences();
    
    // Reset PID gains
    PIDGains_t defaultGains = {DEFAULT_KP, DEFAULT_KI, DEFAULT_KD};
    state_save_pid_gains(&defaultGains);
    
    Serial.println("[State] Reset to defaults");
}
