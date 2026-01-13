/**
 * output_manager.h
 * Multi-Output Control System
 *
 * Manages 3 independent heating/lighting outputs:
 * - Output 1: Lights (AC dimmer only)
 * - Output 2: Heat devices (SSR only)
 * - Output 3: Heat devices (SSR only)
 */

#ifndef OUTPUT_MANAGER_H
#define OUTPUT_MANAGER_H

#include <Arduino.h>

#define MAX_OUTPUTS 3
#define MAX_SCHEDULE_SLOTS 8

/**
 * Schedule slot structure (per-output scheduling)
 */
typedef struct {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    float targetTemp;
    char days[8];  // "SMTWTFS" format
} ScheduleSlot_t;

/**
 * Hardware types
 */
typedef enum {
    HARDWARE_NONE = 0,
    HARDWARE_DIMMER_AC,    // AC dimmer (RobotDyn) - Output 1 only
    HARDWARE_SSR           // SSR (pulse or on/off) - Outputs 2 & 3 only
} HardwareType_t;

/**
 * Control modes
 */
typedef enum {
    CONTROL_MODE_OFF = 0,     // Output disabled
    CONTROL_MODE_MANUAL,      // Manual power setting
    CONTROL_MODE_PID,         // PID temperature control
    CONTROL_MODE_ONOFF,       // Simple thermostat (on/off)
    CONTROL_MODE_SCHEDULE     // Schedule-based control
} ControlMode_t;

/**
 * Device types (for hardware restrictions)
 */
typedef enum {
    DEVICE_LIGHT = 0,         // Lights (dimmer only)
    DEVICE_HEAT_MAT,          // Heat mat (SSR)
    DEVICE_CERAMIC_HEATER,    // Ceramic heater (SSR)
    DEVICE_HEAT_CABLE,        // Heat cable (SSR)
    DEVICE_FOGGER,            // Fogger (SSR)
    DEVICE_MISTER            // Mister (SSR)
} DeviceType_t;

/**
 * Output configuration
 */
typedef struct {
    bool enabled;
    char name[32];
    HardwareType_t hardwareType;
    DeviceType_t deviceType;
    uint8_t controlPin;
    ControlMode_t controlMode;
    char sensorAddress[17];     // DS18B20 ROM address
    float targetTemp;
    float currentTemp;
    int manualPower;            // Manual power % (0-100)
    int currentPower;           // Actual output power %
    bool heating;               // Currently heating

    // PID parameters
    float pidKp;
    float pidKi;
    float pidKd;
    float pidIntegral;
    float pidLastError;
    unsigned long pidLastTime;

    // Schedule
    ScheduleSlot_t schedule[MAX_SCHEDULE_SLOTS];
} OutputConfig_t;

/**
 * Initialize output manager
 * Sets up hardware pins and default configurations
 */
void output_manager_init(void);

/**
 * Get output configuration
 * @param outputIndex Output index (0-2)
 * @return Pointer to output config, or nullptr if invalid
 */
OutputConfig_t* output_manager_get_output(int outputIndex);

/**
 * Update output control loop
 * Call this regularly (e.g., every 100ms) to update all outputs
 */
void output_manager_update(void);

/**
 * Set output enable/disable
 * @param outputIndex Output index (0-2)
 * @param enabled Enable or disable
 */
void output_manager_set_enabled(int outputIndex, bool enabled);

/**
 * Set output name
 * @param outputIndex Output index (0-2)
 * @param name New name (max 31 chars)
 */
void output_manager_set_name(int outputIndex, const char* name);

/**
 * Set hardware type
 * @param outputIndex Output index (0-2)
 * @param hardwareType Hardware type (enforces restrictions)
 * @return true if allowed, false if restricted
 */
bool output_manager_set_hardware_type(int outputIndex, HardwareType_t hardwareType);

/**
 * Set device type
 * @param outputIndex Output index (0-2)
 * @param deviceType Device type (enforces hardware restrictions)
 * @return true if compatible, false if incompatible
 */
bool output_manager_set_device_type(int outputIndex, DeviceType_t deviceType);

/**
 * Set control mode
 * @param outputIndex Output index (0-2)
 * @param mode Control mode
 */
void output_manager_set_mode(int outputIndex, ControlMode_t mode);

/**
 * Set target temperature
 * @param outputIndex Output index (0-2)
 * @param targetTemp Target temperature in °C
 */
void output_manager_set_target(int outputIndex, float targetTemp);

/**
 * Set manual power
 * @param outputIndex Output index (0-2)
 * @param power Power percentage (0-100)
 */
void output_manager_set_manual_power(int outputIndex, int power);

/**
 * Assign sensor to output
 * @param outputIndex Output index (0-2)
 * @param sensorAddress Sensor ROM address string
 */
void output_manager_set_sensor(int outputIndex, const char* sensorAddress);

/**
 * Set PID parameters
 * @param outputIndex Output index (0-2)
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 */
void output_manager_set_pid_params(int outputIndex, float kp, float ki, float kd);

/**
 * Set schedule slot
 * @param outputIndex Output index (0-2)
 * @param slotIndex Slot index (0-7)
 * @param enabled Enable slot
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 * @param targetTemp Target temperature
 * @return true if successful
 */
bool output_manager_set_schedule_slot(int outputIndex, int slotIndex,
                                      bool enabled, uint8_t hour, uint8_t minute, float targetTemp);

/**
 * Load configuration from preferences
 */
void output_manager_load_config(void);

/**
 * Save configuration to preferences
 */
void output_manager_save_config(void);

/**
 * Get device type name
 * @param deviceType Device type
 * @return Device type name string
 */
const char* output_manager_get_device_type_name(DeviceType_t deviceType);

/**
 * Get hardware type name
 * @param hardwareType Hardware type
 * @return Hardware type name string
 */
const char* output_manager_get_hardware_type_name(HardwareType_t hardwareType);

/**
 * Get control mode name
 * @param mode Control mode
 * @return Mode name string
 */
const char* output_manager_get_mode_name(ControlMode_t mode);

/**
 * Check if device type is compatible with hardware type
 * @param deviceType Device type
 * @param hardwareType Hardware type
 * @return true if compatible
 */
bool output_manager_is_compatible(DeviceType_t deviceType, HardwareType_t hardwareType);

/**
 * Get output index by name
 * @param name Output name
 * @return Output index (0-2), or -1 if not found
 */
int output_manager_get_output_by_name(const char* name);

#endif // OUTPUT_MANAGER_H
