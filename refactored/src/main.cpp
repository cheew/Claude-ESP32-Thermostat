/*
 * ESP32 Reptile Thermostat with PID Control
 * Version: 2.2.0 - Safety Features & Project Cleanup
 * Last Updated: January 16, 2026
 *
 * v2.2.0 Changes:
 * - Sensor fault detection (stale/error states per output)
 * - Hard temperature cutoffs (max/min limits override all modes)
 * - Health API endpoint for monitoring
 * - Project cleanup and documentation consolidation
 *
 * v2.1.0 Changes:
 * - MQTT multi-output support (3 Home Assistant climate entities)
 * - Schedule page with per-output scheduling
 * - Mobile responsive design (phones and tablets)
 */

#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>

// Include network modules (Phase 2)
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "web_server.h"

// Include control modules (Phase 3)
#include "system_state.h"

// Include hardware modules (Phase 4)
#include "display_manager.h"

// Include hardware modules (Phase 5 - Multi-output)
#include "sensor_manager.h"
#include "output_manager.h"

// Include utilities (Phase 6)
#include "logger.h"
#include "temp_history.h"
#include "console.h"

// Firmware version
#define FIRMWARE_VERSION "2.2.0"

// Hardware configuration
#define ONE_WIRE_BUS 4  // DS18B20 OneWire bus pin

// Timing
unsigned long lastSensorRead = 0;
unsigned long lastOutputUpdate = 0;
unsigned long lastMqttPublish = 0;
unsigned long bootTime = 0;

// Device name (global for use in loop)
char deviceName[32] = "ESP32-Thermostat";

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
// void onTouchButton(int buttonId);  // OLD TFT - no longer used

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

    // Initialize TFT display (3-output support)
    display_init();

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

    // Load device name from preferences
    // (deviceName is global variable)
    Preferences prefs;
    prefs.begin("thermostat", true);
    String savedName = prefs.getString("device_name", "");
    if (savedName.length() > 0) {
        strncpy(deviceName, savedName.c_str(), sizeof(deviceName) - 1);
    }
    prefs.end();

    // Set up display callbacks
    display_set_control_callback([](int outputId, float newTarget) {
        output_manager_set_target(outputId, newTarget);
        console_add_event_f(CONSOLE_EVENT_SYSTEM, "Display: Output %d target set to %.1f°C", outputId + 1, newTarget);
    });

    display_set_mode_callback([](int outputId, const char* mode) {
        // Convert mode string to enum
        ControlMode_t modeEnum = CONTROL_MODE_OFF;
        if (strcmp(mode, "off") == 0) modeEnum = CONTROL_MODE_OFF;
        else if (strcmp(mode, "manual") == 0) modeEnum = CONTROL_MODE_MANUAL;
        else if (strcmp(mode, "pid") == 0) modeEnum = CONTROL_MODE_PID;
        else if (strcmp(mode, "onoff") == 0) modeEnum = CONTROL_MODE_ONOFF;
        else if (strcmp(mode, "schedule") == 0) modeEnum = CONTROL_MODE_SCHEDULE;

        output_manager_set_mode(outputId, modeEnum);
        console_add_event_f(CONSOLE_EVENT_SYSTEM, "Display: Output %d mode set to %s", outputId + 1, mode);
    });
    
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
    // Note: Schedule data now managed per-output via output_manager
    logger_add("Web server started");
    
    // Display is initialized and showing main screen

    // Do immediate first display update so temps show right away
    for (int i = 0; i < 3; i++) {
        OutputConfig_t* output = output_manager_get_output(i);
        if (output && output->enabled) {
            const char* modeStr = output_manager_get_mode_name(output->controlMode);
            display_update_output(
                i,
                output->currentTemp,
                output->targetTemp,
                modeStr ? modeStr : "off",
                output->currentPower,
                output->heating
            );
            if (output->name[0] != '\0') {
                display_set_output_name(i, output->name);
            }
        }
    }

    // Update system status
    DisplaySystemData sysData = {0};
    strncpy(sysData.deviceName, deviceName, sizeof(sysData.deviceName) - 1);
    strncpy(sysData.firmwareVersion, FIRMWARE_VERSION, sizeof(sysData.firmwareVersion) - 1);
    sysData.wifiConnected = !wifi_is_ap_mode();
    sysData.mqttConnected = mqtt_is_connected();
    sysData.uptime = (millis() - bootTime) / 1000;
    sysData.freeMemory = (ESP.getFreeHeap() * 100) / ESP.getHeapSize();
    display_update_system(&sysData);

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

    // Display task (handles screen updates and touch input)
    display_task();

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

    // Update display with all 3 outputs (every 2 seconds for live temp updates)
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate >= 2000) {
        for (int i = 0; i < 3; i++) {
            OutputConfig_t* output = output_manager_get_output(i);
            if (output && output->enabled) {
                // Get mode name as string
                const char* modeStr = output_manager_get_mode_name(output->controlMode);

                display_update_output(
                    i,
                    output->currentTemp,
                    output->targetTemp,
                    modeStr ? modeStr : "off",
                    output->currentPower,
                    output->heating
                );

                // Update output name on display
                if (output->name[0] != '\0') {
                    display_set_output_name(i, output->name);
                }
            }
        }

        // Update system status for display
        DisplaySystemData sysData = {0};
        strncpy(sysData.deviceName, deviceName, sizeof(sysData.deviceName) - 1);
        strncpy(sysData.firmwareVersion, FIRMWARE_VERSION, sizeof(sysData.firmwareVersion) - 1);
        sysData.wifiConnected = !wifi_is_ap_mode();
        sysData.mqttConnected = mqtt_is_connected();
        sysData.uptime = (millis() - bootTime) / 1000;
        sysData.freeMemory = (ESP.getFreeHeap() * 100) / ESP.getHeapSize();
        display_update_system(&sysData);

        lastDisplayUpdate = millis();
    }

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

        // tft_request_update();  // OLD TFT - display updates automatically
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

    // tft_request_update();  // OLD TFT - display updates automatically
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

    // tft_request_update();  // OLD TFT - display updates automatically

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

/*  OLD TFT BUTTON HANDLER - NO LONGER USED
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
*/  // END OLD TFT BUTTON HANDLER

// ===== END OF MAIN.CPP =====
// Multi-output system v2.0.0
// - 3 independent outputs (lights on dimmer, heat devices on SSR)
// - Up to 6 DS18B20 sensors
// - Per-output PID control and scheduling
// - Legacy TFT and web interface compatibility (uses Output 1)
