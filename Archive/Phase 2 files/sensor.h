#ifndef SENSOR_H
#define SENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

/**
 * @brief Temperature sensor interface for DS18B20
 * 
 * Encapsulates the OneWire/DallasTemperature libraries and provides
 * a clean interface for reading temperature with error handling.
 */
class TemperatureSensor {
public:
    /**
     * @brief Construct a new Temperature Sensor object
     * @param pin GPIO pin for OneWire data
     */
    TemperatureSensor(int pin);
    
    /**
     * @brief Initialize the sensor
     * @return true if sensor found, false otherwise
     */
    bool begin();
    
    /**
     * @brief Read current temperature
     * @param temperature Reference to store the temperature value
     * @return true if reading successful, false if error
     */
    bool readTemperature(float& temperature);
    
    /**
     * @brief Check if sensor is connected and responding
     * @return true if connected, false otherwise
     */
    bool isConnected();
    
    /**
     * @brief Get the last valid temperature reading
     * @return float Last temperature (may be stale if sensor disconnected)
     */
    float getLastTemperature() const { return m_lastTemperature; }
    
    /**
     * @brief Get number of consecutive read errors
     * @return int Error count
     */
    int getErrorCount() const { return m_errorCount; }
    
    /**
     * @brief Reset error counter
     */
    void resetErrorCount() { m_errorCount = 0; }

private:
    OneWire m_oneWire;
    DallasTemperature m_sensor;
    float m_lastTemperature;
    int m_errorCount;
    
    /**
     * @brief Validate temperature reading
     * @param temp Temperature to validate
     * @return true if valid, false if out of range
     */
    bool isValidReading(float temp);
};

#endif // SENSOR_H
