/**
 * output_manager.cpp
 * Multi-Output Control System Implementation
 */

#include "output_manager.h"
#include "sensor_manager.h"
#include "weather_client.h"
#include "profile_manager.h"
#include "console.h"
#include <RBDdimmer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Hardware pin assignments (ESP32-S3 - per PINOUT_WIRING.md)
#define OUTPUT1_PIN 15     // AC Dimmer PWM (GPIO 15)
#define OUTPUT2_PIN 5      // SSR control (GPIO 5)
#define OUTPUT3_PIN 6      // SSR control (GPIO 6)
#define ZEROCROSS_PIN 16   // Shared zero-cross for dimmer (GPIO 16)

// PID limits
#define PID_OUTPUT_MIN 0
#define PID_OUTPUT_MAX 100
#define PID_INTEGRAL_MAX 100.0f

// Output array
static OutputConfig_t outputs[MAX_OUTPUTS];

// Hardware objects
static dimmerLamp* dimmer1 = nullptr;  // Output 1 (AC dimmer)

// Default safety limits
#define DEFAULT_MAX_TEMP_C 40.0f
#define DEFAULT_MIN_TEMP_C 5.0f
#define DEFAULT_FAULT_TIMEOUT_SEC 30
#define DEFAULT_CAP_POWER_PCT 30

// Forward declarations
static void updateOutput(int index);
static void updatePID(int index);
static void updateTimeProp(int index);
static void resetTimePropState(int index);
static void updateSchedule(int index);
static void setOutputPower(int index, int power);
static void checkSensorHealth(int index);
static void checkTemperatureLimits(int index);
static void handleFaultState(int index);

/**
 * Initialize output manager
 */
void output_manager_init(void) {
    Serial.println("[OutputMgr] Initializing...");

    // Clear output array
    memset(outputs, 0, sizeof(outputs));

    // Initialize all outputs with safety defaults
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        outputs[i].maxTempC = DEFAULT_MAX_TEMP_C;
        outputs[i].minTempC = DEFAULT_MIN_TEMP_C;
        outputs[i].faultTimeoutSec = DEFAULT_FAULT_TIMEOUT_SEC;
        outputs[i].faultMode = FAULT_MODE_OFF;
        outputs[i].capPowerPct = DEFAULT_CAP_POWER_PCT;
        outputs[i].autoResumeOnSensorOk = false;
        outputs[i].sensorHealth = SENSOR_OK;
        outputs[i].faultState = FAULT_NONE;
        outputs[i].lastValidReadTime = millis();
        outputs[i].lastValidTemp = 20.0f;
        outputs[i].lastValidPower = 0;
        outputs[i].faultStartTime = 0;

        // Time-proportional defaults
        outputs[i].timePropCycleSec = 30;      // 30 second default cycle
        outputs[i].timePropMinOnSec = 1;       // 1 second minimum ON
        outputs[i].timePropMinOffSec = 1;      // 1 second minimum OFF
        outputs[i].timePropCycleStart = 0;
        outputs[i].timePropCurrentState = false;
        outputs[i].timePropDutyCycle = 0.0f;
    }

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
        // Always update current temperature from sensor (even if disabled)
        const SensorInfo_t* sensor = sensor_manager_get_sensor_by_address(outputs[i].sensorAddress);
        if (sensor && sensor->discovered) {
            outputs[i].currentTemp = sensor->lastReading;

            // Track valid readings for fault recovery
            if (sensor_manager_is_valid_temp(outputs[i].currentTemp)) {
                outputs[i].lastValidReadTime = millis();
                outputs[i].lastValidTemp = outputs[i].currentTemp;
            }
        } else if (strlen(outputs[i].sensorAddress) == 0) {
            // No sensor assigned - keep showing last known temp or 0
        } else {
            // Sensor assigned but not found
            outputs[i].currentTemp = -127.0f;
        }

        if (outputs[i].enabled) {
            // Check sensor health first
            checkSensorHealth(i);

            // Check temperature limits (hard cutoffs)
            checkTemperatureLimits(i);

            // Handle any active fault state
            if (outputs[i].faultState != FAULT_NONE) {
                handleFaultState(i);
            } else {
                // Normal operation
                updateOutput(i);
            }
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

    // Note: currentTemp is already updated in output_manager_update()

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
            output->lastValidPower = output->currentPower;  // Track for fault recovery
            break;

        case CONTROL_MODE_PID:
            if (sensor_manager_is_valid_temp(output->currentTemp)) {
                updatePID(index);
                output->lastValidPower = output->currentPower;  // Track for fault recovery
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
                output->lastValidPower = output->currentPower;  // Track for fault recovery
            } else {
                setOutputPower(index, 0);
                output->currentPower = 0;
                output->heating = false;
            }
            break;

        case CONTROL_MODE_SCHEDULE:
            updateSchedule(index);
            output->lastValidPower = output->currentPower;  // Track for fault recovery
            break;

        case CONTROL_MODE_TIME_PROP:
            if (sensor_manager_is_valid_temp(output->currentTemp)) {
                updateTimeProp(index);
                output->lastValidPower = output->currentPower;  // Track for fault recovery
            } else {
                setOutputPower(index, 0);
                output->currentPower = 0;
                output->heating = false;
            }
            break;

        case CONTROL_MODE_WEATHER:
            // Weather sync mode: target follows outdoor forecast, clamped to profile bounds.
            // Uses PID to reach the weather-derived target temperature.
            {
                float baseTarget = output->targetTemp;

                // Get profile bounds for clamping
                const char* activeId = profile_manager_get_active_id();
                if (activeId && strlen(activeId) > 0) {
                    AnimalProfile_t profile;
                    if (profile_manager_load(activeId, &profile)) {
                        baseTarget = (profile.tempDayMin + profile.tempDayMax) / 2.0f;
                        output->targetTemp = weather_client_adjust_target(
                            profile.tempDayMin, profile.tempDayMax, baseTarget);
                    }
                }

                if (sensor_manager_is_valid_temp(output->currentTemp)) {
                    updatePID(index);
                    output->lastValidPower = output->currentPower;
                } else {
                    setOutputPower(index, 0);
                    output->currentPower = 0;
                    output->heating = false;
                }
            }
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
 * Reset time-proportional cycle state
 */
static void resetTimePropState(int index) {
    OutputConfig_t* output = &outputs[index];
    output->timePropCycleStart = millis();
    output->timePropCurrentState = false;
    output->timePropDutyCycle = 0.0f;
}

/**
 * Update time-proportional control
 * Converts PID output percentage into timed ON/OFF cycles
 * PID runs continuously, duty cycle applied to fixed-length cycles
 */
static void updateTimeProp(int index) {
    OutputConfig_t* output = &outputs[index];
    unsigned long now = millis();

    // Calculate cycle duration in milliseconds
    unsigned long cycleDurationMs = (unsigned long)output->timePropCycleSec * 1000UL;

    // Always run PID calculation (every ~100ms update) for responsive control
    float dt = (now - output->pidLastTime) / 1000.0f;
    if (dt >= 0.1f) {
        float error = output->targetTemp - output->currentTemp;

        // Proportional
        float P = output->pidKp * error;

        // Integral with anti-windup
        output->pidIntegral += error * dt;
        if (output->pidIntegral > PID_INTEGRAL_MAX) {
            output->pidIntegral = PID_INTEGRAL_MAX;
        } else if (output->pidIntegral < -PID_INTEGRAL_MAX) {
            output->pidIntegral = -PID_INTEGRAL_MAX;
        }
        float I = output->pidKi * output->pidIntegral;

        // Derivative
        float D = 0.0f;
        if (dt > 0.0f) {
            D = output->pidKd * (error - output->pidLastError) / dt;
        }

        // Calculate and clamp duty cycle
        output->timePropDutyCycle = P + I + D;
        if (output->timePropDutyCycle < 0) output->timePropDutyCycle = 0;
        if (output->timePropDutyCycle > 100) output->timePropDutyCycle = 100;

        output->pidLastError = error;
        output->pidLastTime = now;
    }

    // Check if we need to start a new cycle (separate from PID calculation)
    if (output->timePropCycleStart == 0 || (now - output->timePropCycleStart >= cycleDurationMs)) {
        output->timePropCycleStart = now;
    }

    // Calculate ON time for this cycle based on current duty cycle
    unsigned long onTimeMs = (unsigned long)((output->timePropDutyCycle / 100.0f) * cycleDurationMs);
    unsigned long minOnMs = (unsigned long)output->timePropMinOnSec * 1000UL;
    unsigned long minOffMs = (unsigned long)output->timePropMinOffSec * 1000UL;

    // Apply minimum on/off time constraints
    if (onTimeMs > 0 && onTimeMs < minOnMs) {
        onTimeMs = minOnMs;  // Ensure minimum ON time
    }
    if (onTimeMs > cycleDurationMs - minOffMs) {
        onTimeMs = cycleDurationMs - minOffMs;  // Ensure minimum OFF time
    }

    // Special cases for very low or very high duty cycles
    if (output->timePropDutyCycle < 2.0f) {
        onTimeMs = 0;  // Too low, stay off entirely
    }
    if (output->timePropDutyCycle > 98.0f) {
        onTimeMs = cycleDurationMs;  // Nearly full, stay on entirely
    }

    // Determine current state based on time within cycle
    unsigned long timeInCycle = now - output->timePropCycleStart;
    bool shouldBeOn = (timeInCycle < onTimeMs);

    // Set output state
    if (shouldBeOn) {
        setOutputPower(index, 100);
        output->currentPower = (int)output->timePropDutyCycle;  // Show duty cycle %
        output->heating = true;
    } else {
        setOutputPower(index, 0);
        output->currentPower = (int)output->timePropDutyCycle;  // Still show duty cycle %
        output->heating = false;
    }

    output->timePropCurrentState = shouldBeOn;
}

/**
 * Update schedule control
 */
static bool isSlotActiveToday(const ScheduleSlot_t* slot, int dayOfWeek) {
    // If days field is empty, slot is active every day
    if (strlen(slot->days) == 0) return true;

    const char dayChars[] = "SMTWTFS";
    if (dayOfWeek < 0 || dayOfWeek > 6) return true;
    char todayChar = dayChars[dayOfWeek];

    return (strchr(slot->days, todayChar) != NULL);
}

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
    int dayOfWeek = timeinfo.tm_wday; // 0=Sunday, 6=Saturday

    // Find active schedule slot (most recent past slot that is active today)
    int activeSlot = -1;
    int minDiff = 24 * 60;

    for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
        if (!output->schedule[i].enabled) continue;
        if (!isSlotActiveToday(&output->schedule[i], dayOfWeek)) continue;

        int slotTotalMinutes = output->schedule[i].hour * 60 + output->schedule[i].minute;
        int diff = currentTotalMinutes - slotTotalMinutes;

        if (diff >= 0 && diff < minDiff) {
            minDiff = diff;
            activeSlot = i;
        }
    }

    // If no slot found today before current time, wrap around to latest slot from yesterday
    if (activeSlot < 0) {
        int latestSlotTime = -1;
        for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
            if (!output->schedule[i].enabled) continue;
            // Check if it was active yesterday
            int yesterday = (dayOfWeek + 6) % 7;
            if (!isSlotActiveToday(&output->schedule[i], yesterday)) continue;

            int slotTime = output->schedule[i].hour * 60 + output->schedule[i].minute;
            if (slotTime > latestSlotTime) {
                latestSlotTime = slotTime;
                activeSlot = i;
            }
        }
    }

    if (activeSlot >= 0) {
        float targetTemp = output->schedule[activeSlot].targetTemp;

        // Temperature ramping: interpolate between active slot and next slot
        if (output->schedule[activeSlot].rampToNext) {
            // Find next enabled slot after active slot (by time)
            int nextSlot = -1;
            int activeTime = output->schedule[activeSlot].hour * 60 + output->schedule[activeSlot].minute;
            int smallestGap = 24 * 60 + 1;

            for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
                if (i == activeSlot || !output->schedule[i].enabled) continue;
                if (!isSlotActiveToday(&output->schedule[i], dayOfWeek)) continue;

                int slotTime = output->schedule[i].hour * 60 + output->schedule[i].minute;
                int gap = slotTime - activeTime;
                if (gap <= 0) gap += 24 * 60; // Wrap around midnight

                if (gap < smallestGap) {
                    smallestGap = gap;
                    nextSlot = i;
                }
            }

            if (nextSlot >= 0 && smallestGap > 0) {
                float nextTemp = output->schedule[nextSlot].targetTemp;
                // Calculate interpolation progress (0.0 to 1.0)
                float progress = (float)minDiff / (float)smallestGap;
                if (progress > 1.0f) progress = 1.0f;
                targetTemp = output->schedule[activeSlot].targetTemp +
                             (nextTemp - output->schedule[activeSlot].targetTemp) * progress;
            }
        }

        // Weather-driven adjustment: shift target within active profile bounds
        if (weather_client_is_available()) {
            const char* activeId = profile_manager_get_active_id();
            if (activeId && strlen(activeId) > 0) {
                AnimalProfile_t profile;
                if (profile_manager_load(activeId, &profile)) {
                    // Use day range as bounds for weather adjustment
                    targetTemp = weather_client_adjust_target(
                        profile.tempDayMin, profile.tempDayMax, targetTemp);
                }
            }
        }

        // Apply schedule target temperature
        output->targetTemp = targetTemp;

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

    // Reset time-prop state when entering that mode
    if (mode == CONTROL_MODE_TIME_PROP) {
        resetTimePropState(outputIndex);
    }

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
 * Set time-proportional control parameters
 */
void output_manager_set_time_prop_params(int outputIndex, uint8_t cycleSec,
                                          uint8_t minOnSec, uint8_t minOffSec) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return;
    }

    // Clamp cycle time to valid range (5-120 seconds)
    if (cycleSec < 5) cycleSec = 5;
    if (cycleSec > 120) cycleSec = 120;

    // Ensure min times don't exceed half the cycle
    uint8_t maxMinTime = cycleSec / 2;
    if (minOnSec > maxMinTime) minOnSec = maxMinTime;
    if (minOffSec > maxMinTime) minOffSec = maxMinTime;
    if (minOnSec < 1) minOnSec = 1;
    if (minOffSec < 1) minOffSec = 1;

    outputs[outputIndex].timePropCycleSec = cycleSec;
    outputs[outputIndex].timePropMinOnSec = minOnSec;
    outputs[outputIndex].timePropMinOffSec = minOffSec;

    // Reset cycle state
    resetTimePropState(outputIndex);
}

/**
 * Set schedule slot
 */
bool output_manager_set_schedule_slot(int outputIndex, int slotIndex,
                                      bool enabled, uint8_t hour, uint8_t minute, float targetTemp,
                                      const char* days, bool rampToNext, const char* label) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return false;
    }
    if (slotIndex < 0 || slotIndex >= MAX_SCHEDULE_SLOTS) {
        return false;
    }
    if (hour > 23 || minute > 59) {
        return false;
    }

    ScheduleSlot_t* slot = &outputs[outputIndex].schedule[slotIndex];
    slot->enabled = enabled;
    slot->hour = hour;
    slot->minute = minute;
    slot->targetTemp = targetTemp;
    slot->rampToNext = rampToNext;

    if (days) {
        strncpy(slot->days, days, sizeof(slot->days) - 1);
        slot->days[sizeof(slot->days) - 1] = '\0';
    } else {
        slot->days[0] = '\0';
    }

    if (label) {
        strncpy(slot->label, label, sizeof(slot->label) - 1);
        slot->label[sizeof(slot->label) - 1] = '\0';
    } else {
        slot->label[0] = '\0';
    }

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

        // Load time-proportional params
        outputs[i].timePropCycleSec = prefs.getUChar("tpCycleSec", 30);
        outputs[i].timePropMinOnSec = prefs.getUChar("tpMinOnSec", 1);
        outputs[i].timePropMinOffSec = prefs.getUChar("tpMinOffSec", 1);

        // Load safety settings
        outputs[i].maxTempC = prefs.getFloat("maxTempC", DEFAULT_MAX_TEMP_C);
        outputs[i].minTempC = prefs.getFloat("minTempC", DEFAULT_MIN_TEMP_C);
        outputs[i].faultTimeoutSec = prefs.getUShort("faultTimeout", DEFAULT_FAULT_TIMEOUT_SEC);
        outputs[i].faultMode = (FaultMode_t)prefs.getUChar("faultMode", FAULT_MODE_OFF);
        outputs[i].capPowerPct = prefs.getUChar("capPowerPct", DEFAULT_CAP_POWER_PCT);
        outputs[i].autoResumeOnSensorOk = prefs.getBool("autoResume", false);

        prefs.end();

        // Load schedule from LittleFS
        char schPath[40];
        snprintf(schPath, sizeof(schPath), "/config/output%d_schedule.json", i + 1);
        File schFile = LittleFS.open(schPath, "r");
        if (schFile) {
            DynamicJsonDocument doc(4096);
            DeserializationError err = deserializeJson(doc, schFile);
            schFile.close();
            if (!err) {
                JsonArray slots = doc["slots"].as<JsonArray>();
                int j = 0;
                for (JsonObject slot : slots) {
                    if (j >= MAX_SCHEDULE_SLOTS) break;
                    outputs[i].schedule[j].enabled = slot["en"] | false;
                    outputs[i].schedule[j].hour = slot["hr"] | 0;
                    outputs[i].schedule[j].minute = slot["min"] | 0;
                    outputs[i].schedule[j].targetTemp = slot["temp"] | 25.0f;
                    outputs[i].schedule[j].rampToNext = slot["ramp"] | false;
                    strncpy(outputs[i].schedule[j].days,
                            slot["days"] | "", sizeof(outputs[i].schedule[j].days) - 1);
                    strncpy(outputs[i].schedule[j].label,
                            slot["lbl"] | "", sizeof(outputs[i].schedule[j].label) - 1);
                    j++;
                }
            }
        }
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

        // Save time-proportional params
        prefs.putUChar("tpCycleSec", outputs[i].timePropCycleSec);
        prefs.putUChar("tpMinOnSec", outputs[i].timePropMinOnSec);
        prefs.putUChar("tpMinOffSec", outputs[i].timePropMinOffSec);

        // Save safety settings
        prefs.putFloat("maxTempC", outputs[i].maxTempC);
        prefs.putFloat("minTempC", outputs[i].minTempC);
        prefs.putUShort("faultTimeout", outputs[i].faultTimeoutSec);
        prefs.putUChar("faultMode", outputs[i].faultMode);
        prefs.putUChar("capPowerPct", outputs[i].capPowerPct);
        prefs.putBool("autoResume", outputs[i].autoResumeOnSensorOk);

        prefs.end();

        // Save schedule to LittleFS
        LittleFS.mkdir("/config");
        char schPath[40];
        snprintf(schPath, sizeof(schPath), "/config/output%d_schedule.json", i + 1);
        File schFile = LittleFS.open(schPath, "w");
        if (schFile) {
            DynamicJsonDocument doc(4096);
            JsonArray slots = doc.createNestedArray("slots");
            for (int j = 0; j < MAX_SCHEDULE_SLOTS; j++) {
                JsonObject slot = slots.createNestedObject();
                slot["en"] = outputs[i].schedule[j].enabled;
                slot["hr"] = outputs[i].schedule[j].hour;
                slot["min"] = outputs[i].schedule[j].minute;
                slot["temp"] = outputs[i].schedule[j].targetTemp;
                slot["ramp"] = outputs[i].schedule[j].rampToNext;
                slot["days"] = outputs[i].schedule[j].days;
                slot["lbl"] = outputs[i].schedule[j].label;
            }
            serializeJson(doc, schFile);
            schFile.close();
        }
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
        case CONTROL_MODE_ONOFF: return "OnOff";
        case CONTROL_MODE_SCHEDULE: return "Schedule";
        case CONTROL_MODE_TIME_PROP: return "TimeProp";
        case CONTROL_MODE_WEATHER: return "Weather";
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

/**
 * Check sensor health status
 */
static void checkSensorHealth(int index) {
    OutputConfig_t* output = &outputs[index];

    // Skip if no sensor assigned or in manual/off mode
    if (strlen(output->sensorAddress) == 0 ||
        output->controlMode == CONTROL_MODE_OFF ||
        output->controlMode == CONTROL_MODE_MANUAL) {
        output->sensorHealth = SENSOR_OK;
        return;
    }

    // Check for invalid reading
    if (!sensor_manager_is_valid_temp(output->currentTemp)) {
        if (output->sensorHealth != SENSOR_ERROR) {
            output->sensorHealth = SENSOR_ERROR;
            if (output->faultState == FAULT_NONE) {
                output->faultState = FAULT_SENSOR_ERROR;
                output->faultStartTime = millis();
                console_add_event_f(CONSOLE_EVENT_SYSTEM, "Output %d: SENSOR ERROR", index + 1);
            }
        }
        return;
    }

    // Check for stale reading
    unsigned long timeSinceValid = (millis() - output->lastValidReadTime) / 1000;
    if (timeSinceValid > output->faultTimeoutSec) {
        if (output->sensorHealth != SENSOR_STALE) {
            output->sensorHealth = SENSOR_STALE;
            if (output->faultState == FAULT_NONE) {
                output->faultState = FAULT_SENSOR_STALE;
                output->faultStartTime = millis();
                console_add_event_f(CONSOLE_EVENT_SYSTEM, "Output %d: SENSOR STALE (%lus)", index + 1, timeSinceValid);
            }
        }
        return;
    }

    // Sensor is healthy - check if we should auto-resume
    if (output->sensorHealth != SENSOR_OK) {
        output->sensorHealth = SENSOR_OK;

        // Auto-resume if configured and fault was sensor-related
        if (output->autoResumeOnSensorOk &&
            (output->faultState == FAULT_SENSOR_STALE || output->faultState == FAULT_SENSOR_ERROR)) {
            output->faultState = FAULT_NONE;
            console_add_event_f(CONSOLE_EVENT_SYSTEM, "Output %d: Sensor recovered, resuming", index + 1);
        }
    }
}

/**
 * Check temperature limits (hard cutoffs)
 */
static void checkTemperatureLimits(int index) {
    OutputConfig_t* output = &outputs[index];

    // Skip if no valid temp or already in fault
    if (!sensor_manager_is_valid_temp(output->currentTemp)) {
        return;
    }

    // Check over-temp (highest priority fault)
    if (output->currentTemp >= output->maxTempC) {
        if (output->faultState != FAULT_OVER_TEMP) {
            output->faultState = FAULT_OVER_TEMP;
            output->faultStartTime = millis();
            console_add_event_f(CONSOLE_EVENT_SYSTEM,
                "Output %d: OVER TEMP! %.1fC >= %.1fC",
                index + 1, output->currentTemp, output->maxTempC);
        }
        return;
    }

    // Check under-temp
    if (output->currentTemp <= output->minTempC) {
        if (output->faultState != FAULT_UNDER_TEMP) {
            output->faultState = FAULT_UNDER_TEMP;
            output->faultStartTime = millis();
            console_add_event_f(CONSOLE_EVENT_SYSTEM,
                "Output %d: UNDER TEMP! %.1fC <= %.1fC",
                index + 1, output->currentTemp, output->minTempC);
        }
        return;
    }

    // If we were in over/under temp and now back in range, clear fault
    if (output->faultState == FAULT_OVER_TEMP || output->faultState == FAULT_UNDER_TEMP) {
        // Add hysteresis: must be 1C away from limit to clear
        bool clearOverTemp = (output->faultState == FAULT_OVER_TEMP &&
                              output->currentTemp < output->maxTempC - 1.0f);
        bool clearUnderTemp = (output->faultState == FAULT_UNDER_TEMP &&
                               output->currentTemp > output->minTempC + 1.0f);

        if (clearOverTemp || clearUnderTemp) {
            output->faultState = FAULT_NONE;
            console_add_event_f(CONSOLE_EVENT_SYSTEM,
                "Output %d: Temp back in range (%.1fC)", index + 1, output->currentTemp);
        }
    }
}

/**
 * Handle active fault state
 */
static void handleFaultState(int index) {
    OutputConfig_t* output = &outputs[index];

    // Over-temp always forces OFF regardless of fault mode
    if (output->faultState == FAULT_OVER_TEMP) {
        setOutputPower(index, 0);
        output->currentPower = 0;
        output->heating = false;
        return;
    }

    // Apply fault mode for other faults
    switch (output->faultMode) {
        case FAULT_MODE_OFF:
            setOutputPower(index, 0);
            output->currentPower = 0;
            output->heating = false;
            break;

        case FAULT_MODE_HOLD_LAST:
            // Maintain last known good power
            setOutputPower(index, output->lastValidPower);
            output->currentPower = output->lastValidPower;
            output->heating = (output->lastValidPower > 5);
            break;

        case FAULT_MODE_CAP_POWER:
            // Cap to configured percentage
            setOutputPower(index, output->capPowerPct);
            output->currentPower = output->capPowerPct;
            output->heating = (output->capPowerPct > 5);
            break;
    }
}

/**
 * Set safety limits
 */
void output_manager_set_safety_limits(int outputIndex, float maxTempC, float minTempC, uint16_t faultTimeoutSec) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return;
    }

    outputs[outputIndex].maxTempC = maxTempC;
    outputs[outputIndex].minTempC = minTempC;
    outputs[outputIndex].faultTimeoutSec = faultTimeoutSec;

    console_add_event_f(CONSOLE_EVENT_SYSTEM,
        "Output %d limits: %.1f-%.1fC, timeout %ds",
        outputIndex + 1, minTempC, maxTempC, faultTimeoutSec);
}

/**
 * Set fault mode
 */
void output_manager_set_fault_mode(int outputIndex, FaultMode_t faultMode, uint8_t capPowerPct) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return;
    }

    outputs[outputIndex].faultMode = faultMode;
    outputs[outputIndex].capPowerPct = capPowerPct;
}

/**
 * Clear fault state (manual reset)
 */
bool output_manager_clear_fault(int outputIndex) {
    if (outputIndex < 0 || outputIndex >= MAX_OUTPUTS) {
        return false;
    }

    OutputConfig_t* output = &outputs[outputIndex];

    // Can't clear over-temp if still over temp
    if (output->faultState == FAULT_OVER_TEMP &&
        sensor_manager_is_valid_temp(output->currentTemp) &&
        output->currentTemp >= output->maxTempC) {
        return false;
    }

    // Can't clear sensor fault if sensor still bad
    if ((output->faultState == FAULT_SENSOR_ERROR || output->faultState == FAULT_SENSOR_STALE) &&
        !sensor_manager_is_valid_temp(output->currentTemp)) {
        return false;
    }

    output->faultState = FAULT_NONE;
    output->sensorHealth = SENSOR_OK;
    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Output %d: Fault cleared", outputIndex + 1);
    return true;
}

/**
 * Get fault state name
 */
const char* output_manager_get_fault_name(FaultState_t fault) {
    switch (fault) {
        case FAULT_NONE: return "None";
        case FAULT_SENSOR_STALE: return "Sensor Stale";
        case FAULT_SENSOR_ERROR: return "Sensor Error";
        case FAULT_OVER_TEMP: return "Over Temp";
        case FAULT_UNDER_TEMP: return "Under Temp";
        case FAULT_HEATER_NO_RISE: return "Heater No Rise";
        case FAULT_HEATER_RUNAWAY: return "Heater Runaway";
        default: return "Unknown";
    }
}

/**
 * Get sensor health name
 */
const char* output_manager_get_sensor_health_name(SensorHealth_t health) {
    switch (health) {
        case SENSOR_OK: return "OK";
        case SENSOR_STALE: return "Stale";
        case SENSOR_ERROR: return "Error";
        default: return "Unknown";
    }
}
