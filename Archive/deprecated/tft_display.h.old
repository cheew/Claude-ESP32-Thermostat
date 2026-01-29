/**
 * tft_display.h
 * TFT Display Manager for ILI9341 Touchscreen
 * 
 * Manages all display output and touch input:
 * - Three screens: Main, Settings, Simple View
 * - Touch button handling
 * - Status indicators (WiFi, MQTT)
 * - Temperature display
 */

#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <TFT_eSPI.h>

// Screen identifiers
typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_SETTINGS = 1,
    SCREEN_SIMPLE = 2
} Screen_t;

// Touch button callback
typedef void (*TouchButtonCallback_t)(int button_id);

// Button IDs
#define BTN_PLUS 1
#define BTN_MINUS 2
#define BTN_SETTINGS 3
#define BTN_SIMPLE 4
#define BTN_MODE_AUTO 5
#define BTN_MODE_ON 6
#define BTN_MODE_OFF 7
#define BTN_BACK 8

/**
 * Initialize TFT display
 */
void tft_init(void);

/**
 * Task to call from main loop
 * Handles display updates and touch processing
 */
void tft_task(void);

/**
 * Show boot message
 * @param message Message to display
 */
void tft_show_boot_message(const char* message);

/**
 * Switch to a different screen
 * @param screen Screen to display
 */
void tft_switch_screen(Screen_t screen);

/**
 * Get current screen
 * @return Current screen ID
 */
Screen_t tft_get_current_screen(void);

/**
 * Update main screen values
 * @param currentTemp Current temperature
 * @param targetTemp Target temperature
 * @param heating True if heating
 * @param mode Operating mode
 * @param power Power output percentage
 */
void tft_update_main_screen(float currentTemp, float targetTemp, 
                            bool heating, const char* mode, int power);

/**
 * Update simple screen values
 * @param currentTemp Current temperature
 * @param targetTemp Target temperature
 * @param heating True if heating
 * @param mode Operating mode
 * @param power Power output percentage
 */
void tft_update_simple_screen(float currentTemp, float targetTemp,
                              bool heating, const char* mode, int power);

/**
 * Set WiFi status indicator
 * @param connected True if WiFi connected
 * @param apMode True if in AP mode
 */
void tft_set_wifi_status(bool connected, bool apMode);

/**
 * Set MQTT status indicator
 * @param connected True if MQTT connected
 */
void tft_set_mqtt_status(bool connected);

/**
 * Set device name for display
 * @param name Device name
 */
void tft_set_device_name(const char* name);

/**
 * Register touch button callback
 * @param callback Function to call when button pressed
 */
void tft_register_touch_callback(TouchButtonCallback_t callback);

/**
 * Force display update on next task call
 */
void tft_request_update(void);

#endif // TFT_DISPLAY_H
