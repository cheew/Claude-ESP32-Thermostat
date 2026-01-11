/**
 * temp_sensor.cpp
 * DS18B20 Temperature Sensor Implementation
 */

#include "temp_sensor.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// Pin configuration
#define ONE_WIRE_BUS 4    // GPIO4 (D4)

// Hardware objects
static OneWire oneWire(ONE_WIRE_BUS);
static DallasTemperature sensors(&oneWire);

void temp_sensor_init(void) {
  sensors.begin();
}

float temp_sensor_read(void) {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  return temp;
}

bool temp_sensor_is_valid(float temp) {
  // Check for sensor error values and reasonable range
  if (temp == DEVICE_DISCONNECTED_C) {
    return false;
  }
  if (temp < -50.0 || temp > 100.0) {
    return false;
  }
  return true;
}
