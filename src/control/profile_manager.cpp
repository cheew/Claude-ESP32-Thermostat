/**
 * profile_manager.cpp
 * Animal Habitat Profile System
 *
 * Loads animal habitat profiles from JSON files stored in LittleFS /profiles/ directory.
 * Profiles contain species-specific temperature, humidity, and photoperiod data.
 */

#include "profile_manager.h"
#include "output_manager.h"
#include "console.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Profile directory in LittleFS
static const char* PROFILE_DIR = "/profiles";

// In-memory profile list (id + name only, loaded at init)
static ProfileListEntry_t profileList[MAX_PROFILES];
static int profileCount = 0;

// Active profile ID (persisted in Preferences)
static char activeProfileId[32] = "";

/**
 * Scan /profiles/ directory and build the profile list
 */
static void scanProfiles(void) {
    profileCount = 0;

    File dir = LittleFS.open(PROFILE_DIR);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("[Profiles] Directory %s not found\n", PROFILE_DIR);
        return;
    }

    File file = dir.openNextFile();
    while (file && profileCount < MAX_PROFILES) {
        String filename = String(file.name());
        if (filename.endsWith(".json")) {
            // Read just the id and name fields
            DynamicJsonDocument doc(1024);
            DeserializationError err = deserializeJson(doc, file);
            if (!err) {
                const char* id = doc["id"];
                const char* name = doc["name"];
                if (id && name) {
                    strncpy(profileList[profileCount].id, id, sizeof(profileList[0].id) - 1);
                    profileList[profileCount].id[sizeof(profileList[0].id) - 1] = '\0';
                    strncpy(profileList[profileCount].name, name, sizeof(profileList[0].name) - 1);
                    profileList[profileCount].name[sizeof(profileList[0].name) - 1] = '\0';
                    profileCount++;
                    Serial.printf("[Profiles] Found: %s (%s)\n", name, id);
                }
            } else {
                Serial.printf("[Profiles] Failed to parse %s: %s\n", filename.c_str(), err.c_str());
            }
        }
        file = dir.openNextFile();
    }

    Serial.printf("[Profiles] %d profiles available\n", profileCount);
}

void profile_manager_init(void) {
    Serial.println("[Profiles] Initializing profile manager");

    // Load active profile ID from preferences
    Preferences prefs;
    prefs.begin("thermostat", true);
    String savedId = prefs.getString("active_profile", "");
    strncpy(activeProfileId, savedId.c_str(), sizeof(activeProfileId) - 1);
    activeProfileId[sizeof(activeProfileId) - 1] = '\0';
    prefs.end();

    // Scan for available profiles
    scanProfiles();

    if (profileCount > 0) {
        console_add_event_f(CONSOLE_EVENT_SYSTEM, "Loaded %d animal profiles", profileCount);
    }

    if (strlen(activeProfileId) > 0) {
        Serial.printf("[Profiles] Active profile: %s\n", activeProfileId);
    }
}

int profile_manager_get_count(void) {
    return profileCount;
}

const ProfileListEntry_t* profile_manager_get_entry(int index) {
    if (index < 0 || index >= profileCount) return nullptr;
    return &profileList[index];
}

bool profile_manager_load(const char* id, AnimalProfile_t* out) {
    if (!id || !out) return false;

    // Build file path: /profiles/{id}.json
    char path[64];
    snprintf(path, sizeof(path), "%s/%s.json", PROFILE_DIR, id);

    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("[Profiles] File not found: %s\n", path);
        return false;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.printf("[Profiles] JSON parse error in %s: %s\n", path, err.c_str());
        return false;
    }

    // Populate the profile struct
    strncpy(out->id, doc["id"] | id, sizeof(out->id) - 1);
    strncpy(out->name, doc["name"] | "Unknown", sizeof(out->name) - 1);
    strncpy(out->species, doc["species"] | "", sizeof(out->species) - 1);
    strncpy(out->region, doc["region"] | "", sizeof(out->region) - 1);

    // Temperature ranges (from arrays or individual fields)
    JsonArray tempDay = doc["temp_day_range"];
    if (tempDay.size() >= 2) {
        out->tempDayMin = tempDay[0];
        out->tempDayMax = tempDay[1];
    } else {
        out->tempDayMin = doc["temp_day_min"] | 25.0f;
        out->tempDayMax = doc["temp_day_max"] | 30.0f;
    }

    JsonArray tempNight = doc["temp_night_range"];
    if (tempNight.size() >= 2) {
        out->tempNightMin = tempNight[0];
        out->tempNightMax = tempNight[1];
    } else {
        out->tempNightMin = doc["temp_night_min"] | 20.0f;
        out->tempNightMax = doc["temp_night_max"] | 25.0f;
    }

    out->baskingTemp = doc["basking_temp"] | 35.0f;

    // Humidity ranges
    JsonArray humDay = doc["humidity_day"];
    if (humDay.size() >= 2) {
        out->humidityDayMin = humDay[0];
        out->humidityDayMax = humDay[1];
    } else {
        out->humidityDayMin = doc["humidity_day_min"] | 30.0f;
        out->humidityDayMax = doc["humidity_day_max"] | 50.0f;
    }

    JsonArray humNight = doc["humidity_night"];
    if (humNight.size() >= 2) {
        out->humidityNightMin = humNight[0];
        out->humidityNightMax = humNight[1];
    } else {
        out->humidityNightMin = doc["humidity_night_min"] | 40.0f;
        out->humidityNightMax = doc["humidity_night_max"] | 60.0f;
    }

    out->photoperiodHours = doc["photoperiod_hours"] | 12;

    // Season modifiers
    JsonObject seasons = doc["seasons"];
    if (seasons.containsKey("dry")) {
        out->dryTempMod = seasons["dry"]["temp_modifier"] | 0.0f;
        out->dryHumidityMod = seasons["dry"]["humidity_modifier"] | 0.0f;
    } else {
        out->dryTempMod = 0.0f;
        out->dryHumidityMod = 0.0f;
    }
    if (seasons.containsKey("wet")) {
        out->wetTempMod = seasons["wet"]["temp_modifier"] | 0.0f;
        out->wetHumidityMod = seasons["wet"]["humidity_modifier"] | 0.0f;
    } else {
        out->wetTempMod = 0.0f;
        out->wetHumidityMod = 0.0f;
    }

    return true;
}

bool profile_manager_apply(const char* id) {
    AnimalProfile_t profile;
    if (!profile_manager_load(id, &profile)) {
        return false;
    }

    Serial.printf("[Profiles] Applying profile: %s (%s)\n", profile.name, profile.id);

    // Apply day temperature to Output 1 (basking/primary heat)
    OutputConfig_t* output1 = output_manager_get_output(0);
    if (output1 && output1->enabled) {
        output_manager_set_target(0, profile.baskingTemp);
        // Set safety limits from profile
        float safeMax = profile.baskingTemp + 5.0f;
        if (safeMax > 50.0f) safeMax = 50.0f;
        output_manager_set_safety_limits(0, safeMax, profile.tempNightMin - 2.0f, output1->faultTimeoutSec);
    }

    // Apply warm-end temperature to Output 2
    OutputConfig_t* output2 = output_manager_get_output(1);
    if (output2 && output2->enabled) {
        output_manager_set_target(1, profile.tempDayMax);
        output_manager_set_safety_limits(1, profile.tempDayMax + 5.0f, profile.tempNightMin - 2.0f, output2->faultTimeoutSec);
    }

    // Apply cool-end/ambient temperature to Output 3
    OutputConfig_t* output3 = output_manager_get_output(2);
    if (output3 && output3->enabled) {
        output_manager_set_target(2, profile.tempDayMin);
        output_manager_set_safety_limits(2, profile.tempDayMax + 3.0f, profile.tempNightMin - 2.0f, output3->faultTimeoutSec);
    }

    // Generate default schedule for each enabled output
    // Output 1 (basking): high during day, off at night
    if (output1 && output1->enabled) {
        output_manager_set_schedule_slot(0, 0, true, 22, 0, profile.tempNightMin);  // Night
        output_manager_set_schedule_slot(0, 1, true, 6, 0, (profile.tempNightMax + profile.tempDayMin) / 2.0f);  // Dawn
        output_manager_set_schedule_slot(0, 2, true, 8, 0, profile.tempDayMin);     // Morning
        output_manager_set_schedule_slot(0, 3, true, 10, 0, profile.baskingTemp);   // Basking
        output_manager_set_schedule_slot(0, 4, true, 14, 0, profile.baskingTemp);   // Afternoon
        output_manager_set_schedule_slot(0, 5, true, 18, 0, profile.tempDayMin);    // Evening
        output_manager_set_schedule_slot(0, 6, true, 20, 0, (profile.tempNightMax + profile.tempDayMin) / 2.0f);  // Dusk
    }

    // Output 2 (warm end): moderate day cycle
    if (output2 && output2->enabled) {
        output_manager_set_schedule_slot(1, 0, true, 22, 0, profile.tempNightMin);
        output_manager_set_schedule_slot(1, 1, true, 7, 0, profile.tempDayMin);
        output_manager_set_schedule_slot(1, 2, true, 10, 0, profile.tempDayMax);
        output_manager_set_schedule_slot(1, 3, true, 18, 0, profile.tempDayMin);
        output_manager_set_schedule_slot(1, 4, true, 21, 0, profile.tempNightMax);
    }

    // Save active profile ID
    strncpy(activeProfileId, id, sizeof(activeProfileId) - 1);
    activeProfileId[sizeof(activeProfileId) - 1] = '\0';

    Preferences prefs;
    prefs.begin("thermostat", false);
    prefs.putString("active_profile", activeProfileId);
    prefs.end();

    // Save output config
    output_manager_save_config();

    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Applied profile: %s", profile.name);
    Serial.printf("[Profiles] Profile applied and saved: %s\n", profile.name);

    return true;
}

const char* profile_manager_get_active_id(void) {
    return activeProfileId;
}
