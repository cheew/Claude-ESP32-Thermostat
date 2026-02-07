/**
 * profile_manager.h
 * Animal Habitat Profile System
 *
 * Loads JSON-based animal habitat profiles from LittleFS.
 * Each profile contains temperature, humidity, and photoperiod
 * data for a specific reptile species.
 */

#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include <Arduino.h>

#define MAX_PROFILES 20

/**
 * Full animal habitat profile data
 */
typedef struct {
    char id[32];              // "bearded_dragon"
    char name[48];            // "Bearded Dragon"
    char species[64];         // "Pogona vitticeps"
    char region[48];          // "Australian Desert"
    float tempDayMin;         // Day low temp (cool end)
    float tempDayMax;         // Day high temp (warm end)
    float tempNightMin;       // Night low temp
    float tempNightMax;       // Night high temp
    float baskingTemp;        // Basking spot temperature
    float humidityDayMin;     // Day humidity low %
    float humidityDayMax;     // Day humidity high %
    float humidityNightMin;   // Night humidity low %
    float humidityNightMax;   // Night humidity high %
    uint8_t photoperiodHours; // Hours of light per day
    // Season modifiers
    float dryTempMod;         // Dry season temp offset
    float dryHumidityMod;     // Dry season humidity offset
    float wetTempMod;         // Wet season temp offset
    float wetHumidityMod;     // Wet season humidity offset
} AnimalProfile_t;

/**
 * Lightweight entry for profile listing (id + name only)
 */
typedef struct {
    char id[32];
    char name[48];
} ProfileListEntry_t;

/**
 * Initialize profile manager
 * Scans /profiles/ directory in LittleFS for .json files
 */
void profile_manager_init(void);

/**
 * Get number of available profiles
 */
int profile_manager_get_count(void);

/**
 * Get profile list entry by index (for dropdown population)
 * @param index Profile index (0 to count-1)
 * @return Pointer to list entry, or nullptr if invalid
 */
const ProfileListEntry_t* profile_manager_get_entry(int index);

/**
 * Load full profile data by ID
 * @param id Profile ID (e.g., "bearded_dragon")
 * @param out Pointer to receive profile data
 * @return true if found and loaded
 */
bool profile_manager_load(const char* id, AnimalProfile_t* out);

/**
 * Apply profile to the thermostat outputs
 * Sets target temperatures and generates a default schedule
 * @param id Profile ID to apply
 * @return true if applied successfully
 */
bool profile_manager_apply(const char* id);

/**
 * Get currently active profile ID
 * @return Profile ID string, or empty string if none active
 */
const char* profile_manager_get_active_id(void);

#endif // PROFILE_MANAGER_H
