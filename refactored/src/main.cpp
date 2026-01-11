/*
 * ESP32 Reptile Thermostat with PID Control
 * Version: 1.5.0 - Modular Refactor Complete
 * Last Updated: January 11, 2026
 *
 * Phase 2: Network stack modularized
 * Phase 3: Control & logic modularized (PID, State, Scheduler)
 * Phase 4: TFT Display modularized
 * Phase 5: Hardware abstraction (temp sensor, dimmer control)
 * Phase 6: Utilities (logger module)
 */

#include <Arduino.h>
#include <SPI.h>

// Include network modules (Phase 2)
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "web_server.h"

// Include control modules (Phase 3)
#include "pid_controller.h"
#include "system_state.h"
#include "scheduler.h"

// Include hardware modules (Phase 4)
#include "tft_display.h"

// Include hardware modules (Phase 5)
#include "temp_sensor.h"
#include "dimmer_control.h"

// Include utilities (Phase 6)
#include "logger.h"
#include "temp_history.h"

// Firmware version
#define FIRMWARE_VERSION "1.6.0"

// Timing
unsigned long lastTempRead = 0;
unsigned long lastPidTime = 0;
unsigned long lastMqttPublish = 0;
unsigned long bootTime = 0;

// Forward declarations
void readTemperature(void);
void onMQTTSetpoint(const char* topic, const char* message);
void onMQTTMode(const char* topic, const char* message);
void onWebControl(float temp, const char* mode);
void onWebRestart(void);
void onTouchButton(int buttonId);
void addLog(const char* message);

void setup() {
    Serial.begin(115200);
    bootTime = millis();

    // Initialize logger and history
    logger_init(bootTime);
    temp_history_init(bootTime);

    Serial.println("=== ESP32 Reptile Thermostat v" FIRMWARE_VERSION " ===");
    logger_add("System boot - v" FIRMWARE_VERSION);
    
    // Initialize TFT display
    tft_init();
    tft_show_boot_message("Initializing...");
    
    // Initialize hardware
    temp_sensor_init();
    dimmer_init();
    
    // Initialize system state (loads preferences)
    state_init();
    logger_add("System state initialized");
    
    // Load PID gains and initialize controller
    PIDGains_t gains;
    state_load_pid_gains(&gains);
    pid_init(gains.Kp, gains.Ki, gains.Kd);
    logger_add("PID controller initialized");
    
    // Initialize scheduler
    scheduler_init();
    logger_add("Scheduler initialized");
    
    // Get device name for network and display
    char deviceName[32];
    state_get_device_name(deviceName, sizeof(deviceName));
    tft_set_device_name(deviceName);
    
    // Register touch button callback
    tft_register_touch_callback(onTouchButton);
    
    // Initialize WiFi
    wifi_init();
    logger_add("WiFi initialized");
    
    // Setup mDNS if connected
    if (!wifi_is_ap_mode()) {
        wifi_setup_mdns(deviceName);
        
        // Setup NTP time
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        logger_add("Time sync started");
        
        // Initialize MQTT
        mqtt_init();
        mqtt_set_setpoint_callback(onMQTTSetpoint);
        mqtt_set_mode_callback(onMQTTMode);
        logger_add("MQTT initialized");
    }
    
    // Initialize web server
    webserver_init();
    webserver_set_device_info(deviceName, FIRMWARE_VERSION);
    webserver_set_control_callback(onWebControl);
    webserver_set_restart_callback(onWebRestart);
    webserver_set_schedule_data(scheduler_is_enabled(), 
                               scheduler_get_slot_count(), 
                               scheduler_get_slots());
    logger_add("Web server started");
    
    // Draw initial screen
    tft_switch_screen(SCREEN_MAIN);
    
    Serial.println("=== Initialization Complete ===");
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
}

void loop() {
    // Get current system state
    SystemState_t* state = state_get();
    
    // Network tasks
    wifi_task();
    
    if (!wifi_is_ap_mode()) {
        mqtt_task();
        scheduler_task();  // Check schedule
    }
    
    webserver_task();
    
    // TFT display task
    tft_task();
    
    // Update TFT display values
    tft_update_main_screen(state->currentTemp, state->targetTemp, 
                          state->heating, state->mode, state->power);
    tft_set_wifi_status(!wifi_is_ap_mode(), wifi_is_ap_mode());
    tft_set_mqtt_status(mqtt_is_connected());
    
    // Read temperature (every 2s)
    if (millis() - lastTempRead >= 2000) {
        readTemperature();
        lastTempRead = millis();
    }
    
    // Update PID control (every 1s)
    if (millis() - lastPidTime >= 1000) {
        // Compute power based on mode
        int power = 0;
        
        if (strcmp(state->mode, MODE_AUTO) == 0) {
            power = pid_compute(state->currentTemp, state->targetTemp);
        } else if (strcmp(state->mode, MODE_ON) == 0) {
            power = 100;
        } else {  // MODE_OFF
            power = 0;
            pid_reset();
        }
        
        // Update state
        state_set_power(power);
        state_set_heating(power > 0);
        
        // Control dimmer
        dimmer_set_power(power);
        
        lastPidTime = millis();
    }
    
    // Update webserver state
    webserver_set_state(state->currentTemp, state->targetTemp, state->heating, 
                       state->mode, state->power);
    webserver_set_network_status(!wifi_is_ap_mode(), wifi_is_ap_mode(), 
                                 wifi_get_ssid(), wifi_get_ip_address());
    
    // Publish MQTT status (every 30s)
    if (!wifi_is_ap_mode() && mqtt_is_connected()) {
        if (millis() - lastMqttPublish >= 30000) {
            mqtt_publish_status(state->currentTemp, state->targetTemp, 
                               state->heating, state->mode, state->power);
            lastMqttPublish = millis();
            
            // Send HA discovery once
            static bool discoveryDone = false;
            if (!discoveryDone) {
                char devName[32];
                state_get_device_name(devName, sizeof(devName));
                mqtt_send_ha_discovery(devName, "reptile_thermostat_01");
                discoveryDone = true;
            }
        }
    }
}

// ===== CALLBACK FUNCTIONS =====

void onMQTTSetpoint(const char* topic, const char* message) {
    float newTarget = atof(message);
    if (newTarget >= 15.0 && newTarget <= 45.0) {
        state_set_target_temp(newTarget, true);
        pid_reset();
        
        char log[64];
        snprintf(log, sizeof(log), "Target: %.1f°C (MQTT)", newTarget);
        logger_add(log);
        
        tft_request_update();
    }
}

void onMQTTMode(const char* topic, const char* message) {
    String newMode = String(message);
    if (newMode == "auto" || newMode == "off" || newMode == "heat") {
        const char* mode = (newMode == "heat") ? MODE_ON : newMode.c_str();
        state_set_mode(mode, true);
        pid_reset();
        
        char log[64];
        snprintf(log, sizeof(log), "Mode: %s (MQTT)", mode);
        logger_add(log);
        
        tft_request_update();
    }
}

void onWebControl(float temp, const char* newMode) {
    state_set_target_temp(temp, true);
    state_set_mode(newMode, true);
    pid_reset();
    
    char log[64];
    snprintf(log, sizeof(log), "Control: %.1f°C, %s (Web)", temp, newMode);
    logger_add(log);
    
    tft_request_update();
    
    // Publish to MQTT
    SystemState_t* state = state_get();
    if (mqtt_is_connected()) {
        mqtt_publish_status(state->currentTemp, state->targetTemp, 
                           state->heating, state->mode, state->power);
    }
}

void onWebRestart(void) {
    logger_add("Restart requested");
    ESP.restart();
}

void onTouchButton(int buttonId) {
    SystemState_t* state = state_get();
    
    switch (buttonId) {
        case BTN_PLUS:
            Serial.println("PLUS button");
            {
                float newTarget = state->targetTemp + 0.5;
                if (newTarget > 45.0) newTarget = 45.0;
                state_set_target_temp(newTarget, true);
                pid_reset();
                tft_request_update();
                
                if (mqtt_is_connected()) {
                    mqtt_publish_status(state->currentTemp, state->targetTemp,
                                       state->heating, state->mode, state->power);
                }
            }
            break;
            
        case BTN_MINUS:
            Serial.println("MINUS button");
            {
                float newTarget = state->targetTemp - 0.5;
                if (newTarget < 15.0) newTarget = 15.0;
                state_set_target_temp(newTarget, true);
                pid_reset();
                tft_request_update();
                
                if (mqtt_is_connected()) {
                    mqtt_publish_status(state->currentTemp, state->targetTemp,
                                       state->heating, state->mode, state->power);
                }
            }
            break;
            
        case BTN_SETTINGS:
            Serial.println("SETTINGS button");
            tft_switch_screen(SCREEN_SETTINGS);
            break;
            
        case BTN_SIMPLE:
            Serial.println("SIMPLE button");
            tft_switch_screen(SCREEN_SIMPLE);
            break;
            
        case BTN_MODE_AUTO:
            Serial.println("AUTO mode");
            state_set_mode(MODE_AUTO, true);
            pid_reset();
            tft_switch_screen(SCREEN_SETTINGS);  // Redraw to show selection
            break;
            
        case BTN_MODE_ON:
            Serial.println("MANUAL ON mode");
            state_set_mode(MODE_ON, true);
            tft_switch_screen(SCREEN_SETTINGS);
            break;
            
        case BTN_MODE_OFF:
            Serial.println("OFF mode");
            state_set_mode(MODE_OFF, true);
            tft_switch_screen(SCREEN_SETTINGS);
            break;
            
        case BTN_BACK:
            Serial.println("BACK button");
            tft_switch_screen(SCREEN_MAIN);
            break;
    }
}

// ===== HARDWARE FUNCTIONS =====

void readTemperature(void) {
    float temp = temp_sensor_read();

    if (temp_sensor_is_valid(temp)) {
        SystemState_t* state = state_get();
        if (abs(temp - state->currentTemp) > 0.1) {
            state_set_current_temp(temp);
            tft_request_update();
        }

        // Record to history (module handles sampling interval)
        temp_history_record(temp);
    } else {
        static unsigned long lastError = 0;
        if (millis() - lastError > 60000) {
            logger_add("Temp sensor error!");
            lastError = millis();
        }
    }
}

// ===== LOGGING =====

void addLog(const char* message) {
    // Wrapper function for compatibility
    logger_add(message);
}
