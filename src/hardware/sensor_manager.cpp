/**
 * sensor_manager.cpp
 * DS18B20 Temperature Sensor Management Implementation
 */

#include "sensor_manager.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

// OneWire and sensor objects
static OneWire* oneWire = nullptr;
static DallasTemperature* sensors = nullptr;
static uint8_t oneWirePin = 0;

// Sensor array
static SensorInfo_t sensorArray[MAX_SENSORS];
static int sensorCount = 0;

/**
 * Initialize sensor manager
 */
void sensor_manager_init(uint8_t pin) {
    oneWirePin = pin;

    // Initialize OneWire and DallasTemperature
    oneWire = new OneWire(oneWirePin);
    sensors = new DallasTemperature(oneWire);
    sensors->begin();

    // Clear sensor array
    memset(sensorArray, 0, sizeof(sensorArray));
    sensorCount = 0;

    Serial.println("[SensorMgr] Initialized");

    // Perform initial scan
    int found = sensor_manager_scan();
    Serial.printf("[SensorMgr] Found %d sensors\n", found);

    // Load saved names
    sensor_manager_load_names();
}

/**
 * Scan OneWire bus for DS18B20 sensors
 */
int sensor_manager_scan(void) {
    if (!sensors) {
        Serial.println("[SensorMgr] Not initialized");
        return 0;
    }

    Serial.println("[SensorMgr] Scanning for DS18B20 sensors...");

    // Clear previous scan results
    memset(sensorArray, 0, sizeof(sensorArray));
    sensorCount = 0;

    // Search for devices
    uint8_t address[8];
    oneWire->reset_search();

    while (oneWire->search(address)) {
        // Verify CRC
        if (OneWire::crc8(address, 7) != address[7]) {
            Serial.println("[SensorMgr] CRC error, skipping device");
            continue;
        }

        // Check device family (0x28 = DS18B20)
        if (address[0] != 0x28) {
            Serial.printf("[SensorMgr] Not a DS18B20 (family: 0x%02X), skipping\n", address[0]);
            continue;
        }

        // Add to array
        if (sensorCount < MAX_SENSORS) {
            SensorInfo_t* sensor = &sensorArray[sensorCount];

            sensor->discovered = true;
            memcpy(sensor->address, address, 8);

            // Convert address to hex string
            snprintf(sensor->addressString, sizeof(sensor->addressString),
                     "%02X%02X%02X%02X%02X%02X%02X%02X",
                     address[0], address[1], address[2], address[3],
                     address[4], address[5], address[6], address[7]);

            // Set default name
            sensor_manager_get_default_name(sensorCount, sensor->name, sizeof(sensor->name));

            sensor->lastReading = -127.0f;
            sensor->lastReadTime = 0;
            sensor->errorCount = 0;

            Serial.printf("[SensorMgr] Sensor %d: %s (%s)\n",
                         sensorCount, sensor->addressString, sensor->name);

            sensorCount++;
        } else {
            Serial.println("[SensorMgr] Max sensors reached, ignoring additional devices");
            break;
        }
    }

    return sensorCount;
}

/**
 * Get number of discovered sensors
 */
int sensor_manager_get_count(void) {
    return sensorCount;
}

/**
 * Get sensor info by index
 */
const SensorInfo_t* sensor_manager_get_sensor(int index) {
    if (index < 0 || index >= sensorCount) {
        return nullptr;
    }
    return &sensorArray[index];
}

/**
 * Get sensor info by address string
 */
const SensorInfo_t* sensor_manager_get_sensor_by_address(const char* addressString) {
    if (!addressString) {
        return nullptr;
    }

    for (int i = 0; i < sensorCount; i++) {
        if (strcmp(sensorArray[i].addressString, addressString) == 0) {
            return &sensorArray[i];
        }
    }

    return nullptr;
}

/**
 * Read temperature from specific sensor
 */
bool sensor_manager_read_sensor(int index, float* temperature) {
    if (!sensors || index < 0 || index >= sensorCount || !temperature) {
        return false;
    }

    SensorInfo_t* sensor = &sensorArray[index];

    // Request temperature
    sensors->requestTemperaturesByAddress(sensor->address);

    // Read temperature
    float temp = sensors->getTempC(sensor->address);

    // Validate
    if (sensor_manager_is_valid_temp(temp)) {
        *temperature = temp;
        sensor->lastReading = temp;
        sensor->lastReadTime = millis();
        sensor->errorCount = 0;
        return true;
    } else {
        sensor->errorCount++;
        Serial.printf("[SensorMgr] Sensor %d read error (count: %d)\n",
                     index, sensor->errorCount);
        return false;
    }
}

/**
 * Read all sensors
 */
void sensor_manager_read_all(void) {
    if (!sensors || sensorCount == 0) {
        return;
    }

    // Request temperatures from all sensors at once
    sensors->requestTemperatures();

    // Read each sensor
    for (int i = 0; i < sensorCount; i++) {
        SensorInfo_t* sensor = &sensorArray[i];

        float temp = sensors->getTempC(sensor->address);

        if (sensor_manager_is_valid_temp(temp)) {
            sensor->lastReading = temp;
            sensor->lastReadTime = millis();
            sensor->errorCount = 0;
        } else {
            sensor->errorCount++;
        }
    }
}

/**
 * Set user-friendly name for sensor
 */
bool sensor_manager_set_name(int index, const char* name) {
    if (index < 0 || index >= sensorCount || !name) {
        return false;
    }

    strncpy(sensorArray[index].name, name, sizeof(sensorArray[index].name) - 1);
    sensorArray[index].name[sizeof(sensorArray[index].name) - 1] = '\0';

    Serial.printf("[SensorMgr] Sensor %d renamed to: %s\n", index, name);

    return true;
}

/**
 * Load sensor names from preferences
 */
void sensor_manager_load_names(void) {
    Preferences prefs;
    prefs.begin("sensors", true);  // Read-only

    for (int i = 0; i < sensorCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "name_%s", sensorArray[i].addressString);

        String savedName = prefs.getString(key, "");
        if (savedName.length() > 0) {
            strncpy(sensorArray[i].name, savedName.c_str(), sizeof(sensorArray[i].name) - 1);
            sensorArray[i].name[sizeof(sensorArray[i].name) - 1] = '\0';
            Serial.printf("[SensorMgr] Loaded name for sensor %d: %s\n", i, sensorArray[i].name);
        }
    }

    prefs.end();
}

/**
 * Save sensor names to preferences
 */
void sensor_manager_save_names(void) {
    Preferences prefs;
    prefs.begin("sensors", false);  // Read-write

    for (int i = 0; i < sensorCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "name_%s", sensorArray[i].addressString);
        prefs.putString(key, sensorArray[i].name);
    }

    prefs.end();
    Serial.println("[SensorMgr] Saved sensor names");
}

/**
 * Get default name for sensor index
 */
void sensor_manager_get_default_name(int index, char* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) {
        return;
    }

    snprintf(buffer, maxLen, "Temperature Sensor %d", index + 1);
}

/**
 * Validate temperature reading
 */
bool sensor_manager_is_valid_temp(float temp) {
    if (temp == DEVICE_DISCONNECTED_C) {
        return false;
    }
    if (temp < -50.0f || temp > 100.0f) {
        return false;
    }
    return true;
}
