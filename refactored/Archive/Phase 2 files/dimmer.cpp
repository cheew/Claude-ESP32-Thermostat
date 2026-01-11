#include "hardware/dimmer.h"
#include "config.h"
#include <Arduino.h>

// ============================================
// ACDimmer Implementation
// ============================================

ACDimmer::ACDimmer(int pwmPin, int zeroCrossPin, const char* name)
    : m_dimmer(pwmPin, zeroCrossPin)
    , m_currentPower(0)
    , m_lastPower(100)
    , m_name(name)
{
}

bool ACDimmer::begin() {
    Serial.print("[Dimmer:");
    Serial.print(m_name);
    Serial.println("] Initializing...");
    
    // Initialize dimmer in normal mode, turned on
    m_dimmer.begin(NORMAL_MODE, ON);
    
    // Set to 0% initially (off)
    m_dimmer.setPower(0);
    m_currentPower = 0;
    
    Serial.print("[Dimmer:");
    Serial.print(m_name);
    Serial.println("] Ready");
    
    return true;
}

void ACDimmer::setPower(int percent) {
    int constrainedPower = constrainPower(percent);
    
    // Only update if power actually changed
    if (constrainedPower != m_currentPower) {
        m_dimmer.setPower(constrainedPower);
        
        // Remember last non-zero power for turnOn()
        if (constrainedPower > 0) {
            m_lastPower = constrainedPower;
        }
        
        m_currentPower = constrainedPower;
        
        Serial.print("[Dimmer:");
        Serial.print(m_name);
        Serial.print("] Power set to ");
        Serial.print(constrainedPower);
        Serial.println("%");
    }
}

void ACDimmer::turnOff() {
    setPower(0);
}

void ACDimmer::turnOn() {
    // Turn on to last non-zero power level
    setPower(m_lastPower);
}

int ACDimmer::constrainPower(int percent) {
    if (percent < PID_OUTPUT_MIN) return PID_OUTPUT_MIN;
    if (percent > PID_OUTPUT_MAX) return PID_OUTPUT_MAX;
    return percent;
}

// ============================================
// DualDimmerController Implementation
// ============================================

DualDimmerController::DualDimmerController(int heatPwmPin, int lightPwmPin, int zeroCrossPin)
    : m_heatDimmer(heatPwmPin, zeroCrossPin, "Heat")
    , m_lightDimmer(lightPwmPin, zeroCrossPin, "Light")
{
}

bool DualDimmerController::begin() {
    Serial.println("[DualDimmer] Initializing dual dimmer system...");
    
    bool heatOk = m_heatDimmer.begin();
    bool lightOk = m_lightDimmer.begin();
    
    if (heatOk && lightOk) {
        Serial.println("[DualDimmer] Both dimmers ready");
        return true;
    } else {
        Serial.println("[DualDimmer] ERROR: Dimmer initialization failed!");
        return false;
    }
}

void DualDimmerController::setHeatPower(int percent) {
    m_heatDimmer.setPower(percent);
}

void DualDimmerController::setLightPower(int percent) {
    m_lightDimmer.setPower(percent);
}

void DualDimmerController::turnOffAll() {
    Serial.println("[DualDimmer] SAFETY: Turning off all dimmers");
    m_heatDimmer.turnOff();
    m_lightDimmer.turnOff();
}
