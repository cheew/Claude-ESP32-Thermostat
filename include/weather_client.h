/**
 * weather_client.h
 * OpenWeatherMap Weather Integration
 *
 * Fetches current weather data and caches it in LittleFS.
 * Used to optionally adjust habitat temperatures based on
 * outdoor conditions within animal profile bounds.
 */

#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include <Arduino.h>

/**
 * Current weather data
 */
typedef struct {
    float tempC;              // Current outdoor temperature
    float humidity;           // Current humidity %
    float feelsLikeC;         // Feels-like temperature
    char description[32];     // "clear sky", "light rain"
    uint16_t clouds;          // Cloud cover %
    float tempMinC;           // Daily low
    float tempMaxC;           // Daily high
    unsigned long fetchTime;  // millis() when this data was fetched
    bool valid;               // Data is current and valid
    int32_t remoteTzOffset;   // Remote city UTC offset in seconds (from OWM)
    bool tzValid;             // Timezone data available
} WeatherData_t;

/**
 * Initialize weather client
 * Loads config from Preferences and cached data from LittleFS
 */
void weather_client_init(void);

/**
 * Weather client task - call from main loop
 * Fetches weather data at configured interval (1-6 hours)
 */
void weather_client_task(void);

/**
 * Get latest weather data
 * @return Pointer to cached weather data (do not free)
 */
const WeatherData_t* weather_client_get_data(void);

/**
 * Check if valid weather data is available
 * @return true if data exists and is not too old
 */
bool weather_client_is_available(void);

/**
 * Check if weather client is enabled
 * @return true if API key and city are configured
 */
bool weather_client_is_enabled(void);

/**
 * Save weather configuration to Preferences
 * @param apiKey OpenWeatherMap API key
 * @param city City name or "lat,lon" coordinates
 * @param enabled Enable/disable weather fetching
 * @param intervalHours Fetch interval in hours (1-6)
 */
void weather_client_save_config(const char* apiKey, const char* city,
                                 bool enabled, uint8_t intervalHours);

/**
 * Force an immediate weather fetch
 * @return true if fetch was triggered
 */
bool weather_client_force_fetch(void);

/**
 * Calculate weather-adjusted target temperature
 * Shifts target within profile min/max bounds based on outdoor temp.
 * @param profileMin Minimum target from animal profile
 * @param profileMax Maximum target from animal profile
 * @param baseTarget The default/schedule target temperature
 * @return Adjusted target temperature within bounds
 */
float weather_client_adjust_target(float profileMin, float profileMax, float baseTarget);

/**
 * Check if weather history contains a time-of-day match.
 * Returns true if we have a history entry from when the remote city's
 * local time was within 3 hours of the ESP32's current local time.
 * The adjust_target function always uses the best available match
 * regardless of this check.
 */
bool weather_client_is_time_aligned(void);

/**
 * Check if weather sync is actively adjusting a given output.
 * Requires: weather available, active profile, schedule mode.
 * @param outputIndex 0-based output index
 * @return true if weather is actively influencing this output's target
 */
bool weather_client_is_syncing_output(int outputIndex);

/**
 * Get forecast history entry count
 * @return Number of valid forecast entries (0-8)
 */
int weather_client_get_history_count(void);

/**
 * Get forecast history entry by index
 * @param index 0-based index (0 = first entry)
 * @param hour Output: remote city local hour (0-23)
 * @param minute Output: remote city local minute (0-59)
 * @param tempC Output: forecasted temperature
 * @return true if entry is valid
 */
bool weather_client_get_history_entry(int index, uint8_t* hour, uint8_t* minute, float* tempC);

#endif // WEATHER_CLIENT_H
