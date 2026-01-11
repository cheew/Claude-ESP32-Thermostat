/**
 * console.cpp
 * Live Console/Debug Event Buffer Implementation
 */

#include "console.h"
#include <stdarg.h>

#define MAX_CONSOLE_EVENTS 50
#define MAX_EVENT_LENGTH 128

// Event structure
typedef struct {
    ConsoleEventType_t type;
    char message[MAX_EVENT_LENGTH];
    unsigned long timestamp;
} ConsoleEvent_t;

// Console buffer (circular)
static ConsoleEvent_t event_buffer[MAX_CONSOLE_EVENTS];
static int event_count = 0;
static int event_index = 0;
static unsigned long boot_time = 0;

void console_init(void) {
    boot_time = millis();
    event_count = 0;
    event_index = 0;
    memset(event_buffer, 0, sizeof(event_buffer));
}

void console_add_event(ConsoleEventType_t type, const char* message) {
    // Calculate uptime
    unsigned long uptime = (millis() - boot_time) / 1000;
    unsigned long hours = uptime / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    unsigned long seconds = uptime % 60;

    // Format timestamp and message
    snprintf(event_buffer[event_index].message, MAX_EVENT_LENGTH,
             "[%02lu:%02lu:%02lu] %s",
             hours, minutes, seconds, message);

    event_buffer[event_index].type = type;
    event_buffer[event_index].timestamp = millis();

    // Print to serial for traditional debugging
    Serial.println(event_buffer[event_index].message);

    // Update circular buffer indices
    event_index = (event_index + 1) % MAX_CONSOLE_EVENTS;
    if (event_count < MAX_CONSOLE_EVENTS) {
        event_count++;
    }
}

void console_add_event_f(ConsoleEventType_t type, const char* format, ...) {
    char buffer[MAX_EVENT_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    console_add_event(type, buffer);
}

int console_get_count(void) {
    return event_count;
}

const char* console_get_event(int index) {
    if (index < 0 || index >= event_count) {
        return nullptr;
    }

    // Calculate actual buffer index (newest first)
    int actual_index;
    if (event_count < MAX_CONSOLE_EVENTS) {
        // Buffer not full yet, start from beginning
        actual_index = event_count - 1 - index;
    } else {
        // Buffer full, calculate from current position
        actual_index = (event_index - 1 - index + MAX_CONSOLE_EVENTS) % MAX_CONSOLE_EVENTS;
    }

    return event_buffer[actual_index].message;
}

ConsoleEventType_t console_get_event_type(int index) {
    if (index < 0 || index >= event_count) {
        return CONSOLE_EVENT_DEBUG;
    }

    // Calculate actual buffer index (newest first)
    int actual_index;
    if (event_count < MAX_CONSOLE_EVENTS) {
        actual_index = event_count - 1 - index;
    } else {
        actual_index = (event_index - 1 - index + MAX_CONSOLE_EVENTS) % MAX_CONSOLE_EVENTS;
    }

    return event_buffer[actual_index].type;
}

void console_clear(void) {
    event_count = 0;
    event_index = 0;
    memset(event_buffer, 0, sizeof(event_buffer));
}

const char* console_get_type_name(ConsoleEventType_t type) {
    switch (type) {
        case CONSOLE_EVENT_SYSTEM:   return "SYSTEM";
        case CONSOLE_EVENT_MQTT:     return "MQTT";
        case CONSOLE_EVENT_WIFI:     return "WIFI";
        case CONSOLE_EVENT_TEMP:     return "TEMP";
        case CONSOLE_EVENT_PID:      return "PID";
        case CONSOLE_EVENT_SCHEDULE: return "SCHEDULE";
        case CONSOLE_EVENT_ERROR:    return "ERROR";
        case CONSOLE_EVENT_DEBUG:    return "DEBUG";
        default:                     return "UNKNOWN";
    }
}
