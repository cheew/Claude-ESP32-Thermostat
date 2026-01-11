#include "hardware/display.h"
#include "config.h"
#include <Arduino.h>

// Define touch areas for button detection
const TouchArea Display::MAIN_MINUS_BUTTON = {100, 100, 150, 140, "MINUS"};
const TouchArea Display::MAIN_PLUS_BUTTON = {100, 40, 150, 80, "PLUS"};
const TouchArea Display::MAIN_SETTINGS_BUTTON = {185, 40, 220, 90, "SETTINGS"};
const TouchArea Display::MAIN_SIMPLE_BUTTON = {200, 270, 215, 310, "SIMPLE"};

const TouchArea Display::SETTINGS_AUTO_BUTTON = {30, 90, 110, 130, "AUTO"};
const TouchArea Display::SETTINGS_ON_BUTTON = {120, 90, 200, 130, "ON"};
const TouchArea Display::SETTINGS_OFF_BUTTON = {210, 90, 290, 130, "OFF"};
const TouchArea Display::SETTINGS_BACK_BUTTON = {5, 50, 25, 80, "BACK"};

Display::Display()
    : m_tft()
    , m_currentScreen(ScreenType::MAIN)
    , m_needsRedraw(true)
    , m_lastUpdate(0)
{
}

bool Display::begin() {
    Serial.println("[Display] Initializing TFT display...");
    
    // Initialize display
    m_tft.init();
    m_tft.setRotation(DISPLAY_ROTATION);
    m_tft.fillScreen(TFT_BLACK);
    
    // Test display with startup message
    showStartupMessage("Initializing...");
    
    Serial.println("[Display] TFT ready");
    return true;
}

void Display::showStartupMessage(const char* message) {
    m_tft.fillScreen(TFT_BLACK);
    m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    m_tft.setTextSize(2);
    
    // Center "Reptile Thermostat"
    m_tft.setCursor(50, 110);
    m_tft.println("Reptile Thermostat");
    
    // Center message below
    m_tft.setCursor(80, 140);
    m_tft.setTextSize(1);
    m_tft.println(message);
}

void Display::setScreen(ScreenType screen) {
    if (screen != m_currentScreen) {
        m_currentScreen = screen;
        m_needsRedraw = true;
        
        Serial.print("[Display] Screen changed to: ");
        switch (screen) {
            case ScreenType::MAIN: Serial.println("MAIN"); break;
            case ScreenType::SETTINGS: Serial.println("SETTINGS"); break;
            case ScreenType::SIMPLE: Serial.println("SIMPLE"); break;
        }
    }
}

void Display::update(const DisplayState& state) {
    // Throttle updates
    unsigned long now = millis();
    if (now - m_lastUpdate < DISPLAY_UPDATE_INTERVAL && !m_needsRedraw) {
        return;
    }
    m_lastUpdate = now;
    
    // Check if state changed
    bool fullRedraw = m_needsRedraw || stateChanged(state);
    
    // Draw appropriate screen
    switch (m_currentScreen) {
        case ScreenType::MAIN:
            drawMainScreen(state, fullRedraw);
            break;
        case ScreenType::SETTINGS:
            drawSettingsScreen(state, fullRedraw);
            break;
        case ScreenType::SIMPLE:
            drawSimpleScreen(state, fullRedraw);
            break;
    }
    
    // Cache state for next comparison
    cacheState(state);
    m_needsRedraw = false;
}

bool Display::handleTouch(DisplayState& state) {
    uint16_t raw_x = 0, raw_y = 0;
    bool pressed = m_tft.getTouch(&raw_x, &raw_y);
    
    if (!pressed) {
        return false;
    }
    
    // Map raw coordinates to screen coordinates
    int x = mapTouchX(raw_x);
    int y = mapTouchY(raw_y);
    
    Serial.print("[Display] Touch: raw(");
    Serial.print(raw_x);
    Serial.print(",");
    Serial.print(raw_y);
    Serial.print(") -> screen(");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.println(")");
    
    // Handle touch based on current screen
    bool handled = false;
    switch (m_currentScreen) {
        case ScreenType::MAIN:
            handled = handleMainScreenTouch(x, y, state);
            break;
        case ScreenType::SETTINGS:
            handled = handleSettingsScreenTouch(x, y, state);
            break;
        case ScreenType::SIMPLE:
            handled = handleSimpleScreenTouch(x, y, state);
            break;
    }
    
    if (handled) {
        delay(TOUCH_DEBOUNCE_MS);  // Debounce
        m_needsRedraw = true;
    }
    
    return handled;
}

// ============================================
// Touch Coordinate Mapping
// ============================================

int Display::mapTouchX(uint16_t raw_x) {
    // Map touch Y to screen X (rotated display)
    return map(raw_x, 0, 320, 0, 320);
}

int Display::mapTouchY(uint16_t raw_y) {
    // Map touch X to screen Y (rotated display)
    return map(raw_y, 0, 240, 0, 240);
}

// ============================================
// Helper Functions
// ============================================

bool Display::isInTouchArea(int x, int y, const TouchArea& area) {
    return (x >= area.x_min && x <= area.x_max &&
            y >= area.y_min && y <= area.y_max);
}

bool Display::stateChanged(const DisplayState& state) {
    // Check if any relevant state changed
    return (abs(state.currentTemp - m_lastState.currentTemp) > TEMP_CHANGE_THRESHOLD ||
            abs(state.targetTemp - m_lastState.targetTemp) > 0.01 ||
            state.heating != m_lastState.heating ||
            state.powerOutput != m_lastState.powerOutput ||
            state.mode != m_lastState.mode ||
            state.wifiConnected != m_lastState.wifiConnected ||
            state.mqttConnected != m_lastState.mqttConnected);
}

void Display::cacheState(const DisplayState& state) {
    m_lastState = state;
}
