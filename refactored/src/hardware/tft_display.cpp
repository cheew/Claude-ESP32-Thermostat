/**
 * tft_display.cpp
 * TFT Display Manager Implementation
 */

#include "tft_display.h"
#include <Arduino.h>

// TFT instance
static TFT_eSPI tft = TFT_eSPI();

// Display state
static Screen_t currentScreen = SCREEN_MAIN;
static bool displayNeedsUpdate = true;
static unsigned long lastUpdateTime = 0;
static const unsigned long UPDATE_INTERVAL = 500; // 500ms

// Status indicators
static bool wifiConnected = false;
static bool wifiAPMode = false;
static bool mqttConnected = false;
static char deviceName[32] = "Thermostat";

// Display values
static float displayCurrentTemp = 0.0;
static float displayTargetTemp = 28.0;
static bool displayHeating = false;
static char displayMode[8] = "auto";
static int displayPower = 0;

// Touch callback
static TouchButtonCallback_t touchCallback = NULL;

// Forward declarations
static void drawMainScreen(void);
static void drawSettingsScreen(void);
static void drawSimpleScreen(void);
static void updateMainDisplay(void);
static void updateSimpleDisplay(void);
static void checkTouch(void);

/**
 * Initialize TFT display
 */
void tft_init(void) {
    tft.init();
    tft.setRotation(1);  // Landscape
    tft.fillScreen(TFT_BLACK);
    
    Serial.println("[TFT] Display initialized");
}

/**
 * Task - call from main loop
 */
void tft_task(void) {
    // Update display at regular intervals
    if (millis() - lastUpdateTime >= UPDATE_INTERVAL) {
        if (currentScreen == SCREEN_MAIN && displayNeedsUpdate) {
            updateMainDisplay();
            displayNeedsUpdate = false;
        } else if (currentScreen == SCREEN_SIMPLE && displayNeedsUpdate) {
            updateSimpleDisplay();
            displayNeedsUpdate = false;
        }
        // Settings screen is static, no updates needed
        
        lastUpdateTime = millis();
    }
    
    // Check for touch input
    checkTouch();
}

/**
 * Show boot message
 */
void tft_show_boot_message(const char* message) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(50, 110);
    tft.println("Reptile Thermostat");
    tft.setCursor(80, 140);
    tft.setTextSize(1);
    tft.println(message);
}

/**
 * Switch screen
 */
void tft_switch_screen(Screen_t screen) {
    currentScreen = screen;
    displayNeedsUpdate = true;
    
    switch (screen) {
        case SCREEN_MAIN:
            drawMainScreen();
            break;
        case SCREEN_SETTINGS:
            drawSettingsScreen();
            break;
        case SCREEN_SIMPLE:
            drawSimpleScreen();
            break;
    }
    
    Serial.printf("[TFT] Switched to screen %d\n", screen);
}

/**
 * Get current screen
 */
Screen_t tft_get_current_screen(void) {
    return currentScreen;
}

/**
 * Update main screen values
 */
void tft_update_main_screen(float currentTemp, float targetTemp, 
                            bool heating, const char* mode, int power) {
    if (abs(currentTemp - displayCurrentTemp) > 0.1 ||
        abs(targetTemp - displayTargetTemp) > 0.1 ||
        heating != displayHeating ||
        strcmp(mode, displayMode) != 0 ||
        power != displayPower) {
        
        displayCurrentTemp = currentTemp;
        displayTargetTemp = targetTemp;
        displayHeating = heating;
        strncpy(displayMode, mode, sizeof(displayMode) - 1);
        displayMode[sizeof(displayMode) - 1] = '\0';
        displayPower = power;
        
        displayNeedsUpdate = true;
    }
}

/**
 * Update simple screen values
 */
void tft_update_simple_screen(float currentTemp, float targetTemp,
                              bool heating, const char* mode, int power) {
    // Same as main screen update
    tft_update_main_screen(currentTemp, targetTemp, heating, mode, power);
}

/**
 * Set WiFi status
 */
void tft_set_wifi_status(bool connected, bool apMode) {
    if (connected != wifiConnected || apMode != wifiAPMode) {
        wifiConnected = connected;
        wifiAPMode = apMode;
        if (currentScreen == SCREEN_MAIN) {
            drawMainScreen();
        }
    }
}

/**
 * Set MQTT status
 */
void tft_set_mqtt_status(bool connected) {
    if (connected != mqttConnected) {
        mqttConnected = connected;
        if (currentScreen == SCREEN_MAIN) {
            drawMainScreen();
        }
    }
}

/**
 * Set device name
 */
void tft_set_device_name(const char* name) {
    if (name != NULL) {
        strncpy(deviceName, name, sizeof(deviceName) - 1);
        deviceName[sizeof(deviceName) - 1] = '\0';
    }
}

/**
 * Register touch callback
 */
void tft_register_touch_callback(TouchButtonCallback_t callback) {
    touchCallback = callback;
}

/**
 * Request update
 */
void tft_request_update(void) {
    displayNeedsUpdate = true;
}

// ===== SCREEN DRAWING FUNCTIONS =====

static void drawMainScreen(void) {
    tft.fillScreen(TFT_BLACK);
    
    // Header bar
    tft.fillRect(0, 0, 320, 35, TFT_DARKGREEN);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextSize(2);
    tft.setCursor(5, 10);
    
    // Device name (truncate if too long)
    char name[14];
    strncpy(name, deviceName, 13);
    name[13] = '\0';
    tft.print(name);
    
    // WiFi status icon (top right)
    tft.setTextSize(1);
    tft.setCursor(245, 8);
    if (wifiConnected) {
        tft.setTextColor(TFT_GREEN, TFT_DARKGREEN);
        tft.print("WiFi");
    } else if (wifiAPMode) {
        tft.setTextColor(TFT_ORANGE, TFT_DARKGREEN);
        tft.print("AP");
    } else {
        tft.setTextColor(TFT_RED, TFT_DARKGREEN);
        tft.print("No WiFi");
    }
    
    // MQTT status (below WiFi)
    tft.setCursor(245, 20);
    if (mqttConnected) {
        tft.setTextColor(TFT_GREEN, TFT_DARKGREEN);
        tft.print("MQTT");
    } else {
        tft.setTextColor(TFT_DARKGREY, TFT_DARKGREEN);
        tft.print("----");
    }
    
    // Labels
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.print("Current:");
    
    tft.setCursor(10, 120);
    tft.print("Target:");
    
    // Draw button boxes
    // MINUS button
    tft.fillRect(210, 115, 45, 45, TFT_DARKGREY);
    tft.drawRect(210, 115, 45, 45, TFT_WHITE);
    
    // PLUS button
    tft.fillRect(265, 115, 45, 45, TFT_DARKGREY);
    tft.drawRect(265, 115, 45, 45, TFT_WHITE);
    
    // SIMPLE VIEW button (bottom left)
    tft.fillRect(10, 185, 70, 50, TFT_PURPLE);
    tft.drawRect(10, 185, 70, 50, TFT_WHITE);
    
    // SETTINGS button (bottom right)
    tft.fillRect(240, 185, 70, 50, TFT_NAVY);
    tft.drawRect(240, 185, 70, 50, TFT_WHITE);
    
    // Button labels
    tft.setTextSize(4);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setCursor(222, 123);
    tft.print("-");
    tft.setCursor(277, 123);
    tft.print("+");
    
    // Simple button label
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_PURPLE);
    tft.setCursor(17, 205);
    tft.print("SIMPLE");
    
    // Settings button label
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setCursor(248, 202);
    tft.print("SETUP");
    
    displayNeedsUpdate = true;
}

static void updateMainDisplay(void) {
    // Current temp
    tft.fillRect(120, 45, 180, 40, TFT_BLACK);
    tft.setTextSize(4);
    tft.setCursor(120, 50);
    tft.setTextColor(displayHeating ? TFT_RED : TFT_CYAN, TFT_BLACK);
    
    char tempStr[10];
    sprintf(tempStr, "%.1fC", displayCurrentTemp);
    tft.print(tempStr);
    
    // Target temp
    tft.fillRect(120, 115, 80, 35, TFT_BLACK);
    tft.setTextSize(3);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(120, 120);
    sprintf(tempStr, "%.1fC", displayTargetTemp);
    tft.print(tempStr);
    
    // Status
    tft.fillRect(90, 185, 140, 50, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(90, 195);
    
    if (strcmp(displayMode, "off") == 0) {
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.print("OFF");
    } else if (displayHeating) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.print("HEAT");
        tft.setTextSize(1);
        tft.setCursor(90, 215);
        tft.print(displayPower);
        tft.print("%");
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.print("IDLE");
    }
}

static void drawSimpleScreen(void) {
    tft.fillScreen(TFT_BLACK);
    
    // Small header
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.print(deviceName);
    
    tft.setCursor(160, 10);
    tft.print("Tap to exit");
    
    // Giant temperature
    tft.setTextColor(displayHeating ? TFT_RED : TFT_CYAN, TFT_BLACK);
    
    char tempStr[10];
    sprintf(tempStr, "%.1f", displayCurrentTemp);
    
    tft.setTextSize(14);
    tft.setCursor(10, 50);
    tft.print(tempStr);
    
    tft.setTextSize(10);
    tft.print("C");
    
    // Target and status at bottom
    tft.setTextSize(3);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 190);
    tft.print("Target: ");
    tft.print(displayTargetTemp, 1);
    tft.print("C");
    
    tft.setTextSize(2);
    tft.setCursor(10, 215);
    if (strcmp(displayMode, "off") == 0) {
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.print("OFF");
    } else if (displayHeating) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.print("HEATING ");
        tft.print(displayPower);
        tft.print("%");
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.print("IDLE");
    }
}

static void updateSimpleDisplay(void) {
    // Redraw entire simple screen (it's simple enough)
    drawSimpleScreen();
}

static void drawSettingsScreen(void) {
    tft.fillScreen(TFT_BLACK);
    
    // Header
    tft.fillRect(0, 0, 320, 35, TFT_DARKGREEN);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextSize(2);
    tft.setCursor(5, 10);
    tft.print("Settings");
    
    // Back button
    tft.fillRect(250, 5, 60, 25, TFT_NAVY);
    tft.drawRect(250, 5, 60, 25, TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(265, 13);
    tft.print("BACK");
    
    // Device name
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.print("Device Name:");
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 75);
    tft.print(deviceName);
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(10, 95);
    tft.print("(Change via web interface)");
    
    // Mode selection
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 120);
    tft.print("Mode:");
    
    int yPos = 145;
    
    // AUTO button
    if (strcmp(displayMode, "auto") == 0) {
        tft.fillRect(10, yPos, 90, 40, TFT_DARKGREEN);
        tft.drawRect(10, yPos, 90, 40, TFT_GREEN);
    } else {
        tft.fillRect(10, yPos, 90, 40, TFT_DARKGREY);
        tft.drawRect(10, yPos, 90, 40, TFT_WHITE);
    }
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, strcmp(displayMode, "auto") == 0 ? TFT_DARKGREEN : TFT_DARKGREY);
    tft.setCursor(25, 157);
    tft.print("AUTO");
    
    // MANUAL ON button
    if (strcmp(displayMode, "on") == 0) {
        tft.fillRect(110, yPos, 90, 40, TFT_MAROON);
        tft.drawRect(110, yPos, 90, 40, TFT_RED);
    } else {
        tft.fillRect(110, yPos, 90, 40, TFT_DARKGREY);
        tft.drawRect(110, yPos, 90, 40, TFT_WHITE);
    }
    tft.setTextColor(TFT_WHITE, strcmp(displayMode, "on") == 0 ? TFT_MAROON : TFT_DARKGREY);
    tft.setCursor(125, 157);
    tft.print("ON");
    
    // OFF button
    if (strcmp(displayMode, "off") == 0) {
        tft.fillRect(210, yPos, 90, 40, TFT_NAVY);
        tft.drawRect(210, yPos, 90, 40, TFT_BLUE);
    } else {
        tft.fillRect(210, yPos, 90, 40, TFT_DARKGREY);
        tft.drawRect(210, yPos, 90, 40, TFT_WHITE);
    }
    tft.setTextColor(TFT_WHITE, strcmp(displayMode, "off") == 0 ? TFT_NAVY : TFT_DARKGREY);
    tft.setCursor(230, 157);
    tft.print("OFF");
    
    // Info section
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 200);
    tft.print("Use web interface for");
    tft.setCursor(10, 212);
    tft.print("full configuration");
}

// ===== TOUCH HANDLING =====

static void checkTouch(void) {
    uint16_t t_x = 0, t_y = 0;
    bool pressed = tft.getTouch(&t_x, &t_y);
    
    if (!pressed || touchCallback == NULL) return;
    
    int screen_x = map(t_y, 0, 320, 0, 320);
    int screen_y = map(t_x, 0, 240, 0, 240);
    
    int buttonId = -1;
    
    if (currentScreen == SCREEN_MAIN) {
        // MINUS button
        if (screen_x >= 100 && screen_x <= 150 && screen_y >= 100 && screen_y <= 140) {
            buttonId = BTN_MINUS;
        }
        // PLUS button
        else if (screen_x >= 100 && screen_x <= 150 && screen_y >= 40 && screen_y <= 80) {
            buttonId = BTN_PLUS;
        }
        // SETTINGS button
        else if (screen_x >= 185 && screen_x <= 220 && screen_y >= 40 && screen_y <= 90) {
            buttonId = BTN_SETTINGS;
        }
        // SIMPLE button
        else if (screen_x >= 200 && screen_x <= 215 && screen_y >= 270 && screen_y <= 310) {
            buttonId = BTN_SIMPLE;
        }
    }
    else if (currentScreen == SCREEN_SETTINGS) {
        // AUTO button
        if (screen_x >= 30 && screen_x <= 110 && screen_y >= 90 && screen_y <= 130) {
            buttonId = BTN_MODE_AUTO;
        }
        // MANUAL ON button
        else if (screen_x >= 120 && screen_x <= 200 && screen_y >= 90 && screen_y <= 130) {
            buttonId = BTN_MODE_ON;
        }
        // OFF button
        else if (screen_x >= 210 && screen_x <= 290 && screen_y >= 90 && screen_y <= 130) {
            buttonId = BTN_MODE_OFF;
        }
        // BACK button
        else if (screen_x >= 5 && screen_x <= 25 && screen_y >= 50 && screen_y <= 80) {
            buttonId = BTN_BACK;
        }
    }
    else if (currentScreen == SCREEN_SIMPLE) {
        // Any touch returns to main
        buttonId = BTN_BACK;
    }
    
    if (buttonId >= 0) {
        Serial.printf("[TFT] Button pressed: %d\n", buttonId);
        touchCallback(buttonId);
        delay(300); // Debounce
    }
}
