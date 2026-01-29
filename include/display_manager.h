/**
 * display_manager.h
 * TFT Display Manager
 *
 * Handles:
 * - TFT display initialization and rendering
 * - Touch screen input processing
 * - Multi-screen UI (main status, control, mode, schedule, settings)
 * - Real-time output status updates
 * - User interactions and callbacks
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Display configuration (from platformio.ini)
#define TFT_CS    15
#define TFT_DC    2
#define TFT_RST   33
#define TFT_MOSI  23
#define TFT_SCLK  18

#define TOUCH_CS  22

// Screen dimensions
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

// UI screens
enum DisplayScreen {
    SCREEN_MAIN,         // Main status dashboard (3 outputs)
    SCREEN_CONTROL,      // Temperature adjustment
    SCREEN_MODE,         // Mode selection
    SCREEN_SCHEDULE,     // Schedule view
    SCREEN_SETTINGS      // System info and settings
};

// Output status data structure
struct DisplayOutputData {
    int id;                    // Output ID (0-2)
    char name[32];             // Output name
    float currentTemp;         // Current temperature
    float targetTemp;          // Target temperature
    char mode[16];             // Control mode (off, manual, pid, onoff, schedule)
    int power;                 // Power percentage (0-100)
    bool heating;              // Currently heating
    bool enabled;              // Output enabled
};

// System status data structure
struct DisplaySystemData {
    char deviceName[32];       // Device name
    char firmwareVersion[16];  // Firmware version
    bool wifiConnected;        // WiFi connection status
    char ssid[32];             // WiFi SSID
    char ipAddress[16];        // IP address
    int rssi;                  // WiFi signal strength
    bool mqttConnected;        // MQTT connection status
    char mqttBroker[32];       // MQTT broker address
    unsigned long uptime;      // Uptime in seconds
    int freeMemory;            // Free memory percentage
};

// Callback function types
typedef void (*DisplayControlCallback_t)(int outputId, float newTarget);
typedef void (*DisplayModeCallback_t)(int outputId, const char* mode);

// Function declarations

/**
 * Initialize display and touch screen
 * Call once during setup()
 */
void display_init(void);

/**
 * Display task - call from main loop
 * Handles screen updates and touch input (non-blocking)
 */
void display_task(void);

/**
 * Update output status data for display
 * @param outputId Output ID (0-2)
 * @param temp Current temperature
 * @param target Target temperature
 * @param mode Control mode string
 * @param power Power percentage (0-100)
 * @param heating Currently heating flag
 */
void display_update_output(int outputId, float temp, float target,
                           const char* mode, int power, bool heating);

/**
 * Set output name for display
 * @param outputId Output ID (0-2)
 * @param name Output name
 */
void display_set_output_name(int outputId, const char* name);

/**
 * Update system status data for display
 * @param data System status data structure
 */
void display_update_system(const DisplaySystemData* data);

/**
 * Switch to different screen
 * @param screen Target screen
 */
void display_set_screen(DisplayScreen screen);

/**
 * Get current active screen
 * @return Current screen
 */
DisplayScreen display_get_screen(void);

/**
 * Set display brightness (0-100%)
 * @param percent Brightness percentage
 */
void display_set_brightness(uint8_t percent);

/**
 * Enable/disable display sleep mode
 * @param enable True to sleep, false to wake
 */
void display_sleep(bool enable);

/**
 * Force display refresh
 * Normally called automatically by display_task()
 */
void display_refresh(void);

/**
 * Set callback for temperature control changes from display
 * @param callback Function to call when user adjusts target temperature
 */
void display_set_control_callback(DisplayControlCallback_t callback);

/**
 * Set callback for mode changes from display
 * @param callback Function to call when user changes output mode
 */
void display_set_mode_callback(DisplayModeCallback_t callback);

/**
 * Check if display hardware is initialized
 * @return True if initialized successfully
 */
bool display_is_initialized(void);

#endif // DISPLAY_MANAGER_H
