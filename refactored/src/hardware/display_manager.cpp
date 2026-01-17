/**
 * display_manager.cpp
 * TFT Display Manager Implementation
 */

#include "display_manager.h"
#include "output_manager.h"
#include <Arduino.h>

// TFT and Touch instances
static TFT_eSPI tft = TFT_eSPI();
static XPT2046_Touchscreen touch(TOUCH_CS);

// Display state
static bool initialized = false;
static bool sleeping = false;
static uint8_t brightness = 100;
static DisplayScreen currentScreen = SCREEN_MAIN;
static DisplayScreen previousScreen = SCREEN_MAIN;  // Track screen changes for full redraw
static int selectedOutput = 0;  // Which output is being adjusted (0-2)
static bool needsRefresh = true;  // Flag to trigger refresh only when needed
static bool needsFullRedraw = true;  // Flag for full screen redraw (on screen change)
static unsigned long lastUpdate = 0;
static unsigned long lastTouch = 0;
static unsigned long lastInteraction = 0;
static unsigned long lastButtonPress = 0;

// Previous values for partial updates (to detect changes)
static float prevCurrentTemp[3] = {-999, -999, -999};
static float prevTargetTemp[3] = {-999, -999, -999};
static int prevPower[3] = {-1, -1, -1};
static bool prevHeating[3] = {false, false, false};
static char prevMode[3][16] = {"", "", ""};

// Output data (3 outputs)
static DisplayOutputData outputs[3] = {0};

// System data
static DisplaySystemData systemData = {0};

// Callbacks
static DisplayControlCallback_t controlCallback = nullptr;
static DisplayModeCallback_t modeCallback = nullptr;

// Sleep timeout (5 minutes)
static const unsigned long SLEEP_TIMEOUT = 5UL * 60UL * 1000UL;

// Update rates
static const unsigned long UPDATE_INTERVAL = 100;  // 10 Hz
static const unsigned long TOUCH_INTERVAL = 50;    // 20 Hz
static const unsigned long BUTTON_DEBOUNCE = 300;  // 300ms debounce

// Forward declarations
static void drawMainScreen(void);
static void drawMainScreenPartial(void);  // Partial update for main screen
static void drawControlScreen(void);
static void drawControlScreenPartial(void);  // Partial update for control screen
static void drawInfoScreen(void);
static void handleTouch(void);
static void handleControlTouch(int x, int y);
static uint16_t getHeatColor(int power);
static void invalidatePreviousValues(void);  // Force full redraw next time

/**
 * Initialize display and touch screen
 */
void display_init(void) {
    Serial.println("[Display] Initializing TFT display...");

    // Initialize TFT
    tft.init();
    tft.setRotation(0);  // Portrait mode (240x320)
    tft.fillScreen(TFT_BLACK);

    // Initialize touch
    if (touch.begin()) {
        Serial.println("[Display] Touch screen initialized");
    } else {
        Serial.println("[Display] WARNING: Touch screen initialization failed");
    }

    // Set initial screen
    currentScreen = SCREEN_MAIN;
    initialized = true;
    lastInteraction = millis();

    // Draw splash screen
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("ESP32 Thermostat", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 4);
    tft.drawString("v2.2.0", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 20, 2);
    delay(2000);

    // Draw main screen
    drawMainScreen();

    Serial.println("[Display] Initialization complete");
}

/**
 * Display task - non-blocking update loop
 */
void display_task(void) {
    if (!initialized || sleeping) {
        return;
    }

    unsigned long now = millis();

    // Update display only when needed (not every cycle)
    if (needsRefresh && (now - lastUpdate >= UPDATE_INTERVAL)) {
        display_refresh();
        needsRefresh = false;
        lastUpdate = now;
    }

    // Check touch input at 20 Hz
    if (now - lastTouch >= TOUCH_INTERVAL) {
        handleTouch();
        lastTouch = now;
    }

    // Auto-sleep after timeout (DISABLED - TODO: Make configurable via web settings)
    // if (brightness > 0 && (now - lastInteraction > SLEEP_TIMEOUT)) {
    //     Serial.println("[Display] Auto-sleep timeout");
    //     display_sleep(true);
    // }
}

/**
 * Update output status data
 */
void display_update_output(int outputId, float temp, float target,
                           const char* mode, int power, bool heating) {
    if (outputId < 0 || outputId >= 3) return;

    DisplayOutputData* output = &outputs[outputId];

    // Check if anything actually changed
    bool changed = false;
    if (output->currentTemp != temp || output->targetTemp != target ||
        output->power != power || output->heating != heating ||
        (mode && strcmp(output->mode, mode) != 0)) {
        changed = true;
    }

    // Update the data
    output->id = outputId;
    output->currentTemp = temp;
    output->targetTemp = target;
    output->power = power;
    output->heating = heating;
    output->enabled = true;

    if (mode) {
        strncpy(output->mode, mode, sizeof(output->mode) - 1);
    }

    // Set default name if not set
    if (output->name[0] == '\0') {
        char defaultName[32];
        sprintf(defaultName, "Output %d", outputId + 1);
        strncpy(output->name, defaultName, sizeof(output->name) - 1);
        changed = true;  // Name was set, need to redraw
    }

    // Only request refresh if something changed AND we're on main screen
    if (changed && currentScreen == SCREEN_MAIN && initialized && !sleeping) {
        needsRefresh = true;
    }
}

/**
 * Set output name
 */
void display_set_output_name(int outputId, const char* name) {
    if (outputId < 0 || outputId >= 3 || !name) return;
    strncpy(outputs[outputId].name, name, sizeof(outputs[outputId].name) - 1);
    outputs[outputId].name[sizeof(outputs[outputId].name) - 1] = '\0';
}

/**
 * Update system status data
 */
void display_update_system(const DisplaySystemData* data) {
    if (data) {
        memcpy(&systemData, data, sizeof(DisplaySystemData));
    }
}

/**
 * Switch to different screen
 */
void display_set_screen(DisplayScreen screen) {
    if (screen != currentScreen) {
        currentScreen = screen;
        tft.fillScreen(TFT_BLACK);
        needsRefresh = true;
        display_refresh();
    }
}

/**
 * Get current active screen
 */
DisplayScreen display_get_screen(void) {
    return currentScreen;
}

/**
 * Set display brightness
 */
void display_set_brightness(uint8_t percent) {
    brightness = (percent > 100) ? 100 : percent;
    // Note: Brightness control via backlight LED not implemented yet
    // Would require PWM on backlight pin
}

/**
 * Enable/disable sleep mode
 */
void display_sleep(bool enable) {
    if (enable && !sleeping) {
        Serial.println("[Display] Entering sleep mode");
        sleeping = true;
        tft.fillScreen(TFT_BLACK);
        // Turn off backlight here if PWM control implemented
    } else if (!enable && sleeping) {
        Serial.println("[Display] Waking from sleep mode");
        sleeping = false;
        lastInteraction = millis();
        display_refresh();
    }
}

/**
 * Force display refresh
 */
void display_refresh(void) {
    if (!initialized || sleeping) return;

    // Check if screen changed - need full redraw
    bool screenChanged = (currentScreen != previousScreen);
    if (screenChanged) {
        needsFullRedraw = true;
        previousScreen = currentScreen;
        invalidatePreviousValues();
    }

    switch (currentScreen) {
        case SCREEN_MAIN:
            if (needsFullRedraw) {
                drawMainScreen();
                needsFullRedraw = false;
            } else {
                drawMainScreenPartial();
            }
            break;
        case SCREEN_CONTROL:
            if (needsFullRedraw) {
                drawControlScreen();
                needsFullRedraw = false;
            } else {
                drawControlScreenPartial();
            }
            break;
        case SCREEN_MODE:
            // TODO: Draw mode selection screen
            break;
        case SCREEN_SCHEDULE:
            // TODO: Draw schedule screen
            break;
        case SCREEN_SETTINGS:
            drawInfoScreen();  // Info screen doesn't change frequently, full redraw is fine
            break;
    }
}

/**
 * Set control callback
 */
void display_set_control_callback(DisplayControlCallback_t callback) {
    controlCallback = callback;
}

/**
 * Set mode callback
 */
void display_set_mode_callback(DisplayModeCallback_t callback) {
    modeCallback = callback;
}

/**
 * Check if initialized
 */
bool display_is_initialized(void) {
    return initialized;
}

// ============================================================================
// PRIVATE FUNCTIONS
// ============================================================================

/**
 * Draw main status screen (3 outputs)
 */
static void drawMainScreen(void) {
    tft.fillScreen(TFT_BLACK);

    // Header (green background)
    tft.fillRect(0, 0, SCREEN_WIDTH, 30, TFT_DARKGREEN);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(systemData.deviceName[0] ? systemData.deviceName : "Thermostat", 5, 7, 2);

    // WiFi status icon (right side)
    tft.setTextDatum(TR_DATUM);
    if (systemData.wifiConnected) {
        tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
        tft.drawString("WiFi", SCREEN_WIDTH - 25, 7, 2);
    } else {
        tft.setTextColor(TFT_RED, TFT_DARKGREEN);
        tft.drawString("AP", SCREEN_WIDTH - 25, 7, 2);
    }

    // Info button (small circle in top right corner)
    tft.fillCircle(SCREEN_WIDTH - 10, 15, 8, TFT_BLUE);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("i", SCREEN_WIDTH - 10, 15, 2);

    int yPos = 40;
    const int cardHeight = 85;
    const int cardSpacing = 5;

    // Draw 3 output cards
    for (int i = 0; i < 3; i++) {
        DisplayOutputData* output = &outputs[i];

        // Card background
        uint16_t bgColor = output->heating ? 0x4208 : 0x2104;  // Dark red if heating, dark grey otherwise
        tft.fillRoundRect(5, yPos, SCREEN_WIDTH - 10, cardHeight, 5, bgColor);

        // Output name
        tft.setTextColor(TFT_WHITE, bgColor);
        tft.setTextDatum(TL_DATUM);
        if (output->name[0] != '\0') {
            tft.drawString(output->name, 10, yPos + 5, 2);
        } else {
            char name[32];
            sprintf(name, "Output %d", i + 1);
            tft.drawString(name, 10, yPos + 5, 2);
        }

        // Heating indicator
        if (output->heating) {
            tft.setTextColor(TFT_ORANGE, bgColor);
            tft.setTextDatum(TR_DATUM);
            tft.drawString("HEAT", SCREEN_WIDTH - 15, yPos + 5, 2);
        }

        // Current temperature
        tft.setTextColor(TFT_WHITE, bgColor);
        tft.setTextDatum(TL_DATUM);
        char tempStr[16];
        sprintf(tempStr, "%.1f", output->currentTemp);
        tft.drawString(tempStr, 10, yPos + 25, 4);
        tft.drawString("C", 70, yPos + 28, 2);

        // Target temperature
        tft.setTextDatum(TR_DATUM);
        sprintf(tempStr, "-> %.1f C", output->targetTemp);
        tft.drawString(tempStr, SCREEN_WIDTH - 15, yPos + 30, 2);

        // Mode
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_CYAN, bgColor);
        tft.drawString(output->mode, 10, yPos + 50, 2);

        // Power percentage
        tft.setTextDatum(TR_DATUM);
        sprintf(tempStr, "%d%%", output->power);
        tft.drawString(tempStr, SCREEN_WIDTH - 15, yPos + 50, 2);

        // Power bar
        int barX = 10;
        int barY = yPos + 70;
        int barWidth = SCREEN_WIDTH - 30;
        int barHeight = 8;

        // Background
        tft.fillRoundRect(barX, barY, barWidth, barHeight, 3, TFT_DARKGREY);

        // Fill based on power
        if (output->power > 0) {
            int fillWidth = (barWidth * output->power) / 100;
            uint16_t barColor = getHeatColor(output->power);
            tft.fillRoundRect(barX, barY, fillWidth, barHeight, 3, barColor);
        }

        yPos += cardHeight + cardSpacing;
    }

    // Footer instruction
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Tap output to adjust", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 10, 2);

    // Store current values as previous for next partial update
    for (int i = 0; i < 3; i++) {
        prevCurrentTemp[i] = outputs[i].currentTemp;
        prevTargetTemp[i] = outputs[i].targetTemp;
        prevPower[i] = outputs[i].power;
        prevHeating[i] = outputs[i].heating;
        strncpy(prevMode[i], outputs[i].mode, sizeof(prevMode[i]) - 1);
    }
}

/**
 * Invalidate previous values to force full content redraw
 */
static void invalidatePreviousValues(void) {
    for (int i = 0; i < 3; i++) {
        prevCurrentTemp[i] = -999;
        prevTargetTemp[i] = -999;
        prevPower[i] = -1;
        prevHeating[i] = false;
        prevMode[i][0] = '\0';
    }
}

/**
 * Partial update for main screen - only redraw changed values
 */
static void drawMainScreenPartial(void) {
    int yPos = 40;
    const int cardHeight = 85;
    const int cardSpacing = 5;

    for (int i = 0; i < 3; i++) {
        DisplayOutputData* output = &outputs[i];
        uint16_t bgColor = output->heating ? 0x4208 : 0x2104;
        char tempStr[16];

        // Check if heating state changed (requires card background redraw)
        bool heatingChanged = (output->heating != prevHeating[i]);
        if (heatingChanged) {
            // Redraw entire card background
            tft.fillRoundRect(5, yPos, SCREEN_WIDTH - 10, cardHeight, 5, bgColor);

            // Redraw static elements
            tft.setTextColor(TFT_WHITE, bgColor);
            tft.setTextDatum(TL_DATUM);
            if (output->name[0] != '\0') {
                tft.drawString(output->name, 10, yPos + 5, 2);
            } else {
                char name[32];
                sprintf(name, "Output %d", i + 1);
                tft.drawString(name, 10, yPos + 5, 2);
            }
            tft.drawString("C", 70, yPos + 28, 2);

            // Force redraw all values
            prevCurrentTemp[i] = -999;
            prevTargetTemp[i] = -999;
            prevPower[i] = -1;
            prevMode[i][0] = '\0';
        }

        // Heating indicator
        if (heatingChanged) {
            // Clear area first
            tft.fillRect(SCREEN_WIDTH - 55, yPos + 3, 50, 18, bgColor);
            if (output->heating) {
                tft.setTextColor(TFT_ORANGE, bgColor);
                tft.setTextDatum(TR_DATUM);
                tft.drawString("HEAT", SCREEN_WIDTH - 15, yPos + 5, 2);
            }
        }

        // Current temperature (only if changed)
        if (abs(output->currentTemp - prevCurrentTemp[i]) >= 0.05f) {
            // Clear old value area
            tft.fillRect(10, yPos + 25, 55, 22, bgColor);
            tft.setTextColor(TFT_WHITE, bgColor);
            tft.setTextDatum(TL_DATUM);
            sprintf(tempStr, "%.1f", output->currentTemp);
            tft.drawString(tempStr, 10, yPos + 25, 4);
            prevCurrentTemp[i] = output->currentTemp;
        }

        // Target temperature (only if changed)
        if (abs(output->targetTemp - prevTargetTemp[i]) >= 0.05f) {
            // Clear old value area
            tft.fillRect(SCREEN_WIDTH - 100, yPos + 28, 85, 16, bgColor);
            tft.setTextColor(TFT_WHITE, bgColor);
            tft.setTextDatum(TR_DATUM);
            sprintf(tempStr, "-> %.1f C", output->targetTemp);
            tft.drawString(tempStr, SCREEN_WIDTH - 15, yPos + 30, 2);
            prevTargetTemp[i] = output->targetTemp;
        }

        // Mode (only if changed)
        if (strcmp(output->mode, prevMode[i]) != 0) {
            tft.fillRect(10, yPos + 48, 80, 18, bgColor);
            tft.setTextDatum(TL_DATUM);
            tft.setTextColor(TFT_CYAN, bgColor);
            tft.drawString(output->mode, 10, yPos + 50, 2);
            strncpy(prevMode[i], output->mode, sizeof(prevMode[i]) - 1);
        }

        // Power percentage (only if changed)
        if (output->power != prevPower[i]) {
            // Clear old value area
            tft.fillRect(SCREEN_WIDTH - 55, yPos + 48, 40, 18, bgColor);
            tft.setTextColor(TFT_CYAN, bgColor);
            tft.setTextDatum(TR_DATUM);
            sprintf(tempStr, "%d%%", output->power);
            tft.drawString(tempStr, SCREEN_WIDTH - 15, yPos + 50, 2);

            // Update power bar
            int barX = 10;
            int barY = yPos + 70;
            int barWidth = SCREEN_WIDTH - 30;
            int barHeight = 8;

            tft.fillRoundRect(barX, barY, barWidth, barHeight, 3, TFT_DARKGREY);
            if (output->power > 0) {
                int fillWidth = (barWidth * output->power) / 100;
                uint16_t barColor = getHeatColor(output->power);
                tft.fillRoundRect(barX, barY, fillWidth, barHeight, 3, barColor);
            }
            prevPower[i] = output->power;
        }

        prevHeating[i] = output->heating;
        yPos += cardHeight + cardSpacing;
    }
}

/**
 * Draw control/adjustment overlay
 */
static void drawControlScreen(void) {
    DisplayOutputData* output = &outputs[selectedOutput];

    // Semi-transparent overlay background
    tft.fillScreen(TFT_BLACK);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, 40, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(output->name, SCREEN_WIDTH / 2, 20, 2);

    // Current temperature display (large)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    char tempStr[16];
    sprintf(tempStr, "%.1f C", output->currentTemp);
    tft.drawString(tempStr, SCREEN_WIDTH / 2, 80, 4);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Current", SCREEN_WIDTH / 2, 60, 2);

    // Target temperature with +/- buttons
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Target", SCREEN_WIDTH / 2, 120, 2);
    sprintf(tempStr, "%.1f C", output->targetTemp);
    tft.drawString(tempStr, SCREEN_WIDTH / 2, 150, 4);

    // [-] button
    tft.fillRoundRect(20, 135, 50, 50, 8, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.drawString("-", 45, 160, 4);

    // [+] button
    tft.fillRoundRect(SCREEN_WIDTH - 70, 135, 50, 50, 8, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.drawString("+", SCREEN_WIDTH - 45, 160, 4);

    // Mode button
    tft.fillRoundRect(40, 210, SCREEN_WIDTH - 80, 40, 8, TFT_BLUE);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawString(output->mode, SCREEN_WIDTH / 2, 230, 2);

    // Back button
    tft.fillRoundRect(40, 270, SCREEN_WIDTH - 80, 40, 8, TFT_MAROON);
    tft.setTextColor(TFT_WHITE, TFT_MAROON);
    tft.drawString("BACK", SCREEN_WIDTH / 2, 290, 2);

    // Store values for partial update comparison
    prevCurrentTemp[selectedOutput] = output->currentTemp;
    prevTargetTemp[selectedOutput] = output->targetTemp;
    strncpy(prevMode[selectedOutput], output->mode, sizeof(prevMode[selectedOutput]) - 1);
}

/**
 * Partial update for control screen - only redraw changed values
 */
static void drawControlScreenPartial(void) {
    DisplayOutputData* output = &outputs[selectedOutput];
    char tempStr[16];

    // Current temperature (only if changed)
    if (abs(output->currentTemp - prevCurrentTemp[selectedOutput]) >= 0.05f) {
        // Clear old value area
        tft.fillRect(40, 68, SCREEN_WIDTH - 80, 28, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        sprintf(tempStr, "%.1f C", output->currentTemp);
        tft.drawString(tempStr, SCREEN_WIDTH / 2, 80, 4);
        prevCurrentTemp[selectedOutput] = output->currentTemp;
    }

    // Target temperature (only if changed)
    if (abs(output->targetTemp - prevTargetTemp[selectedOutput]) >= 0.05f) {
        // Clear old value area
        tft.fillRect(70, 138, 100, 28, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        sprintf(tempStr, "%.1f C", output->targetTemp);
        tft.drawString(tempStr, SCREEN_WIDTH / 2, 150, 4);
        prevTargetTemp[selectedOutput] = output->targetTemp;
    }

    // Mode (only if changed)
    if (strcmp(output->mode, prevMode[selectedOutput]) != 0) {
        // Redraw mode button
        tft.fillRoundRect(40, 210, SCREEN_WIDTH - 80, 40, 8, TFT_BLUE);
        tft.setTextColor(TFT_WHITE, TFT_BLUE);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(output->mode, SCREEN_WIDTH / 2, 230, 2);
        strncpy(prevMode[selectedOutput], output->mode, sizeof(prevMode[selectedOutput]) - 1);
    }
}

/**
 * Draw info/settings screen
 */
static void drawInfoScreen(void) {
    tft.fillScreen(TFT_BLACK);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, 30, TFT_DARKGREEN);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("System Info", SCREEN_WIDTH / 2, 15, 2);

    // Content area
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    int y = 45;
    char buf[64];

    // Device name
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Device:", 10, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(systemData.deviceName, 80, y, 2);
    y += 25;

    // Firmware version
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Version:", 10, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(systemData.firmwareVersion, 80, y, 2);
    y += 25;

    // WiFi status
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("WiFi:", 10, y, 2);
    if (systemData.wifiConnected) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Connected", 80, y, 2);
        y += 20;
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(systemData.ssid, 20, y, 2);
        y += 20;
        tft.drawString(systemData.ipAddress, 20, y, 2);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("AP Mode", 80, y, 2);
    }
    y += 25;

    // MQTT status
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("MQTT:", 10, y, 2);
    if (systemData.mqttConnected) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Connected", 80, y, 2);
        y += 20;
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(systemData.mqttBroker, 20, y, 2);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("Disconnected", 80, y, 2);
    }
    y += 25;

    // Uptime
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Uptime:", 10, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    unsigned long hours = systemData.uptime / 3600;
    unsigned long mins = (systemData.uptime % 3600) / 60;
    sprintf(buf, "%luh %lum", hours, mins);
    tft.drawString(buf, 80, y, 2);
    y += 25;

    // Memory
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Memory:", 10, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    sprintf(buf, "%d%% free", systemData.freeMemory);
    tft.drawString(buf, 80, y, 2);

    // Back button
    tft.fillRoundRect(40, 270, SCREEN_WIDTH - 80, 40, 8, TFT_MAROON);
    tft.setTextColor(TFT_WHITE, TFT_MAROON);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BACK", SCREEN_WIDTH / 2, 290, 2);
}

/**
 * Handle touch input
 */
static void handleTouch(void) {
    if (!touch.touched()) {
        return;
    }

    // Read touch coordinates
    TS_Point p = touch.getPoint();

    // Debug: Show raw touch values (disabled to prevent screen flash)
    // Serial.printf("[Display] Raw touch: x=%d, y=%d, z=%d\n", p.x, p.y, p.z);

    // Map touch coordinates to screen
    // IMPORTANT: Touch controller is rotated 90° - MUST swap X/Y and invert Y
    // DO NOT CHANGE THIS MAPPING - it was calibrated and confirmed working!
    // Raw Y (high to low) maps to Screen X (left to right)
    // Raw X (constant ~3500) maps to Screen Y (top to bottom)
    // Test data: Bottom-left button: raw(3483,3446)→screen(10,290) ✓
    //            Bottom-right button: raw(3506,717)→screen(230,290) ✓
    int x = map(p.y, 3800, 200, 0, SCREEN_WIDTH);     // Raw Y → Screen X (inverted)
    int y = map(p.x, 200, 3800, 0, SCREEN_HEIGHT);    // Raw X → Screen Y

    // Constrain to screen bounds
    x = constrain(x, 0, SCREEN_WIDTH - 1);
    y = constrain(y, 0, SCREEN_HEIGHT - 1);

    // Serial.printf("[Display] Mapped touch: x=%d, y=%d (screen: %dx%d)\n", x, y, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Wake from sleep on touch (consume the first touch to wake)
    if (sleeping) {
        display_sleep(false);
        Serial.println("[Display] Waking up - touch consumed");
        return;  // Don't process this touch, just wake up
    }

    // Reset sleep timer on any touch when awake
    lastInteraction = millis();

    // Serial.printf("[Display] Touch at (%d, %d)\n", x, y);

    // Handle touch based on current screen
    if (currentScreen == SCREEN_MAIN) {
        // Debounce check
        unsigned long now = millis();
        if (now - lastButtonPress < BUTTON_DEBOUNCE) {
            return;
        }
        lastButtonPress = now;

        // Check info button (circle at SCREEN_WIDTH-10, y=15, radius=8)
        // Touch area: x > 220, y < 25
        if (x >= (SCREEN_WIDTH - 20) && y <= 25) {
            Serial.println("[Display] Info button pressed");
            display_set_screen(SCREEN_SETTINGS);
            return;
        }

        // Check which output card was tapped
        // Card layout: yPos=40, cardHeight=85, cardSpacing=5
        const int firstCardY = 40;
        const int cardHeight = 85;
        const int cardSpacing = 5;

        for (int i = 0; i < 3; i++) {
            int cardY = firstCardY + i * (cardHeight + cardSpacing);
            int cardBottom = cardY + cardHeight;

            if (y >= cardY && y < cardBottom) {
                // Output card tapped! Open control screen
                Serial.printf("[Display] Output %d tapped - opening control\n", i + 1);
                selectedOutput = i;
                display_set_screen(SCREEN_CONTROL);
                return;
            }
        }
    } else if (currentScreen == SCREEN_CONTROL) {
        handleControlTouch(x, y);
    } else if (currentScreen == SCREEN_SETTINGS) {
        // Info screen - only back button
        unsigned long now = millis();
        if (now - lastButtonPress < BUTTON_DEBOUNCE) {
            return;
        }
        lastButtonPress = now;

        // Back button (40, 270, 160x40)
        if (x >= 40 && x <= (SCREEN_WIDTH - 40) && y >= 270 && y <= 310) {
            display_set_screen(SCREEN_MAIN);
        }
    }
}

/**
 * Handle touch on control/adjustment screen
 */
static void handleControlTouch(int x, int y) {
    // Debounce
    unsigned long now = millis();
    if (now - lastButtonPress < BUTTON_DEBOUNCE) {
        return;
    }
    lastButtonPress = now;

    DisplayOutputData* output = &outputs[selectedOutput];

    // [-] button (20, 135, 50x50)
    if (x >= 20 && x <= 70 && y >= 135 && y <= 185) {
        if (controlCallback) {
            float newTarget = output->targetTemp - 0.5f;
            if (newTarget < 5.0f) newTarget = 5.0f;
            controlCallback(selectedOutput, newTarget);

            // Update local display data immediately
            output->targetTemp = newTarget;
            needsRefresh = true;
            display_refresh();  // Force immediate refresh
        }
        return;
    }

    // [+] button (170, 135, 50x50)
    if (x >= (SCREEN_WIDTH - 70) && x <= (SCREEN_WIDTH - 20) && y >= 135 && y <= 185) {
        if (controlCallback) {
            float newTarget = output->targetTemp + 0.5f;
            if (newTarget > 35.0f) newTarget = 35.0f;
            controlCallback(selectedOutput, newTarget);

            // Update local display data immediately
            output->targetTemp = newTarget;
            needsRefresh = true;
            display_refresh();  // Force immediate refresh
        }
        return;
    }

    // Mode button (40, 210, 160x40)
    if (x >= 40 && x <= (SCREEN_WIDTH - 40) && y >= 210 && y <= 250) {
        if (modeCallback) {
            // Cycle through modes: off -> manual -> pid -> onoff -> timeprop -> schedule -> off
            const char* modes[] = {"off", "manual", "pid", "onoff", "timeprop", "schedule"};
            int numModes = 6;
            int currentModeIndex = 0;
            for (int i = 0; i < numModes; i++) {
                if (strcmp(output->mode, modes[i]) == 0) {
                    currentModeIndex = i;
                    break;
                }
            }
            int nextModeIndex = (currentModeIndex + 1) % numModes;
            modeCallback(selectedOutput, modes[nextModeIndex]);

            // Update local display data immediately
            strncpy(output->mode, modes[nextModeIndex], sizeof(output->mode) - 1);
            needsRefresh = true;
            display_refresh();  // Force immediate refresh
        }
        return;
    }

    // Back button (40, 270, 160x40)
    if (x >= 40 && x <= (SCREEN_WIDTH - 40) && y >= 270 && y <= 310) {
        display_set_screen(SCREEN_MAIN);
        return;
    }
}

/**
 * Get color based on heat power level
 * 0% = Green, 50% = Yellow, 100% = Red
 */
static uint16_t getHeatColor(int power) {
    if (power < 50) {
        // Green to Yellow (0-50%)
        int r = (power * 255) / 50;
        int g = 255;
        int b = 0;
        return tft.color565(r, g, b);
    } else {
        // Yellow to Red (50-100%)
        int r = 255;
        int g = 255 - ((power - 50) * 255) / 50;
        int b = 0;
        return tft.color565(r, g, b);
    }
}
