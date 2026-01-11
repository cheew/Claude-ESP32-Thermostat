/*
 * ESP32 Reptile Thermostat with PID Control
 * Version: 2.1.0 - Multi-Output with MQTT, Schedule & Mobile
 * Last Updated: January 11, 2026
 *
 * v2.1.0 Changes:
 * - MQTT multi-output support (3 Home Assistant climate entities)
 * - Schedule page with per-output scheduling
 * - Mobile responsive design (phones and tablets)
 *
 * v2.0.0 Changes:
 * - Multi-output control (3 independent outputs)
 * - Multi-sensor support (up to 6 DS18B20 sensors)
 * - Output 1: Lights (AC dimmer only)
 * - Output 2 & 3: Heat devices (SSR only)
 * - Sensor manager with auto-discovery
 * - Per-output scheduling and PID control
 */

#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>

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

// Include hardware modules (Phase 5 - Multi-output)
#include "sensor_manager.h"
#include "output_manager.h"

// Include utilities (Phase 6)
#include "logger.h"
#include "temp_history.h"
#include "console.h"

// Firmware version
#define FIRMWARE_VERSION "2.1.0"

// Hardware configuration
#define ONE_WIRE_BUS 4  // DS18B20 OneWire bus pin

// Timing
unsigned long lastSensorRead = 0;
unsigned long lastOutputUpdate = 0;
unsigned long lastMqttPublish = 0;
unsigned long bootTime = 0;

// Legacy compatibility (for TFT display - uses Output 1 by default)
static SystemState_t legacyState = {
    0.0f,      // currentTemp
    25.0f,     // targetTemp
    false,     // heating
    "off",     // mode
    0          // power
};

// Forward declarations
void readSensors(void);
void updateOutputs(void);
void updateLegacyState(void);
void onMQTTSetpoint(const char* topic, const char* message);
void onMQTTMode(const char* topic, const char* message);
void onWebControl(float temp, const char* mode);
void onWebRestart(void);
void onTouchButton(int buttonId);

void setup() {
    Serial.begin(115200);
    bootTime = millis();

    // Initialize logger, history, and console
    logger_init(bootTime);
    temp_history_init(bootTime);
    console_init();

    Serial.println("=== ESP32 Reptile Thermostat v" FIRMWARE_VERSION " ===");
    Serial.println("=== Multi-Output Environmental Control ===");
    logger_add("System boot - v" FIRMWARE_VERSION);
    console_add_event(CONSOLE_EVENT_SYSTEM, "System boot - v" FIRMWARE_VERSION);

    // Initialize TFT display
    tft_init();
    tft_show_boot_message("Initializing...");

    // Initialize sensor manager
    sensor_manager_init(ONE_WIRE_BUS);
    int sensorCount = sensor_manager_get_count();
    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Found %d temperature sensors", sensorCount);
    logger_add("Sensor manager initialized");

    // Initialize output manager
    output_manager_init();
    console_add_event(CONSOLE_EVENT_SYSTEM, "Output manager initialized (3 outputs)");
    logger_add("Output manager initialized");

    // Auto-assign sensors to outputs (if available)
    if (sensorCount > 0) {
        for (int i = 0; i < 3 && i < sensorCount; i++) {
            const SensorInfo_t* sensor = sensor_manager_get_sensor(i);
            if (sensor && sensor->discovered) {
                output_manager_set_sensor(i, sensor->addressString);
                console_add_event_f(CONSOLE_EVENT_SYSTEM,
                                   "Output %d auto-assigned to sensor: %s",
                                   i + 1, sensor->name);
            }
        }
    }

    // Get device name for network and display
    char deviceName[32] = "ESP32-Thermostat";
    // Try to load from preferences
    Preferences prefs;
    prefs.begin("thermostat", true);
    String savedName = prefs.getString("device_name", "");
    if (savedName.length() > 0) {
        strncpy(deviceName, savedName.c_str(), sizeof(deviceName) - 1);
    }
    prefs.end();
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
    // Network tasks
    wifi_task();

    if (!wifi_is_ap_mode()) {
        mqtt_task();
    }

    webserver_task();

    // TFT display task
    tft_task();

    // Read all sensors (every 2s)
    if (millis() - lastSensorRead >= 2000) {
        readSensors();
        lastSensorRead = millis();
    }

    // Update all outputs (every 100ms for responsive control)
    if (millis() - lastOutputUpdate >= 100) {
        updateOutputs();
        lastOutputUpdate = millis();
    }

    // Update legacy state for TFT display (uses Output 1)
    updateLegacyState();

    // Update TFT display values (Output 1 only for now)
    tft_update_main_screen(legacyState.currentTemp, legacyState.targetTemp,
                          legacyState.heating, legacyState.mode, legacyState.power);
    tft_set_wifi_status(!wifi_is_ap_mode(), wifi_is_ap_mode());
    tft_set_mqtt_status(mqtt_is_connected());

    // Update webserver state (legacy - uses Output 1)
    webserver_set_state(legacyState.currentTemp, legacyState.targetTemp, legacyState.heating,
                       legacyState.mode, legacyState.power);
    webserver_set_network_status(!wifi_is_ap_mode(), wifi_is_ap_mode(),
                                 wifi_get_ssid(), wifi_get_ip_address());
    
    // Publish MQTT status (every 30s) - Multi-output
    if (!wifi_is_ap_mode() && mqtt_is_connected()) {
        if (millis() - lastMqttPublish >= 30000) {
            // Publish all 3 outputs status
            mqtt_publish_all_outputs(WiFi.RSSI(), ESP.getFreeHeap(), millis() / 1000);
            lastMqttPublish = millis();

            // Send HA discovery once
            static bool discoveryDone = false;
            if (!discoveryDone) {
                char devName[32] = "ESP32-Thermostat";
                Preferences prefs;
                prefs.begin("thermostat", true);
                String savedName = prefs.getString("device_name", "");
                if (savedName.length() > 0) {
                    strncpy(devName, savedName.c_str(), sizeof(devName) - 1);
                }
                prefs.end();
                mqtt_send_ha_discovery(devName, "reptile_thermostat_01");
                discoveryDone = true;
            }
        }
    }
}

// ===== HELPER FUNCTIONS =====

/**
 * Read all sensors
 */
void readSensors(void) {
    sensor_manager_read_all();

    // Update temperature history (use Output 1's sensor for now)
    OutputConfig_t* output1 = output_manager_get_output(0);
    if (output1 && sensor_manager_is_valid_temp(output1->currentTemp)) {
        temp_history_record(output1->currentTemp);
        console_add_event_f(CONSOLE_EVENT_TEMP, "Temp: %.1f°C", output1->currentTemp);
    }
}

/**
 * Update all outputs
 */
void updateOutputs(void) {
    output_manager_update();
}

/**
 * Update legacy state from Output 1 (for TFT display and web compatibility)
 */
void updateLegacyState(void) {
    OutputConfig_t* output1 = output_manager_get_output(0);
    if (!output1) return;

    legacyState.currentTemp = output1->currentTemp;
    legacyState.targetTemp = output1->targetTemp;
    legacyState.heating = output1->heating;
    legacyState.power = output1->currentPower;

    // Map control mode to legacy mode string
    switch (output1->controlMode) {
        case CONTROL_MODE_OFF:
            strcpy(legacyState.mode, "off");
            break;
        case CONTROL_MODE_MANUAL:
            strcpy(legacyState.mode, "manual");
            break;
        case CONTROL_MODE_PID:
            strcpy(legacyState.mode, "auto");
            break;
        case CONTROL_MODE_ONOFF:
            strcpy(legacyState.mode, "onoff");
            break;
        case CONTROL_MODE_SCHEDULE:
            strcpy(legacyState.mode, "schedule");
            break;
        default:
            strcpy(legacyState.mode, "off");
            break;
    }
}

// ===== CALLBACK FUNCTIONS =====

void onMQTTSetpoint(const char* topic, const char* message) {
    // Apply to Output 1 (legacy compatibility)
    float newTarget = atof(message);
    if (newTarget >= 15.0 && newTarget <= 45.0) {
        output_manager_set_target(0, newTarget);

        char log[64];
        snprintf(log, sizeof(log), "Output 1 target: %.1f°C (MQTT)", newTarget);
        logger_add(log);
        console_add_event_f(CONSOLE_EVENT_MQTT, "MQTT: Target %.1f°C", newTarget);

        tft_request_update();
    }
}

void onMQTTMode(const char* topic, const char* message) {
    // Apply to Output 1 (legacy compatibility)
    String newMode = String(message);
    ControlMode_t mode = CONTROL_MODE_OFF;

    if (newMode == "auto") {
        mode = CONTROL_MODE_PID;
    } else if (newMode == "heat") {
        mode = CONTROL_MODE_MANUAL;
        output_manager_set_manual_power(0, 100);
    } else if (newMode == "off") {
        mode = CONTROL_MODE_OFF;
    }

    output_manager_set_mode(0, mode);

    char log[64];
    snprintf(log, sizeof(log), "Output 1 mode: %s (MQTT)", newMode.c_str());
    logger_add(log);
    console_add_event_f(CONSOLE_EVENT_MQTT, "MQTT: Mode %s", newMode.c_str());

    tft_request_update();
}

void onWebControl(float temp, const char* newMode) {
    // Apply to Output 1 (legacy compatibility)
    output_manager_set_target(0, temp);

    ControlMode_t mode = CONTROL_MODE_OFF;
    if (strcmp(newMode, "auto") == 0) {
        mode = CONTROL_MODE_PID;
    } else if (strcmp(newMode, "on") == 0) {
        mode = CONTROL_MODE_MANUAL;
        output_manager_set_manual_power(0, 100);
    } else if (strcmp(newMode, "off") == 0) {
        mode = CONTROL_MODE_OFF;
    }

    output_manager_set_mode(0, mode);

    char log[64];
    snprintf(log, sizeof(log), "Output 1: %.1f°C, %s (Web)", temp, newMode);
    logger_add(log);
    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Web control: %.1f°C %s", temp, newMode);

    tft_request_update();

    // Publish to MQTT
    if (mqtt_is_connected()) {
        mqtt_publish_status(legacyState.currentTemp, legacyState.targetTemp,
                           legacyState.heating, legacyState.mode, legacyState.power);
    }
}

void onWebRestart(void) {
    logger_add("Restart requested");
    ESP.restart();
}

void onTouchButton(int buttonId) {
    // TFT controls Output 1 only (legacy compatibility)
    OutputConfig_t* output1 = output_manager_get_output(0);
    if (!output1) return;

    switch (buttonId) {
        case BTN_PLUS:
            Serial.println("PLUS button");
            {
                float newTarget = output1->targetTemp + 0.5;
                if (newTarget > 45.0) newTarget = 45.0;
                output_manager_set_target(0, newTarget);
                tft_request_update();

                if (mqtt_is_connected()) {
                    mqtt_publish_status(legacyState.currentTemp, legacyState.targetTemp,
                                       legacyState.heating, legacyState.mode, legacyState.power);
                }
            }
            break;

        case BTN_MINUS:
            Serial.println("MINUS button");
            {
                float newTarget = output1->targetTemp - 0.5;
                if (newTarget < 15.0) newTarget = 15.0;
                output_manager_set_target(0, newTarget);
                tft_request_update();

                if (mqtt_is_connected()) {
                    mqtt_publish_status(legacyState.currentTemp, legacyState.targetTemp,
                                       legacyState.heating, legacyState.mode, legacyState.power);
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
            output_manager_set_mode(0, CONTROL_MODE_PID);
            tft_switch_screen(SCREEN_SETTINGS);  // Redraw to show selection
            break;

        case BTN_MODE_ON:
            Serial.println("MANUAL ON mode");
            output_manager_set_mode(0, CONTROL_MODE_MANUAL);
            output_manager_set_manual_power(0, 100);
            tft_switch_screen(SCREEN_SETTINGS);
            break;

        case BTN_MODE_OFF:
            Serial.println("OFF mode");
            output_manager_set_mode(0, CONTROL_MODE_OFF);
            tft_switch_screen(SCREEN_SETTINGS);
            break;

        case BTN_BACK:
            Serial.println("BACK button");
            tft_switch_screen(SCREEN_MAIN);
            break;
    }
}

// ===== END OF MAIN.CPP =====
// Multi-output system v2.0.0
// - 3 independent outputs (lights on dimmer, heat devices on SSR)
// - Up to 6 DS18B20 sensors
// - Per-output PID control and scheduling
// - Legacy TFT and web interface compatibility (uses Output 1)
