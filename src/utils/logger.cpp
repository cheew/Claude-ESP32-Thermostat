/**
 * logger.cpp
 * System Logging Implementation
 */

#include "logger.h"
#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#define MAX_LOG_ENTRIES 20
#define MAX_LOG_LENGTH 128

// Static log storage
static char log_buffer[MAX_LOG_ENTRIES][MAX_LOG_LENGTH];
static int log_count = 0;
static int log_index = 0;
static unsigned long boot_time = 0;

void logger_init(unsigned long boot_time_ms) {
    boot_time = boot_time_ms;
    log_count = 0;
    log_index = 0;
    memset(log_buffer, 0, sizeof(log_buffer));
}

void logger_add(const char* message) {
    // Try real-time clock (NTP), fall back to uptime
    struct timeval tv;
    gettimeofday(&tv, NULL);
    char ts[16];

    if (tv.tv_sec > 1577836800) {  // After 2020-01-01 = NTP synced
        struct tm timeinfo;
        localtime_r(&tv.tv_sec, &timeinfo);
        snprintf(ts, sizeof(ts), "%02d:%02d:%02d",
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        // Fall back to uptime
        unsigned long uptime = (millis() - boot_time) / 1000;
        snprintf(ts, sizeof(ts), "%02lu:%02lu:%02lu",
                 uptime / 3600, (uptime % 3600) / 60, uptime % 60);
    }

    // Format timestamp and message
    snprintf(log_buffer[log_index], MAX_LOG_LENGTH,
             "[%s] %s", ts, message);

    // Print to serial
    Serial.println(log_buffer[log_index]);

    // Update circular buffer indices
    log_index = (log_index + 1) % MAX_LOG_ENTRIES;
    if (log_count < MAX_LOG_ENTRIES) {
        log_count++;
    }
}

const char* logger_get_entry(int index) {
    if (index < 0 || index >= log_count) {
        return nullptr;
    }

    // Calculate actual index (newest first)
    int actual_index = (log_index - 1 - index + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
    return log_buffer[actual_index];
}

int logger_get_count(void) {
    return log_count;
}

void logger_clear(void) {
    log_count = 0;
    log_index = 0;
    memset(log_buffer, 0, sizeof(log_buffer));
}
