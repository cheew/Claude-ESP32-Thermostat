/**
 * pid_controller.cpp
 * PID Temperature Control Algorithm Implementation
 */

#include "pid_controller.h"
#include <Arduino.h>

// PID gains
static float Kp = 10.0;
static float Ki = 0.5;
static float Kd = 5.0;

// PID state
static float integral = 0.0;
static float previousError = 0.0;
static unsigned long lastComputeTime = 0;

// Debug info
static float lastP = 0.0;
static float lastI = 0.0;
static float lastD = 0.0;
static float lastError = 0.0;

// Constants
static const float INTEGRAL_MIN = -10.0;
static const float INTEGRAL_MAX = 10.0;
static const float OUTPUT_MIN = 0.0;
static const float OUTPUT_MAX = 100.0;

/**
 * Initialize PID controller
 */
void pid_init(float kp, float ki, float kd) {
    Kp = kp;
    Ki = ki;
    Kd = kd;
    
    integral = 0.0;
    previousError = 0.0;
    lastComputeTime = millis();
    
    Serial.println("[PID] Controller initialized");
    Serial.printf("[PID] Gains - Kp: %.2f, Ki: %.2f, Kd: %.2f\n", Kp, Ki, Kd);
}

/**
 * Compute PID output
 */
int pid_compute(float current, float target) {
    // Calculate time delta
    unsigned long now = millis();
    float dt = (now - lastComputeTime) / 1000.0;  // Convert to seconds
    
    // Minimum time step to avoid division issues
    if (dt < 0.001) {
        dt = 0.001;
    }
    
    lastComputeTime = now;
    
    // Calculate error
    float error = target - current;
    lastError = error;
    
    // Proportional term
    float P = Kp * error;
    lastP = P;
    
    // Integral term with anti-windup
    integral += error * dt;
    integral = constrain(integral, INTEGRAL_MIN, INTEGRAL_MAX);
    float I = Ki * integral;
    lastI = I;
    
    // Derivative term
    float derivative = (error - previousError) / dt;
    float D = Kd * derivative;
    lastD = D;
    
    // Store error for next iteration
    previousError = error;
    
    // Calculate total output
    float output = P + I + D;
    
    // Constrain to 0-100%
    int powerOutput = (int)constrain(output, OUTPUT_MIN, OUTPUT_MAX);
    
    // Debug output (comment out for production)
    // Serial.printf("[PID] Err:%.2f P:%.2f I:%.2f D:%.2f Out:%d\n", 
    //               error, P, I, D, powerOutput);
    
    return powerOutput;
}

/**
 * Reset PID state
 */
void pid_reset(void) {
    integral = 0.0;
    previousError = 0.0;
    lastComputeTime = millis();
    
    Serial.println("[PID] State reset");
}

/**
 * Update PID gains
 */
void pid_set_gains(float kp, float ki, float kd) {
    Kp = kp;
    Ki = ki;
    Kd = kd;
    
    Serial.printf("[PID] Gains updated - Kp: %.2f, Ki: %.2f, Kd: %.2f\n", Kp, Ki, Kd);
}

/**
 * Get current gains
 */
void pid_get_gains(float* kp, float* ki, float* kd) {
    if (kp) *kp = Kp;
    if (ki) *ki = Ki;
    if (kd) *kd = Kd;
}

/**
 * Get debug info
 */
void pid_get_debug_info(float* p_term, float* i_term, float* d_term, float* error) {
    if (p_term) *p_term = lastP;
    if (i_term) *i_term = lastI;
    if (d_term) *d_term = lastD;
    if (error) *error = lastError;
}
