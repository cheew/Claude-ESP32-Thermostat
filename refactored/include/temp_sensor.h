/**
 * temp_sensor.h
 * DS18B20 Temperature Sensor Interface
 *
 * Hardware abstraction for temperature sensing
 */

#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

/**
 * Initialize temperature sensor
 */
void temp_sensor_init(void);

/**
 * Read current temperature
 * @return Temperature in Celsius, or -127.0 if error
 */
float temp_sensor_read(void);

/**
 * Check if temperature reading is valid
 * @param temp Temperature value to validate
 * @return true if valid, false if error
 */
bool temp_sensor_is_valid(float temp);

#endif // TEMP_SENSOR_H
