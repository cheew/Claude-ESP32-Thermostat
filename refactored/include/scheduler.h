/**
 * scheduler.h
 * Temperature Scheduling System
 * 
 * Manages time-based temperature changes:
 * - Up to 8 time slots per day
 * - Day-specific scheduling (different temps for different days)
 * - Automatic target temperature updates based on time
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Preferences.h>

#define MAX_SCHEDULE_SLOTS 8

// Schedule slot structure
typedef struct {
    bool enabled;          // Slot active/inactive
    int hour;             // Hour (0-23)
    int minute;           // Minute (0-59)
    float targetTemp;     // Target temperature for this time
    char days[8];         // Active days "SMTWTFS" (Sunday-Saturday)
} ScheduleSlot_t;

/**
 * Initialize scheduler
 * Loads schedule from preferences
 */
void scheduler_init(void);

/**
 * Scheduler task - call from main loop
 * Checks time and updates target temperature if needed
 */
void scheduler_task(void);

/**
 * Enable/disable scheduler
 * @param enabled true to enable, false to disable
 */
void scheduler_set_enabled(bool enabled);

/**
 * Check if scheduler is enabled
 * @return true if enabled
 */
bool scheduler_is_enabled(void);

/**
 * Get number of active schedule slots
 * @return Number of slots in use (0-8)
 */
int scheduler_get_slot_count(void);

/**
 * Set number of active slots
 * @param count Number of slots (0-8)
 */
void scheduler_set_slot_count(int count);

/**
 * Get a schedule slot
 * @param index Slot index (0-7)
 * @param slot Output structure for slot data
 * @return true if successful
 */
bool scheduler_get_slot(int index, ScheduleSlot_t* slot);

/**
 * Set a schedule slot
 * @param index Slot index (0-7)
 * @param slot Slot data to save
 * @return true if successful
 */
bool scheduler_set_slot(int index, const ScheduleSlot_t* slot);

/**
 * Save entire schedule to preferences
 */
void scheduler_save(void);

/**
 * Load schedule from preferences
 */
void scheduler_load(void);

/**
 * Get next scheduled change
 * @param nextTime Output buffer for time string (e.g., "14:30")
 * @param nextTemp Output for next temperature
 * @return true if there is a next schedule today
 */
bool scheduler_get_next(char* nextTime, float* nextTemp);

/**
 * Clear all schedule slots
 */
void scheduler_clear_all(void);

/**
 * Get pointer to all slots (for web interface)
 * @return Pointer to schedule slot array
 */
ScheduleSlot_t* scheduler_get_slots(void);

#endif // SCHEDULER_H
