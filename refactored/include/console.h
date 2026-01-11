/**
 * console.h
 * Live Console/Debug Event Buffer
 *
 * Captures system events, MQTT activity, and debug messages
 * for real-time streaming to web console
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <Arduino.h>

// Event types for filtering
typedef enum {
    CONSOLE_EVENT_SYSTEM,    // System events (boot, state changes)
    CONSOLE_EVENT_MQTT,      // MQTT activity (publish/receive)
    CONSOLE_EVENT_WIFI,      // WiFi events (connect/disconnect)
    CONSOLE_EVENT_TEMP,      // Temperature readings and changes
    CONSOLE_EVENT_PID,       // PID calculations
    CONSOLE_EVENT_SCHEDULE,  // Scheduler activations
    CONSOLE_EVENT_ERROR,     // Errors and warnings
    CONSOLE_EVENT_DEBUG      // General debug messages
} ConsoleEventType_t;

/**
 * Initialize console event buffer
 */
void console_init(void);

/**
 * Add event to console buffer
 * @param type Event type for filtering
 * @param message Event message
 */
void console_add_event(ConsoleEventType_t type, const char* message);

/**
 * Add formatted event to console buffer
 * @param type Event type for filtering
 * @param format Printf-style format string
 * @param ... Arguments for format string
 */
void console_add_event_f(ConsoleEventType_t type, const char* format, ...);

/**
 * Get total number of events in buffer
 * @return Number of events
 */
int console_get_count(void);

/**
 * Get event at index (newest first)
 * @param index Event index (0 = newest)
 * @return Event string or nullptr if invalid index
 */
const char* console_get_event(int index);

/**
 * Get event type at index
 * @param index Event index (0 = newest)
 * @return Event type
 */
ConsoleEventType_t console_get_event_type(int index);

/**
 * Clear all events from buffer
 */
void console_clear(void);

/**
 * Get event type name
 * @param type Event type
 * @return Type name string
 */
const char* console_get_type_name(ConsoleEventType_t type);

#endif // CONSOLE_H
