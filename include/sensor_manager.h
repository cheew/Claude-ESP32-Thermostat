/**
 * sensor_manager.h
 * DS18B20 Temperature Sensor Management
 *
 * Handles multiple DS18B20 sensors on OneWire bus:
 * - Discovery and enumeration
 * - Sensor naming and identification
 * - Reading multiple sensors
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

#define MAX_SENSORS 6  // Support up to 6 DS18B20 sensors

/**
 * Sensor information structure
 */
typedef struct {
    bool discovered;              // Sensor found on bus
    uint8_t address[8];           // 64-bit ROM address
    char addressString[17];       // Hex string (e.g., "28FF1A2B3C4D5E6F")
    char name[32];                // User-friendly name (e.g., "Basking Spot")
    float lastReading;            // Last temperature reading
    unsigned long lastReadTime;   // When last read
    int errorCount;               // Consecutive read errors
} SensorInfo_t;

/**
 * Initialize sensor manager
 * @param oneWirePin GPIO pin for OneWire bus
 */
void sensor_manager_init(uint8_t oneWirePin);

/**
 * Scan OneWire bus for DS18B20 sensors
 * @return Number of sensors found
 */
int sensor_manager_scan(void);

/**
 * Get number of discovered sensors
 * @return Sensor count
 */
int sensor_manager_get_count(void);

/**
 * Get sensor info by index
 * @param index Sensor index (0 to count-1)
 * @return Pointer to sensor info, or nullptr if invalid
 */
const SensorInfo_t* sensor_manager_get_sensor(int index);

/**
 * Get sensor info by address string
 * @param addressString Hex address (e.g., "28FF1A2B3C4D5E6F")
 * @return Pointer to sensor info, or nullptr if not found
 */
const SensorInfo_t* sensor_manager_get_sensor_by_address(const char* addressString);

/**
 * Read temperature from specific sensor
 * @param index Sensor index
 * @param temperature Output temperature value
 * @return true if read successful, false on error
 */
bool sensor_manager_read_sensor(int index, float* temperature);

/**
 * Read all sensors
 * Updates lastReading and lastReadTime for all discovered sensors
 */
void sensor_manager_read_all(void);

/**
 * Set user-friendly name for sensor
 * @param index Sensor index
 * @param name New name (max 31 chars)
 * @return true if successful
 */
bool sensor_manager_set_name(int index, const char* name);

/**
 * Load sensor names from preferences
 */
void sensor_manager_load_names(void);

/**
 * Save sensor names to preferences
 */
void sensor_manager_save_names(void);

/**
 * Get default name for sensor index
 * @param index Sensor index
 * @param buffer Output buffer
 * @param maxLen Buffer size
 */
void sensor_manager_get_default_name(int index, char* buffer, size_t maxLen);

/**
 * Validate temperature reading
 * @param temp Temperature value
 * @return true if valid
 */
bool sensor_manager_is_valid_temp(float temp);

#endif // SENSOR_MANAGER_H
