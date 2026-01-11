#include "hardware/display.h"
#include "config.h"

// This file contains the touch handling functions for the Display class
// Split into separate file for better organization

// ============================================
// MAIN SCREEN TOUCH HANDLING
// ============================================

bool Display::handleMainScreenTouch(int x, int y, DisplayState& state) {
    // Check MINUS button
    if (isInTouchArea(x, y, MAIN_MINUS_BUTTON)) {
        Serial.println("[Display] MINUS button pressed");
        state.targetTemp -= 0.5;
        if (state.targetTemp < TEMP_MIN_SETPOINT) {
            state.targetTemp = TEMP_MIN_SETPOINT;
        }
        return true;
    }
    
    // Check PLUS button
    if (isInTouchArea(x, y, MAIN_PLUS_BUTTON)) {
        Serial.println("[Display] PLUS button pressed");
        state.targetTemp += 0.5;
        if (state.targetTemp > TEMP_MAX_SETPOINT) {
            state.targetTemp = TEMP_MAX_SETPOINT;
        }
        return true;
    }
    
    // Check SETTINGS button
    if (isInTouchArea(x, y, MAIN_SETTINGS_BUTTON)) {
        Serial.println("[Display] SETTINGS button pressed");
        setScreen(ScreenType::SETTINGS);
        return true;
    }
    
    // Check SIMPLE button
    if (isInTouchArea(x, y, MAIN_SIMPLE_BUTTON)) {
        Serial.println("[Display] SIMPLE button pressed");
        setScreen(ScreenType::SIMPLE);
        return true;
    }
    
    return false;
}

// ============================================
// SETTINGS SCREEN TOUCH HANDLING
// ============================================

bool Display::handleSettingsScreenTouch(int x, int y, DisplayState& state) {
    // Check AUTO button
    if (isInTouchArea(x, y, SETTINGS_AUTO_BUTTON)) {
        Serial.println("[Display] AUTO mode selected");
        state.mode = "auto";
        return true;
    }
    
    // Check ON button
    if (isInTouchArea(x, y, SETTINGS_ON_BUTTON)) {
        Serial.println("[Display] MANUAL ON mode selected");
        state.mode = "on";
        return true;
    }
    
    // Check OFF button
    if (isInTouchArea(x, y, SETTINGS_OFF_BUTTON)) {
        Serial.println("[Display] OFF mode selected");
        state.mode = "off";
        return true;
    }
    
    // Check BACK button
    if (isInTouchArea(x, y, SETTINGS_BACK_BUTTON)) {
        Serial.println("[Display] BACK button pressed");
        setScreen(ScreenType::MAIN);
        return true;
    }
    
    return false;
}

// ============================================
// SIMPLE SCREEN TOUCH HANDLING
// ============================================

bool Display::handleSimpleScreenTouch(int x, int y, DisplayState& state) {
    // Any touch returns to main screen
    Serial.println("[Display] Returning to main screen from simple view");
    setScreen(ScreenType::MAIN);
    return true;
}
