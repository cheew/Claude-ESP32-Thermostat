/**
 * logger.h
 * System Logging Interface
 *
 * Centralized logging with timestamps and circular buffer
 */

#ifndef LOGGER_H
#define LOGGER_H

/**
 * Initialize logger with boot time
 * @param boot_time_ms Boot timestamp in milliseconds
 */
void logger_init(unsigned long boot_time_ms);

/**
 * Add entry to log
 * @param message Log message
 */
void logger_add(const char* message);

/**
 * Get log entry by index
 * @param index Entry index (0 = newest)
 * @return Log entry string, or nullptr if invalid index
 */
const char* logger_get_entry(int index);

/**
 * Get total number of log entries
 * @return Entry count
 */
int logger_get_count(void);

/**
 * Clear all log entries
 */
void logger_clear(void);

#endif // LOGGER_H
