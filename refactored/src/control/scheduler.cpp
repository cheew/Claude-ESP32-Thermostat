/**
 * scheduler.cpp
 * Temperature Scheduling System Implementation
 */

#include "scheduler.h"
#include "system_state.h"
#include <Arduino.h>
#include <time.h>

// Schedule storage
static ScheduleSlot_t scheduleSlots[MAX_SCHEDULE_SLOTS];
static bool scheduleEnabled = false;
static int scheduleSlotCount = 0;

// Last check time (to avoid duplicate triggers)
static int lastCheckHour = -1;
static int lastCheckMinute = -1;

// Preferences namespace
static const char* PREFS_NAMESPACE = "thermostat";

// Default schedule (2 slots: day and night)
static void initializeDefaultSchedule(void);

/**
 * Initialize scheduler
 */
void scheduler_init(void) {
    Serial.println("[Scheduler] Initializing scheduler");
    
    // Load schedule from preferences
    scheduler_load();
    
    // If no schedule exists, create default
    if (scheduleSlotCount == 0) {
        initializeDefaultSchedule();
        scheduler_save();
    }
    
    Serial.printf("[Scheduler] Loaded %d slots, %s\n", 
                  scheduleSlotCount, scheduleEnabled ? "enabled" : "disabled");
}

/**
 * Scheduler task
 */
void scheduler_task(void) {
    if (!scheduleEnabled) return;
    
    // Get current time
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    // Check if time is valid (NTP synced)
    if (timeinfo->tm_year < 100) {
        // Time not synced yet
        return;
    }
    
    int currentHour = timeinfo->tm_hour;
    int currentMinute = timeinfo->tm_min;
    int currentDay = timeinfo->tm_wday;  // 0=Sunday
    
    // Only check once per minute
    if (currentHour == lastCheckHour && currentMinute == lastCheckMinute) {
        return;
    }
    
    lastCheckHour = currentHour;
    lastCheckMinute = currentMinute;
    
    // Check each schedule slot
    String dayChar = "SMTWTFS";
    char todayChar = dayChar[currentDay];
    
    for (int i = 0; i < scheduleSlotCount; i++) {
        if (!scheduleSlots[i].enabled) continue;
        
        // Check if today is in the schedule
        if (strchr(scheduleSlots[i].days, todayChar) == NULL) continue;
        
        // Check if this is the scheduled time
        if (scheduleSlots[i].hour == currentHour && 
            scheduleSlots[i].minute == currentMinute) {
            
            Serial.printf("[Scheduler] Slot %d triggered: %.1f°C\n", 
                          i, scheduleSlots[i].targetTemp);
            
            // Update target temperature
            state_set_target_temp(scheduleSlots[i].targetTemp, true);
            
            // Only trigger first matching slot
            break;
        }
    }
}

/**
 * Enable/disable scheduler
 */
void scheduler_set_enabled(bool enabled) {
    scheduleEnabled = enabled;
    
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putBool("sched_enabled", scheduleEnabled);
    prefs.end();
    
    Serial.printf("[Scheduler] %s\n", enabled ? "Enabled" : "Disabled");
}

/**
 * Check if enabled
 */
bool scheduler_is_enabled(void) {
    return scheduleEnabled;
}

/**
 * Get slot count
 */
int scheduler_get_slot_count(void) {
    return scheduleSlotCount;
}

/**
 * Set slot count
 */
void scheduler_set_slot_count(int count) {
    scheduleSlotCount = constrain(count, 0, MAX_SCHEDULE_SLOTS);
}

/**
 * Get a schedule slot
 */
bool scheduler_get_slot(int index, ScheduleSlot_t* slot) {
    if (index < 0 || index >= MAX_SCHEDULE_SLOTS || slot == NULL) {
        return false;
    }
    
    *slot = scheduleSlots[index];
    return true;
}

/**
 * Set a schedule slot
 */
bool scheduler_set_slot(int index, const ScheduleSlot_t* slot) {
    if (index < 0 || index >= MAX_SCHEDULE_SLOTS || slot == NULL) {
        return false;
    }
    
    scheduleSlots[index] = *slot;
    return true;
}

/**
 * Save schedule to preferences
 */
void scheduler_save(void) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    
    prefs.putBool("sched_enabled", scheduleEnabled);
    prefs.putInt("sched_count", scheduleSlotCount);
    
    for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
        String prefix = "s" + String(i) + "_";
        prefs.putBool((prefix + "en").c_str(), scheduleSlots[i].enabled);
        prefs.putInt((prefix + "h").c_str(), scheduleSlots[i].hour);
        prefs.putInt((prefix + "m").c_str(), scheduleSlots[i].minute);
        prefs.putFloat((prefix + "t").c_str(), scheduleSlots[i].targetTemp);
        prefs.putString((prefix + "d").c_str(), scheduleSlots[i].days);
    }
    
    prefs.end();
    Serial.println("[Scheduler] Schedule saved to preferences");
}

/**
 * Load schedule from preferences
 */
void scheduler_load(void) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    
    scheduleEnabled = prefs.getBool("sched_enabled", false);
    scheduleSlotCount = prefs.getInt("sched_count", 0);
    
    for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
        String prefix = "s" + String(i) + "_";
        scheduleSlots[i].enabled = prefs.getBool((prefix + "en").c_str(), false);
        scheduleSlots[i].hour = prefs.getInt((prefix + "h").c_str(), 0);
        scheduleSlots[i].minute = prefs.getInt((prefix + "m").c_str(), 0);
        scheduleSlots[i].targetTemp = prefs.getFloat((prefix + "t").c_str(), 28.0);
        String days = prefs.getString((prefix + "d").c_str(), "SMTWTFS");
        strncpy(scheduleSlots[i].days, days.c_str(), sizeof(scheduleSlots[i].days) - 1);
        scheduleSlots[i].days[sizeof(scheduleSlots[i].days) - 1] = '\0';
    }
    
    prefs.end();
    Serial.println("[Scheduler] Schedule loaded from preferences");
}

/**
 * Get next scheduled change
 */
bool scheduler_get_next(char* nextTime, float* nextTemp) {
    if (!scheduleEnabled) return false;
    
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    if (timeinfo->tm_year < 100) return false;  // Time not synced
    
    int currentMinutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    int currentDay = timeinfo->tm_wday;
    String dayChar = "SMTWTFS";
    char todayChar = dayChar[currentDay];
    
    int nextHour = 99;
    int nextMinute = 99;
    float nextTemperature = 0;
    
    // Find next schedule entry today
    for (int i = 0; i < scheduleSlotCount; i++) {
        if (!scheduleSlots[i].enabled) continue;
        if (strchr(scheduleSlots[i].days, todayChar) == NULL) continue;
        
        int slotMinutes = scheduleSlots[i].hour * 60 + scheduleSlots[i].minute;
        
        if (slotMinutes > currentMinutes) {
            int testMinutes = scheduleSlots[i].hour * 60 + scheduleSlots[i].minute;
            if (testMinutes < (nextHour * 60 + nextMinute)) {
                nextHour = scheduleSlots[i].hour;
                nextMinute = scheduleSlots[i].minute;
                nextTemperature = scheduleSlots[i].targetTemp;
            }
        }
    }
    
    if (nextHour < 99) {
        if (nextTime) {
            sprintf(nextTime, "%02d:%02d", nextHour, nextMinute);
        }
        if (nextTemp) {
            *nextTemp = nextTemperature;
        }
        return true;
    }
    
    return false;
}

/**
 * Clear all schedule slots
 */
void scheduler_clear_all(void) {
    for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
        scheduleSlots[i].enabled = false;
        scheduleSlots[i].hour = 0;
        scheduleSlots[i].minute = 0;
        scheduleSlots[i].targetTemp = 28.0;
        strcpy(scheduleSlots[i].days, "SMTWTFS");
    }
    scheduleSlotCount = 0;
    
    Serial.println("[Scheduler] All slots cleared");
}

/**
 * Get pointer to all slots
 */
ScheduleSlot_t* scheduler_get_slots(void) {
    return scheduleSlots;
}

/**
 * Initialize default schedule
 */
static void initializeDefaultSchedule(void) {
    scheduleSlotCount = 2;
    
    // Slot 0: 7:00 AM - 28°C
    scheduleSlots[0].enabled = true;
    scheduleSlots[0].hour = 7;
    scheduleSlots[0].minute = 0;
    scheduleSlots[0].targetTemp = 28.0;
    strcpy(scheduleSlots[0].days, "SMTWTFS");
    
    // Slot 1: 10:00 PM - 24°C
    scheduleSlots[1].enabled = true;
    scheduleSlots[1].hour = 22;
    scheduleSlots[1].minute = 0;
    scheduleSlots[1].targetTemp = 24.0;
    strcpy(scheduleSlots[1].days, "SMTWTFS");
    
    Serial.println("[Scheduler] Default schedule created");
}
