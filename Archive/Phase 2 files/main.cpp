/*
 * ESP32 Reptile Thermostat - Modular Version
 * Phase 1: Hardware Drivers Only
 * Version: 1.4.0-phase1
 * 
 * This is a minimal main.cpp for testing Phase 1 hardware modules:
 * - Temperature Sensor
 * - AC Dimmer
 * - TFT Display with Touch
 * 
 * Once Phase 1 is verified, proceed to Phase 2 (Network Stack)
 */

#include <Arduino.h>
#include "config.h"
#include "hardware/sensor.h"
#include "hardware/dimmer.h"
#include "hardware/display.h"

// ============================================
// GLOBAL OBJECTS
// ============================================
TemperatureSensor sensor(ONE_WIRE_BUS);
ACDimmer dimmer(DIMMER_HEAT_PIN, ZEROCROSS_PIN, "Heat");
Display display;

// ============================================
// STATE VARIABLES
// ============================================
DisplayState currentState;
unsigned long lastTempRead = 0;
unsigned long lastDisplayUpdate = 0;

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println("\n\n=================================");
    Serial.println("ESP32 Thermostat - Phase 1 Test");
    Serial.println("Version: " + String(FIRMWARE_VERSION));
    Serial.println("=================================\n");
    
    // Initialize display first (shows startup message)
    if (!display.begin()) {
        Serial.println("ERROR: Display initialization failed!");
    }
    display.showStartupMessage("Phase 1 Testing...");
    delay(BOOT_DELAY_MS);
    
    // Initialize temperature sensor
    Serial.println("Initializing temperature sensor...");
    if (!sensor.begin()) {
        Serial.println("ERROR: Sensor initialization failed!");
        display.showStartupMessage("Sensor Error!");
        delay(3000);
    }
    
    // Initialize AC dimmer
    Serial.println("Initializing AC dimmer...");
    if (!dimmer.begin()) {
        Serial.println("ERROR: Dimmer initialization failed!");
    }
    
    // Initialize state
    currentState.currentTemp = 0.0;
    currentState.targetTemp = TEMP_DEFAULT_TARGET;
    currentState.heating = false;
    currentState.powerOutput = 0;
    currentState.mode = "auto";
    currentState.deviceName = "Phase1-Test";
    currentState.wifiConnected = false;
    currentState.mqttConnected = false;
    currentState.apMode = false;
    
    // Show main screen
    display.setScreen(ScreenType::MAIN);
    display.forceRedraw();
    
    Serial.println("\n=================================");
    Serial.println("Phase 1 Setup Complete!");
    Serial.println("=================================\n");
    Serial.println("Test Instructions:");
    Serial.println("1. Verify temperature reading");
    Serial.println("2. Test +/- buttons on display");
    Serial.println("3. Test screen switching (Settings, Simple)");
    Serial.println("4. Test dimmer control (manual mode)");
    Serial.println("\nSerial Commands:");
    Serial.println("  t - Read temperature");
    Serial.println("  d0-100 - Set dimmer (e.g., 'd50' = 50%)");
    Serial.println("  s - Show status");
    Serial.println("  m - Toggle to next screen");
    Serial.println("\n");
}

// ============================================
// LOOP
// ============================================
void loop() {
    // Read temperature periodically
    if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
        float temp;
        if (sensor.readTemperature(temp)) {
            currentState.currentTemp = temp;
            Serial.print("[Sensor] Temperature: ");
            Serial.print(temp, 1);
            Serial.println("°C");
        } else {
            Serial.println("[Sensor] Read failed!");
        }
        lastTempRead = millis();
    }
    
    // Simple PID-like control for testing
    if (currentState.mode == "auto") {
        float error = currentState.targetTemp - currentState.currentTemp;
        int power = constrain((int)(error * 10), 0, 100);
        currentState.powerOutput = power;
        currentState.heating = (power > 0);
        dimmer.setPower(power);
    } else if (currentState.mode == "on") {
        currentState.powerOutput = 100;
        currentState.heating = true;
        dimmer.setPower(100);
    } else {  // off
        currentState.powerOutput = 0;
        currentState.heating = false;
        dimmer.setPower(0);
    }
    
    // Update display
    if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        display.update(currentState);
        lastDisplayUpdate = millis();
    }
    
    // Handle touch input
    DisplayState newState = currentState;
    if (display.handleTouch(newState)) {
        // Touch modified state - apply changes
        if (newState.targetTemp != currentState.targetTemp) {
            Serial.print("[Touch] Target temp changed to: ");
            Serial.print(newState.targetTemp, 1);
            Serial.println("°C");
            currentState.targetTemp = newState.targetTemp;
        }
        
        if (newState.mode != currentState.mode) {
            Serial.print("[Touch] Mode changed to: ");
            Serial.println(newState.mode);
            currentState.mode = newState.mode;
        }
    }
    
    // Handle serial commands for testing
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "t") {
            // Force temperature read
            float temp;
            if (sensor.readTemperature(temp)) {
                Serial.print("Temperature: ");
                Serial.print(temp, 1);
                Serial.println("°C");
            }
        } else if (cmd.startsWith("d")) {
            // Set dimmer power (d0-d100)
            int power = cmd.substring(1).toInt();
            if (power >= 0 && power <= 100) {
                dimmer.setPower(power);
                currentState.powerOutput = power;
                currentState.heating = (power > 0);
                Serial.print("Dimmer set to: ");
                Serial.print(power);
                Serial.println("%");
            }
        } else if (cmd == "s") {
            // Show status
            Serial.println("\n=== SYSTEM STATUS ===");
            Serial.print("Current Temp: ");
            Serial.print(currentState.currentTemp, 1);
            Serial.println("°C");
            Serial.print("Target Temp:  ");
            Serial.print(currentState.targetTemp, 1);
            Serial.println("°C");
            Serial.print("Mode:         ");
            Serial.println(currentState.mode);
            Serial.print("Heating:      ");
            Serial.println(currentState.heating ? "YES" : "NO");
            Serial.print("Power:        ");
            Serial.print(currentState.powerOutput);
            Serial.println("%");
            Serial.print("Free Heap:    ");
            Serial.print(ESP.getFreeHeap());
            Serial.println(" bytes");
            Serial.print("Screen:       ");
            switch (display.getCurrentScreen()) {
                case ScreenType::MAIN: Serial.println("MAIN"); break;
                case ScreenType::SETTINGS: Serial.println("SETTINGS"); break;
                case ScreenType::SIMPLE: Serial.println("SIMPLE"); break;
            }
            Serial.println("====================\n");
        } else if (cmd == "m") {
            // Cycle through screens
            ScreenType current = display.getCurrentScreen();
            if (current == ScreenType::MAIN) {
                display.setScreen(ScreenType::SETTINGS);
                Serial.println("Switched to SETTINGS screen");
            } else if (current == ScreenType::SETTINGS) {
                display.setScreen(ScreenType::SIMPLE);
                Serial.println("Switched to SIMPLE screen");
            } else {
                display.setScreen(ScreenType::MAIN);
                Serial.println("Switched to MAIN screen");
            }
        } else {
            Serial.println("Unknown command. Available: t, d0-100, s, m");
        }
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}
