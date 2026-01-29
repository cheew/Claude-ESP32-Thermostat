#include "hardware/sensor.h"
#include "config.h"
#include <Arduino.h>

TemperatureSensor::TemperatureSensor(int pin)
    : m_oneWire(pin)
    , m_sensor(&m_oneWire)
    , m_lastTemperature(0.0)
    , m_errorCount(0)
{
}

bool TemperatureSensor::begin() {
    m_sensor.begin();
    
    // Check if any devices found
    int deviceCount = m_sensor.getDeviceCount();
    
    if (deviceCount == 0) {
        Serial.println("[Sensor] ERROR: No DS18B20 devices found!");
        return false;
    }
    
    Serial.print("[Sensor] Found ");
    Serial.print(deviceCount);
    Serial.println(" temperature sensor(s)");
    
    // Request initial reading
    m_sensor.requestTemperatures();
    delay(100); // Wait for conversion
    
    float temp = m_sensor.getTempCByIndex(0);
    if (isValidReading(temp)) {
        m_lastTemperature = temp;
        Serial.print("[Sensor] Initial temperature: ");
        Serial.print(temp, 1);
        Serial.println("°C");
        return true;
    }
    
    Serial.println("[Sensor] WARNING: Initial reading invalid");
    return false;
}

bool TemperatureSensor::readTemperature(float& temperature) {
    // Request new reading
    m_sensor.requestTemperatures();
    
    // Read temperature from first sensor
    float temp = m_sensor.getTempCByIndex(0);
    
    // Validate reading
    if (!isValidReading(temp)) {
        m_errorCount++;
        Serial.print("[Sensor] ERROR: Invalid reading (");
        Serial.print(temp);
        Serial.print("°C), error count: ");
        Serial.println(m_errorCount);
        return false;
    }
    
    // Valid reading
    m_errorCount = 0;
    m_lastTemperature = temp;
    temperature = temp;
    
    return true;
}

bool TemperatureSensor::isConnected() {
    // Try to read temperature
    float temp;
    bool success = readTemperature(temp);
    
    // If we have too many errors, sensor is likely disconnected
    if (m_errorCount > 5) {
        return false;
    }
    
    return success;
}

bool TemperatureSensor::isValidReading(float temp) {
    // Check for sensor error code
    if (temp == DEVICE_DISCONNECTED_C) {
        return false;
    }
    
    // Check if reading is within valid range
    if (temp < TEMP_MIN_VALID || temp > TEMP_MAX_VALID) {
        return false;
    }
    
    return true;
}
