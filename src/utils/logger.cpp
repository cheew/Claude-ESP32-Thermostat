/**
 * logger.cpp
 * System Logging Implementation
 */

#include "logger.h"
#include <Arduino.h>

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
    // Calculate uptime
    unsigned long uptime = (millis() - boot_time) / 1000;
    unsigned long hours = uptime / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    unsigned long seconds = uptime % 60;

    // Format timestamp and message
    snprintf(log_buffer[log_index], MAX_LOG_LENGTH,
             "[%02lu:%02lu:%02lu] %s",
             hours, minutes, seconds, message);

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
