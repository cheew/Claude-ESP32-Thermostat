/**
 * weather_client.cpp
 * OpenWeatherMap Weather Integration
 *
 * Fetches current weather via HTTP from OpenWeatherMap API.
 * Caches data in LittleFS for persistence across reboots.
 * Optionally adjusts habitat temperatures based on outdoor conditions.
 */

#include "weather_client.h"
#include "console.h"
#include "profile_manager.h"
#include "output_manager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

// Configuration
static char weatherApiKey[48] = "";
static char weatherCity[64] = "";
static bool weatherEnabled = false;
static uint8_t weatherIntervalHours = 3;  // Default: every 3 hours

// Cached data
static WeatherData_t currentWeather;

// Weather history buffer - stores fetches with remote city's local time
#define WEATHER_HISTORY_SIZE 8

typedef struct {
    float tempC;
    uint8_t remoteHour;    // Remote city's local hour (0-23) at fetch time
    uint8_t remoteMinute;  // Remote city's local minute (0-59)
    bool valid;
} WeatherHistEntry_t;

static WeatherHistEntry_t weatherHistory[WEATHER_HISTORY_SIZE];
static int historyIndex = 0;
static int historyCount = 0;

// Timing
static unsigned long lastFetchTime = 0;
static bool fetchPending = false;

// Cache file paths
static const char* CACHE_FILE = "/cache/weather.json";
static const char* HISTORY_FILE = "/cache/weather_hist.json";

// OpenWeatherMap API base URLs
static const char* OWM_API_BASE = "http://api.openweathermap.org/data/2.5/weather";
static const char* OWM_FORECAST_BASE = "http://api.openweathermap.org/data/2.5/forecast";

/**
 * Load cached weather data from LittleFS
 */
static void loadCache(void) {
    if (!LittleFS.exists(CACHE_FILE)) return;

    File f = LittleFS.open(CACHE_FILE, "r");
    if (!f) return;

    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Weather] Cache parse error: %s\n", err.c_str());
        return;
    }

    currentWeather.tempC = doc["tempC"] | 0.0f;
    currentWeather.humidity = doc["humidity"] | 0.0f;
    currentWeather.feelsLikeC = doc["feelsLikeC"] | 0.0f;
    currentWeather.clouds = doc["clouds"] | 0;
    currentWeather.tempMinC = doc["tempMinC"] | 0.0f;
    currentWeather.tempMaxC = doc["tempMaxC"] | 0.0f;
    currentWeather.fetchTime = doc["fetchTime"] | 0UL;
    currentWeather.remoteTzOffset = doc["remoteTz"] | 0;
    currentWeather.tzValid = doc.containsKey("remoteTz");
    currentWeather.valid = true;

    const char* desc = doc["description"] | "";
    strncpy(currentWeather.description, desc, sizeof(currentWeather.description) - 1);
    currentWeather.description[sizeof(currentWeather.description) - 1] = '\0';

    Serial.printf("[Weather] Loaded cache: %.1f°C, %s\n",
                  currentWeather.tempC, currentWeather.description);
}

/**
 * Load weather history from LittleFS
 */
static void loadHistory(void) {
    if (!LittleFS.exists(HISTORY_FILE)) return;

    File f = LittleFS.open(HISTORY_FILE, "r");
    if (!f) return;

    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) return;

    JsonArray arr = doc["entries"];
    historyCount = 0;
    historyIndex = 0;
    for (int i = 0; i < (int)arr.size() && i < WEATHER_HISTORY_SIZE; i++) {
        weatherHistory[i].tempC = arr[i]["t"] | 0.0f;
        weatherHistory[i].remoteHour = arr[i]["rh"] | 0;
        weatherHistory[i].remoteMinute = arr[i]["rm"] | 0;
        weatherHistory[i].valid = true;
        historyCount++;
        historyIndex = (i + 1) % WEATHER_HISTORY_SIZE;
    }
    Serial.printf("[Weather] Loaded %d history entries\n", historyCount);
}

/**
 * Save weather history to LittleFS
 */
static void saveHistory(void) {
    if (!LittleFS.exists("/cache")) {
        LittleFS.mkdir("/cache");
    }

    File f = LittleFS.open(HISTORY_FILE, "w");
    if (!f) return;

    DynamicJsonDocument doc(512);
    JsonArray arr = doc.createNestedArray("entries");

    // Write entries in order (oldest first)
    int start = (historyCount < WEATHER_HISTORY_SIZE) ? 0 :
                historyIndex;  // oldest entry position when buffer is full
    for (int i = 0; i < historyCount; i++) {
        int idx = (start + i) % WEATHER_HISTORY_SIZE;
        if (!weatherHistory[idx].valid) continue;
        JsonObject entry = arr.createNestedObject();
        entry["t"] = serialized(String(weatherHistory[idx].tempC, 1));
        entry["rh"] = weatherHistory[idx].remoteHour;
        entry["rm"] = weatherHistory[idx].remoteMinute;
    }

    serializeJson(doc, f);
    f.close();
}

/**
 * Save weather data to LittleFS cache
 */
static void saveCache(void) {
    // Ensure cache directory exists
    if (!LittleFS.exists("/cache")) {
        LittleFS.mkdir("/cache");
    }

    File f = LittleFS.open(CACHE_FILE, "w");
    if (!f) {
        Serial.println("[Weather] Failed to write cache");
        return;
    }

    DynamicJsonDocument doc(512);
    doc["tempC"] = currentWeather.tempC;
    doc["humidity"] = currentWeather.humidity;
    doc["feelsLikeC"] = currentWeather.feelsLikeC;
    doc["description"] = currentWeather.description;
    doc["clouds"] = currentWeather.clouds;
    doc["tempMinC"] = currentWeather.tempMinC;
    doc["tempMaxC"] = currentWeather.tempMaxC;
    doc["fetchTime"] = currentWeather.fetchTime;
    if (currentWeather.tzValid) {
        doc["remoteTz"] = currentWeather.remoteTzOffset;
    }

    serializeJson(doc, f);
    f.close();
}

/**
 * Load configuration from Preferences
 */
static void loadConfig(void) {
    Preferences prefs;
    prefs.begin("thermostat", true);

    weatherEnabled = prefs.getBool("weather_on", false);
    weatherIntervalHours = prefs.getUChar("weather_hrs", 3);

    String key = prefs.getString("weather_key", "");
    strncpy(weatherApiKey, key.c_str(), sizeof(weatherApiKey) - 1);
    weatherApiKey[sizeof(weatherApiKey) - 1] = '\0';

    String city = prefs.getString("weather_city", "");
    strncpy(weatherCity, city.c_str(), sizeof(weatherCity) - 1);
    weatherCity[sizeof(weatherCity) - 1] = '\0';

    prefs.end();

    // Clamp interval
    if (weatherIntervalHours < 1) weatherIntervalHours = 1;
    if (weatherIntervalHours > 6) weatherIntervalHours = 6;
}

/**
 * Fetch weather data from OpenWeatherMap
 * @return true if successful
 */
static bool fetchWeather(void) {
    if (strlen(weatherApiKey) == 0 || strlen(weatherCity) == 0) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Weather] No WiFi, skipping fetch");
        return false;
    }

    // Build URL
    String url = String(OWM_API_BASE) + "?q=" + String(weatherCity)
               + "&appid=" + String(weatherApiKey)
               + "&units=metric";

    Serial.printf("[Weather] Fetching: %s\n", weatherCity);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000);  // 10 second timeout

    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.printf("[Weather] HTTP error: %d\n", httpCode);
        if (httpCode > 0) {
            String body = http.getString();
            Serial.printf("[Weather] Response: %s\n", body.c_str());
        }
        http.end();
        console_add_event_f(CONSOLE_EVENT_SYSTEM, "Weather fetch failed: HTTP %d", httpCode);
        return false;
    }

    String payload = http.getString();
    http.end();

    // Parse JSON response
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
        return false;
    }

    // Extract data from OWM response
    // https://openweathermap.org/current
    JsonObject main = doc["main"];
    currentWeather.tempC = main["temp"] | 0.0f;
    currentWeather.feelsLikeC = main["feels_like"] | 0.0f;
    currentWeather.humidity = main["humidity"] | 0.0f;
    currentWeather.tempMinC = main["temp_min"] | 0.0f;
    currentWeather.tempMaxC = main["temp_max"] | 0.0f;

    // Cloud cover
    currentWeather.clouds = doc["clouds"]["all"] | 0;

    // Timezone offset (seconds from UTC) for time-alignment checks
    if (doc.containsKey("timezone")) {
        currentWeather.remoteTzOffset = doc["timezone"] | 0;
        currentWeather.tzValid = true;
    }

    // Weather description (first entry)
    JsonArray weather = doc["weather"];
    if (weather.size() > 0) {
        const char* desc = weather[0]["description"] | "";
        strncpy(currentWeather.description, desc, sizeof(currentWeather.description) - 1);
        currentWeather.description[sizeof(currentWeather.description) - 1] = '\0';
    }

    currentWeather.fetchTime = millis();
    currentWeather.valid = true;

    // Save to cache
    saveCache();

    Serial.printf("[Weather] OK: %.1f°C (feels %.1f°C), %s, %d%% clouds\n",
                  currentWeather.tempC, currentWeather.feelsLikeC,
                  currentWeather.description, currentWeather.clouds);

    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Weather: %.1f°C, %s",
                        currentWeather.tempC, currentWeather.description);

    return true;
}

/**
 * Fetch 24-hour forecast from OpenWeatherMap (3-hour intervals, 8 entries).
 * Populates the weather history buffer with the remote city's local time-of-day
 * and forecasted temperature for each entry.
 * @return true if successful
 */
static bool fetchForecast(void) {
    if (strlen(weatherApiKey) == 0 || strlen(weatherCity) == 0) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    // Build URL - cnt=8 gives 24 hours of 3-hour intervals
    String url = String(OWM_FORECAST_BASE) + "?q=" + String(weatherCity)
               + "&appid=" + String(weatherApiKey)
               + "&units=metric&cnt=8";

    Serial.println("[Weather] Fetching 24h forecast...");

    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000);  // 15 second timeout (response is larger)

    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.printf("[Weather] Forecast HTTP error: %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    // Use filter to minimize memory - only parse fields we need
    StaticJsonDocument<128> filter;
    filter["city"]["timezone"] = true;
    JsonObject listFilter = filter["list"].createNestedObject();
    listFilter["dt"] = true;
    listFilter["main"]["temp"] = true;

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, payload,
                                               DeserializationOption::Filter(filter));

    if (err) {
        Serial.printf("[Weather] Forecast parse error: %s\n", err.c_str());
        return false;
    }

    int32_t tzOffset = doc["city"]["timezone"] | 0;
    JsonArray list = doc["list"];

    if (list.size() == 0) {
        Serial.println("[Weather] Forecast: empty list");
        return false;
    }

    // Clear and repopulate history with forecast data
    historyCount = 0;
    historyIndex = 0;
    memset(weatherHistory, 0, sizeof(weatherHistory));

    for (int i = 0; i < (int)list.size() && i < WEATHER_HISTORY_SIZE; i++) {
        time_t dt = list[i]["dt"] | (time_t)0;
        float temp = list[i]["main"]["temp"] | 0.0f;

        if (dt == 0) continue;

        // Convert UTC timestamp to remote city's local time
        time_t remoteTime = dt + tzOffset;
        struct tm remoteTm;
        gmtime_r(&remoteTime, &remoteTm);

        weatherHistory[historyIndex].tempC = temp;
        weatherHistory[historyIndex].remoteHour = remoteTm.tm_hour;
        weatherHistory[historyIndex].remoteMinute = remoteTm.tm_min;
        weatherHistory[historyIndex].valid = true;
        historyIndex = (historyIndex + 1) % WEATHER_HISTORY_SIZE;
        historyCount++;
    }

    // Update timezone info on current weather struct too
    currentWeather.remoteTzOffset = tzOffset;
    currentWeather.tzValid = true;

    saveHistory();

    Serial.printf("[Weather] Forecast: %d entries covering 24h\n", historyCount);
    for (int i = 0; i < historyCount; i++) {
        Serial.printf("  [%02d:%02d] %.1f°C\n",
                      weatherHistory[i].remoteHour,
                      weatherHistory[i].remoteMinute,
                      weatherHistory[i].tempC);
    }

    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Weather forecast: %d entries loaded", historyCount);

    return true;
}

// ===== Public API =====

void weather_client_init(void) {
    memset(&currentWeather, 0, sizeof(currentWeather));
    memset(weatherHistory, 0, sizeof(weatherHistory));

    loadConfig();
    loadCache();
    loadHistory();

    if (weatherEnabled && strlen(weatherApiKey) > 0) {
        Serial.printf("[Weather] Enabled - city: %s, interval: %dh\n",
                      weatherCity, weatherIntervalHours);
        // Trigger first fetch after 30 seconds (let WiFi stabilize)
        lastFetchTime = millis() - ((unsigned long)weatherIntervalHours * 3600000UL) + 30000UL;
    } else {
        Serial.println("[Weather] Disabled or not configured");
    }
}

void weather_client_task(void) {
    if (!weatherEnabled || strlen(weatherApiKey) == 0) return;

    unsigned long intervalMs = (unsigned long)weatherIntervalHours * 3600000UL;
    unsigned long now = millis();

    if (now - lastFetchTime >= intervalMs || fetchPending) {
        fetchPending = false;
        lastFetchTime = now;
        if (fetchWeather()) {
            // Also fetch 24h forecast to populate history buffer
            fetchForecast();
        }
    }
}

const WeatherData_t* weather_client_get_data(void) {
    return &currentWeather;
}

bool weather_client_is_available(void) {
    if (!currentWeather.valid) return false;

    // Consider data stale after 2x the fetch interval
    unsigned long maxAge = (unsigned long)weatherIntervalHours * 7200000UL;
    if (millis() - currentWeather.fetchTime > maxAge) {
        return false;
    }

    return true;
}

bool weather_client_is_enabled(void) {
    return weatherEnabled && strlen(weatherApiKey) > 0 && strlen(weatherCity) > 0;
}

void weather_client_save_config(const char* apiKey, const char* city,
                                 bool enabled, uint8_t intervalHours) {
    // Clamp interval
    if (intervalHours < 1) intervalHours = 1;
    if (intervalHours > 6) intervalHours = 6;

    Preferences prefs;
    prefs.begin("thermostat", false);
    prefs.putString("weather_key", apiKey);
    prefs.putString("weather_city", city);
    prefs.putBool("weather_on", enabled);
    prefs.putUChar("weather_hrs", intervalHours);
    prefs.end();

    // Update runtime config
    strncpy(weatherApiKey, apiKey, sizeof(weatherApiKey) - 1);
    weatherApiKey[sizeof(weatherApiKey) - 1] = '\0';
    strncpy(weatherCity, city, sizeof(weatherCity) - 1);
    weatherCity[sizeof(weatherCity) - 1] = '\0';
    weatherEnabled = enabled;
    weatherIntervalHours = intervalHours;

    Serial.printf("[Weather] Config saved - enabled: %d, city: %s, interval: %dh\n",
                  enabled, city, intervalHours);

    // Trigger fetch on next task() call if newly enabled
    if (enabled && strlen(apiKey) > 0 && strlen(city) > 0) {
        fetchPending = true;
    }
}

bool weather_client_force_fetch(void) {
    if (!weatherEnabled || strlen(weatherApiKey) == 0) return false;
    bool ok = fetchWeather();
    if (ok) fetchForecast();
    return ok;
}

/**
 * Helper: get the ESP32's current local time-of-day in minutes since midnight.
 * Returns -1 if NTP is not synced.
 */
static int getLocalMinutes(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tv.tv_sec < 1577836800) return -1;
    struct tm localTm;
    localtime_r(&tv.tv_sec, &localTm);
    return localTm.tm_hour * 60 + localTm.tm_min;
}

/**
 * Helper: find the weather history entry whose remote time-of-day
 * best matches the given target time (in minutes since midnight).
 * Returns pointer to best entry, or nullptr if no history.
 */
static const WeatherHistEntry_t* findBestHistoryMatch(int targetMinutes) {
    if (historyCount == 0 || targetMinutes < 0) return nullptr;

    const WeatherHistEntry_t* best = nullptr;
    int bestDiff = 9999;

    for (int i = 0; i < historyCount && i < WEATHER_HISTORY_SIZE; i++) {
        if (!weatherHistory[i].valid) continue;
        int entryMin = weatherHistory[i].remoteHour * 60 + weatherHistory[i].remoteMinute;
        int diff = abs(entryMin - targetMinutes);
        if (diff > 720) diff = 1440 - diff;  // Wrap around midnight
        if (diff < bestDiff) {
            bestDiff = diff;
            best = &weatherHistory[i];
        }
    }

    return best;
}

float weather_client_adjust_target(float profileMin, float profileMax, float baseTarget) {
    // Work from cached forecast history even if current weather is stale (offline mode).
    // This allows the system to keep adjusting using stored 24h forecast data.
    if (historyCount == 0 && !weather_client_is_available()) {
        return baseTarget;
    }

    // Get the outdoor temp to use for adjustment.
    // Strategy: find the forecast entry whose remote city local time-of-day
    // best matches our current local time. E.g. if it's 13:00 here,
    // use the forecast from when it was ~13:00 at the remote city.
    float outdoorTemp = currentWeather.valid ? currentWeather.tempC : 20.0f;

    int localMin = getLocalMinutes();
    if (localMin >= 0 && historyCount > 0) {
        const WeatherHistEntry_t* match = findBestHistoryMatch(localMin);
        if (match) {
            outdoorTemp = match->tempC;
        }
    }

    // Weather influence:
    // ±1.5°C max adjustment proportional to deviation from 20°C neutral
    float neutral = 20.0f;
    float deviation = outdoorTemp - neutral;
    float adjustment = (deviation / 15.0f) * 1.5f;
    if (adjustment > 1.5f) adjustment = 1.5f;
    if (adjustment < -1.5f) adjustment = -1.5f;

    float adjusted = baseTarget + adjustment;

    // Clamp to profile bounds
    if (adjusted < profileMin) adjusted = profileMin;
    if (adjusted > profileMax) adjusted = profileMax;

    return adjusted;
}

bool weather_client_is_time_aligned(void) {
    if (!currentWeather.valid || !currentWeather.tzValid) {
        return true;  // No timezone data = assume aligned
    }

    // If we have history, check if any entry's remote time is close
    // to our current local time (within 3 hours)
    int localMin = getLocalMinutes();
    if (localMin < 0) return true;  // NTP not synced, assume aligned

    if (historyCount > 0) {
        const WeatherHistEntry_t* match = findBestHistoryMatch(localMin);
        if (match) {
            int entryMin = match->remoteHour * 60 + match->remoteMinute;
            int diff = abs(entryMin - localMin);
            if (diff > 720) diff = 1440 - diff;
            return diff <= 180;  // Have a match within 3 hours
        }
    }

    // No history: fall back to checking if remote city's CURRENT time matches
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t remoteTime = tv.tv_sec + currentWeather.remoteTzOffset;
    struct tm remoteTm;
    gmtime_r(&remoteTime, &remoteTm);
    int remoteMin = remoteTm.tm_hour * 60 + remoteTm.tm_min;

    int diff = abs(localMin - remoteMin);
    if (diff > 720) diff = 1440 - diff;
    return diff <= 180;
}

int weather_client_get_history_count(void) {
    return historyCount;
}

bool weather_client_get_history_entry(int index, uint8_t* hour, uint8_t* minute, float* tempC) {
    if (index < 0 || index >= historyCount || index >= WEATHER_HISTORY_SIZE) return false;
    if (!weatherHistory[index].valid) return false;
    if (hour) *hour = weatherHistory[index].remoteHour;
    if (minute) *minute = weatherHistory[index].remoteMinute;
    if (tempC) *tempC = weatherHistory[index].tempC;
    return true;
}

bool weather_client_is_syncing_output(int outputIndex) {
    // Active if we have forecast history (even offline) or fresh current data
    if (historyCount == 0 && !weather_client_is_available()) return false;

    const char* activeId = profile_manager_get_active_id();
    if (!activeId || strlen(activeId) == 0) return false;

    OutputConfig_t* output = output_manager_get_output(outputIndex);
    if (!output || !output->enabled) return false;
    if (output->controlMode != CONTROL_MODE_SCHEDULE &&
        output->controlMode != CONTROL_MODE_WEATHER) return false;

    return true;
}
