/**
 * temp_history.h
 * Temperature History Tracking
 *
 * Records temperature readings over time with configurable
 * sample interval and buffer size for visualization
 */

#ifndef TEMP_HISTORY_H
#define TEMP_HISTORY_H

#include <Arduino.h>

// Configuration
#define HISTORY_BUFFER_SIZE 288  // 24 hours at 5-minute intervals
#define HISTORY_SAMPLE_INTERVAL 300000  // 5 minutes in milliseconds

/**
 * Temperature history data point
 */
typedef struct {
    unsigned long timestamp;  // Unix timestamp
    float temperature;        // Temperature in Celsius
} TempHistoryPoint_t;

/**
 * Initialize temperature history module
 * @param boot_time_ms Boot timestamp for relative time calculation
 */
void temp_history_init(unsigned long boot_time_ms);

/**
 * Record a temperature reading
 * Call this regularly - module handles sampling interval internally
 * @param temp Current temperature
 */
void temp_history_record(float temp);

/**
 * Get number of stored history points
 * @return Count of stored readings
 */
int temp_history_get_count(void);

/**
 * Get history point by index
 * @param index Index (0 = oldest, count-1 = newest)
 * @return Pointer to history point, or nullptr if invalid index
 */
const TempHistoryPoint_t* temp_history_get_point(int index);

/**
 * Clear all history data
 */
void temp_history_clear(void);

/**
 * Get last sample time (for debugging)
 * @return Milliseconds since last sample
 */
unsigned long temp_history_get_last_sample_time(void);

#endif // TEMP_HISTORY_H
