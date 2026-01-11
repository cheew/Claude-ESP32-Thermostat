/**
 * output_manager.cpp
 * Multi-Output Control System Implementation
 */

#include "output_manager.h"
#include "sensor_manager.h"
#include "console.h"
#include <RBDdimmer.h>
#include <Preferences.h>

// Hardware pin assignments
#define OUTPUT1_PIN 5      // AC Dimmer PWM
#define OUTPUT2_PIN 14     // SSR control
#define OUTPUT3_PIN 32     // SSR control
#define ZEROCROSS_PIN 27   // Shared zero-cross for dimmer

// PID limits
#define PID_OUTPUT_MIN 0
#define PID_OUTPUT_MAX 100
#define PID_INTEGRAL_MAX 100.0f

// Output array
static OutputConfig_t outputs[MAX_OUTPUTS];

// Hardware objects
static dimmerLamp* dimmer1 = nullptr;  // Output 1 (AC dimmer)

// Forward declarations
static void updateOutput(int index);
static void updatePID(int index);
static void updateSchedule(int index);
static void setOutputPower(int index, int power);

/**
 * Initialize output manager
 */
void output_manager_init(void) {
    Serial.println("[OutputMgr] Initializing...");

    // Clear output array
    memset(outputs, 0, sizeof(outputs));

    // Initialize Output 1 (AC Dimmer for lights)
    outputs[0].enabled = true;
    strncpy(outputs[0].name, "Lights", sizeof(outputs[0].name));
    outputs[0].hardwareType = HARDWARE_DIMMER_AC;
    outputs[0].deviceType = DEVICE_LIGHT;
    outputs[0].controlPin = OUTPUT1_PIN;
    outputs[0].controlMode = CONTROL_MODE_MANUAL;
    outputs[0].targetTemp = 25.0f;
    outputs[0].manualPower = 0;
    outputs[0].pidKp = 10.0f;
    outputs[0].pidKi = 0.5f;
    outputs[0].pidKd = 2.0f;

    // Initialize Output 2 (SSR for heat mat)
    outputs[1].enabled = true;
    strncpy(outputs[1].name, "Heat Mat", sizeof(outputs[1].name));
    outputs[1].hardwareType = HARDWARE_SSR;
    outputs[1].deviceType = DEVICE_HEAT_MAT;
    outputs[1].controlPin = OUTPUT2_PIN;
    outputs[1].controlMode = CONTROL_MODE_OFF;
    outputs[1].targetTemp = 28.0f;
    outputs[1].manualPower = 0;
    outputs[1].pidKp = 10.0f;
    outputs[1].pidKi = 0.5f;
    outputs[1].pidKd = 2.0f;

    // Initialize Output 3 (SSR for ceramic heater)
    outputs[2].enabled = true;
    strncpy(outputs[2].name, "Ceramic Heater", sizeof(outputs[2].name));
    outputs[2].hardwareType = HARDWARE_SSR;
    outputs[2].deviceType = DEVICE_CERAMIC_HEATER;
    outputs[2].controlPin = OUTPUT3_PIN;
    outputs[2].controlMode = CONTROL_MODE_OFF;
    outputs[2].targetTemp = 30.0f;
    outputs[2].manualPower = 0;
    outputs[2].pidKp = 10.0f;
    outputs[2].pidKi = 0.5f;
    outputs[2].pidKd = 2.0f;

    // Setup hardware
    // Output 1: AC Dimmer
    dimmer1 = new dimmerLamp(OUTPUT1_PIN, ZEROCROSS_PIN);
    dimmer1->begin(NORMAL_MODE, ON);
    dimmer1->setPower(0);

    // Output 2 & 3: SSR (digital outputs)
    pinMode(OUTPUT2_PIN, OUTPUT);
    pinMode(OUTPUT3_PIN, OUTPUT);
    digitalWrite(OUTPUT2_PIN, LOW);
    digitalWrite(OUTPUT3_PIN, LOW);

    // Load saved configuration
    output_manager_load_config();

    Serial.println("[OutputMgr] Initialized 3 outputs");
    console_add_event(CONSOLE_EVENT_SYSTEM, "Output manager initialized (3 outputs)");
}

/**
 * Get output configuration
 */
OutputConfig_t* output_manager_get_output(int outputIndex) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return nullptr;
    }
    return &outputs[outputIndex];
}

/**
 * Update output control loop
 */
void output_manager_update(void) {
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        if (outputs[i].enabled) {
            updateOutput(i);
        } else {
            // Output disabled, turn off
            setOutputPower(i, 0);
            outputs[i].currentPower = 0;
            outputs[i].heating = false;
        }
    }
}

/**
 * Update single output
 */
static void updateOutput(int index) {
    OutputConfig_t* output = &outputs[index];

    // Get current temperature from assigned sensor
    const SensorInfo_t* sensor = sensor_manager_get_sensor_by_address(output->sensorAddress);
    if (sensor && sensor->discovered) {
        output->currentTemp = sensor->lastReading;
    } else {
        // No sensor assigned or sensor not found
        output->currentTemp = -127.0f;
    }

    // Update based on control mode
    switch (output->controlMode) {
        case CONTROL_MODE_OFF:
            setOutputPower(index, 0);
            output->currentPower = 0;
            output->heating = false;
            break;

        case CONTROL_MODE_MANUAL:
            setOutputPower(index, output->manualPower);
            output->currentPower = output->manualPower;
            output->heating = (output->manualPower > 0);
            break;

        case CONTROL_MODE_PID:
            if (sensor_manager_is_valid_temp(output->currentTemp)) {
                updatePID(index);
            } else {
                setOutputPower(index, 0);
                output->currentPower = 0;
                output->heating = false;
            }
            break;

        case CONTROL_MODE_ONOFF:
            if (sensor_manager_is_valid_temp(output->currentTemp)) {
                if (output->currentTemp < output->targetTemp - 0.5f) {
                    // Heat on (full power)
                    setOutputPower(index, 100);
                    output->currentPower = 100;
                    output->heating = true;
                } else if (output->currentTemp > output->targetTemp + 0.5f) {
                    // Heat off
                    setOutputPower(index, 0);
                    output->currentPower = 0;
                    output->heating = false;
                }
                // Else: maintain current state (hysteresis)
            } else {
                setOutputPower(index, 0);
                output->currentPower = 0;
                output->heating = false;
            }
            break;

        case CONTROL_MODE_SCHEDULE:
            updateSchedule(index);
            break;
    }
}

/**
 * Update PID control
 */
static void updatePID(int index) {
    OutputConfig_t* output = &outputs[index];

    unsigned long now = millis();
    float dt = (now - output->pidLastTime) / 1000.0f;  // Convert to seconds

    if (dt < 0.1f) {
        return;  // Too soon, wait for more time
    }

    // Calculate error
    float error = output->targetTemp - output->currentTemp;

    // Proportional term
    float P = output->pidKp * error;

    // Integral term
    output->pidIntegral += error * dt;
    // Anti-windup
    if (output->pidIntegral > PID_INTEGRAL_MAX) {
        output->pidIntegral = PID_INTEGRAL_MAX;
    } else if (output->pidIntegral < -PID_INTEGRAL_MAX) {
        output->pidIntegral = -PID_INTEGRAL_MAX;
    }
    float I = output->pidKi * output->pidIntegral;

    // Derivative term
    float D = 0.0f;
    if (dt > 0.0f) {
        D = output->pidKd * (error - output->pidLastError) / dt;
    }

    // Calculate output
    float pidOutput = P + I + D;

    // Clamp output
    if (pidOutput < PID_OUTPUT_MIN) {
        pidOutput = PID_OUTPUT_MIN;
    } else if (pidOutput > PID_OUTPUT_MAX) {
        pidOutput = PID_OUTPUT_MAX;
    }

    // Set power
    int power = (int)pidOutput;
    setOutputPower(index, power);
    output->currentPower = power;
    output->heating = (power > 5);  // Consider heating if power > 5%

    // Update state
    output->pidLastError = error;
    output->pidLastTime = now;
}

/**
 * Update schedule control
 */
static void updateSchedule(int index) {
    OutputConfig_t* output = &outputs[index];

    // Get current time
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        // Time not set, default to manual mode behavior
        setOutputPower(index, output->manualPower);
        output->currentPower = output->manualPower;
        return;
    }

    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentTotalMinutes = currentHour * 60 + currentMinute;

    // Find active schedule slot
    int activeSlot = -1;
    int minDiff = 24 * 60;  // Max difference in minutes

    for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
        if (!output->schedule[i].enabled) {
            continue;
        }

        int slotTotalMinutes = output->schedule[i].hour * 60 + output->schedule[i].minute;
        int diff = currentTotalMinutes - slotTotalMinutes;

        if (diff >= 0 && diff < minDiff) {
            minDiff = diff;
            activeSlot = i;
        }
    }

    if (activeSlot >= 0) {
        // Apply schedule target temperature
        output->targetTemp = output->schedule[activeSlot].targetTemp;
        // Use PID to reach target
        if (sensor_manager_is_valid_temp(output->currentTemp)) {
            updatePID(index);
        } else {
            setOutputPower(index, 0);
            output->currentPower = 0;
            output->heating = false;
        }
    } else {
        // No active schedule, turn off
        setOutputPower(index, 0);
        output->currentPower = 0;
        output->heating = false;
    }
}

/**
 * Set output power
 */
static void setOutputPower(int index, int power) {
    if (power < 0) power = 0;
    if (power > 100) power = 100;

    switch (index) {
        case 0:
            // Output 1: AC Dimmer
            if (dimmer1) {
                dimmer1->setPower(power);
            }
            break;

        case 1:
            // Output 2: SSR (simple on/off for now)
            digitalWrite(OUTPUT2_PIN, power > 50 ? HIGH : LOW);
            break;

        case 2:
            // Output 3: SSR (simple on/off for now)
            digitalWrite(OUTPUT3_PIN, power > 50 ? HIGH : LOW);
            break;
    }
}

/**
 * Set output enabled
 */
void output_manager_set_enabled(int outputIndex, bool enabled) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return;
    }
    outputs[outputIndex].enabled = enabled;
    if (!enabled) {
        setOutputPower(outputIndex, 0);
    }
    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Output %d %s", outputIndex + 1, enabled ? "enabled" : "disabled");
}

/**
 * Set output name
 */
void output_manager_set_name(int outputIndex, const char* name) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS || !name) {
        return;
    }
    strncpy(outputs[outputIndex].name, name, sizeof(outputs[outputIndex].name) - 1);
    outputs[outputIndex].name[sizeof(outputs[outputIndex].name) - 1] = '\0';
}

/**
 * Set hardware type (with restrictions)
 */
bool output_manager_set_hardware_type(int outputIndex, HardwareType_t hardwareType) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return false;
    }

    // Enforce hardware restrictions
    if (outputIndex == 0) {
        // Output 1: Only dimmer allowed
        if (hardwareType != HARDWARE_DIMMER_AC) {
            return false;
        }
    } else {
        // Outputs 2 & 3: Only SSR allowed
        if (hardwareType != HARDWARE_SSR) {
            return false;
        }
    }

    outputs[outputIndex].hardwareType = hardwareType;
    return true;
}

/**
 * Set device type (with compatibility check)
 */
bool output_manager_set_device_type(int outputIndex, DeviceType_t deviceType) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return false;
    }

    // Check compatibility
    if (!output_manager_is_compatible(deviceType, outputs[outputIndex].hardwareType)) {
        return false;
    }

    outputs[outputIndex].deviceType = deviceType;
    return true;
}

/**
 * Set control mode
 */
void output_manager_set_mode(int outputIndex, ControlMode_t mode) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return;
    }
    outputs[outputIndex].controlMode = mode;

    // Reset PID state when changing modes
    outputs[outputIndex].pidIntegral = 0.0f;
    outputs[outputIndex].pidLastError = 0.0f;
    outputs[outputIndex].pidLastTime = millis();

    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Output %d mode: %s",
                       outputIndex + 1, output_manager_get_mode_name(mode));
}

/**
 * Set target temperature
 */
void output_manager_set_target(int outputIndex, float targetTemp) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return;
    }
    outputs[outputIndex].targetTemp = targetTemp;
}

/**
 * Set manual power
 */
void output_manager_set_manual_power(int outputIndex, int power) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return;
    }
    if (power < 0) power = 0;
    if (power > 100) power = 100;
    outputs[outputIndex].manualPower = power;
}

/**
 * Assign sensor to output
 */
void output_manager_set_sensor(int outputIndex, const char* sensorAddress) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS || !sensorAddress) {
        return;
    }
    strncpy(outputs[outputIndex].sensorAddress, sensorAddress, sizeof(outputs[outputIndex].sensorAddress) - 1);
    outputs[outputIndex].sensorAddress[sizeof(outputs[outputIndex].sensorAddress) - 1] = '\0';

    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Output %d sensor assigned", outputIndex + 1);
}

/**
 * Set PID parameters
 */
void output_manager_set_pid_params(int outputIndex, float kp, float ki, float kd) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return;
    }
    outputs[outputIndex].pidKp = kp;
    outputs[outputIndex].pidKi = ki;
    outputs[outputIndex].pidKd = kd;

    // Reset integral
    outputs[outputIndex].pidIntegral = 0.0f;
}

/**
 * Set schedule slot
 */
bool output_manager_set_schedule_slot(int outputIndex, int slotIndex,
                                      bool enabled, uint8_t hour, uint8_t minute, float targetTemp) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return false;
    }
    if (slotIndex < 0 || slotIndex >= MAX_SCHEDULE_SLOTS) {
        return false;
    }
    if (hour > 23 || minute > 59) {
        return false;
    }

    outputs[outputIndex].schedule[slotIndex].enabled = enabled;
    outputs[outputIndex].schedule[slotIndex].hour = hour;
    outputs[outputIndex].schedule[slotIndex].minute = minute;
    outputs[outputIndex].schedule[slotIndex].targetTemp = targetTemp;

    return true;
}

/**
 * Load configuration from preferences
 */
void output_manager_load_config(void) {
    Preferences prefs;

    for (int i = 0; i < MAX_OUTPUTS; i++) {
        char namespace_name[16];
        snprintf(namespace_name, sizeof(namespace_name), "output%d", i + 1);

        prefs.begin(namespace_name, true);  // Read-only

        // Load basic config
        outputs[i].enabled = prefs.getBool("enabled", outputs[i].enabled);
        String name = prefs.getString("name", "");
        if (name.length() > 0) {
            strncpy(outputs[i].name, name.c_str(), sizeof(outputs[i].name) - 1);
        }

        outputs[i].deviceType = (DeviceType_t)prefs.getUChar("deviceType", outputs[i].deviceType);
        outputs[i].controlMode = (ControlMode_t)prefs.getUChar("mode", outputs[i].controlMode);
        outputs[i].targetTemp = prefs.getFloat("target", outputs[i].targetTemp);
        outputs[i].manualPower = prefs.getInt("manualPower", outputs[i].manualPower);

        String sensor = prefs.getString("sensor", "");
        if (sensor.length() > 0) {
            strncpy(outputs[i].sensorAddress, sensor.c_str(), sizeof(outputs[i].sensorAddress) - 1);
        }

        // Load PID params
        outputs[i].pidKp = prefs.getFloat("pidKp", outputs[i].pidKp);
        outputs[i].pidKi = prefs.getFloat("pidKi", outputs[i].pidKi);
        outputs[i].pidKd = prefs.getFloat("pidKd", outputs[i].pidKd);

        // Load schedule
        for (int j = 0; j < MAX_SCHEDULE_SLOTS; j++) {
            char key[16];
            snprintf(key, sizeof(key), "sch%d_en", j);
            outputs[i].schedule[j].enabled = prefs.getBool(key, false);

            snprintf(key, sizeof(key), "sch%d_hr", j);
            outputs[i].schedule[j].hour = prefs.getUChar(key, 0);

            snprintf(key, sizeof(key), "sch%d_min", j);
            outputs[i].schedule[j].minute = prefs.getUChar(key, 0);

            snprintf(key, sizeof(key), "sch%d_temp", j);
            outputs[i].schedule[j].targetTemp = prefs.getFloat(key, 25.0f);
        }

        prefs.end();
    }

    Serial.println("[OutputMgr] Configuration loaded");
}

/**
 * Save configuration to preferences
 */
void output_manager_save_config(void) {
    Preferences prefs;

    for (int i = 0; i < MAX_OUTPUTS; i++) {
        char namespace_name[16];
        snprintf(namespace_name, sizeof(namespace_name), "output%d", i + 1);

        prefs.begin(namespace_name, false);  // Read-write

        // Save basic config
        prefs.putBool("enabled", outputs[i].enabled);
        prefs.putString("name", outputs[i].name);
        prefs.putUChar("deviceType", outputs[i].deviceType);
        prefs.putUChar("mode", outputs[i].controlMode);
        prefs.putFloat("target", outputs[i].targetTemp);
        prefs.putInt("manualPower", outputs[i].manualPower);
        prefs.putString("sensor", outputs[i].sensorAddress);

        // Save PID params
        prefs.putFloat("pidKp", outputs[i].pidKp);
        prefs.putFloat("pidKi", outputs[i].pidKi);
        prefs.putFloat("pidKd", outputs[i].pidKd);

        // Save schedule
        for (int j = 0; j < MAX_SCHEDULE_SLOTS; j++) {
            char key[16];
            snprintf(key, sizeof(key), "sch%d_en", j);
            prefs.putBool(key, outputs[i].schedule[j].enabled);

            snprintf(key, sizeof(key), "sch%d_hr", j);
            prefs.putUChar(key, outputs[i].schedule[j].hour);

            snprintf(key, sizeof(key), "sch%d_min", j);
            prefs.putUChar(key, outputs[i].schedule[j].minute);

            snprintf(key, sizeof(key), "sch%d_temp", j);
            prefs.putFloat(key, outputs[i].schedule[j].targetTemp);
        }

        prefs.end();
    }

    Serial.println("[OutputMgr] Configuration saved");
    console_add_event(CONSOLE_EVENT_SYSTEM, "Output configuration saved");
}

/**
 * Get device type name
 */
const char* output_manager_get_device_type_name(DeviceType_t deviceType) {
    switch (deviceType) {
        case DEVICE_LIGHT: return "Light";
        case DEVICE_HEAT_MAT: return "Heat Mat";
        case DEVICE_CERAMIC_HEATER: return "Ceramic Heater";
        case DEVICE_HEAT_CABLE: return "Heat Cable";
        case DEVICE_FOGGER: return "Fogger";
        case DEVICE_MISTER: return "Mister";
        default: return "Unknown";
    }
}

/**
 * Get hardware type name
 */
const char* output_manager_get_hardware_type_name(HardwareType_t hardwareType) {
    switch (hardwareType) {
        case HARDWARE_DIMMER_AC: return "AC Dimmer";
        case HARDWARE_SSR: return "SSR";
        default: return "None";
    }
}

/**
 * Get control mode name
 */
const char* output_manager_get_mode_name(ControlMode_t mode) {
    switch (mode) {
        case CONTROL_MODE_OFF: return "Off";
        case CONTROL_MODE_MANUAL: return "Manual";
        case CONTROL_MODE_PID: return "PID";
        case CONTROL_MODE_ONOFF: return "On/Off";
        case CONTROL_MODE_SCHEDULE: return "Schedule";
        default: return "Unknown";
    }
}

/**
 * Check device/hardware compatibility
 */
bool output_manager_is_compatible(DeviceType_t deviceType, HardwareType_t hardwareType) {
    if (deviceType == DEVICE_LIGHT) {
        // Lights only work with AC dimmer
        return (hardwareType == HARDWARE_DIMMER_AC);
    } else {
        // Heat devices only work with SSR
        return (hardwareType == HARDWARE_SSR);
    }
}

/**
 * Get output index by name
 */
int output_manager_get_output_by_name(const char* name) {
    if (!name) {
        return -1;
    }

    for (int i = 0; i < MAX_OUTPUTS; i++) {
        if (strcmp(outputs[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}
