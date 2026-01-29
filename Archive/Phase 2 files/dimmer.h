#ifndef DIMMER_H
#define DIMMER_H

#include <RBDdimmer.h>

/**
 * @brief AC Dimmer control interface
 * 
 * Encapsulates RobotDyn AC dimmer module control.
 * Supports single or dual dimmer configuration for heat and light control.
 */
class ACDimmer {
public:
    /**
     * @brief Construct a new ACDimmer object
     * @param pwmPin GPIO pin for PWM control
     * @param zeroCrossPin GPIO pin for zero-cross detection
     * @param name Descriptive name (e.g., "Heat", "Light")
     */
    ACDimmer(int pwmPin, int zeroCrossPin, const char* name = "Dimmer");
    
    /**
     * @brief Initialize the dimmer
     * @return true if successful, false otherwise
     */
    bool begin();
    
    /**
     * @brief Set dimmer power output
     * @param percent Power level 0-100%
     */
    void setPower(int percent);
    
    /**
     * @brief Get current power output
     * @return int Power level 0-100%
     */
    int getPower() const { return m_currentPower; }
    
    /**
     * @brief Check if dimmer is on (power > 0)
     * @return true if on, false if off
     */
    bool isOn() const { return m_currentPower > 0; }
    
    /**
     * @brief Turn dimmer off (set to 0%)
     */
    void turnOff();
    
    /**
     * @brief Turn dimmer on (set to last non-zero power or 100%)
     */
    void turnOn();
    
    /**
     * @brief Get dimmer name
     * @return const char* Dimmer name
     */
    const char* getName() const { return m_name; }

private:
    dimmerLamp m_dimmer;
    int m_currentPower;
    int m_lastPower;  // Remember last non-zero power for turnOn()
    const char* m_name;
    
    /**
     * @brief Constrain power value to valid range
     * @param percent Input power value
     * @return int Constrained value (0-100)
     */
    int constrainPower(int percent);
};

/**
 * @brief Dual dimmer controller for heat and light
 * 
 * Manages two AC dimmers with a shared zero-cross detection pin.
 * Provides coordinated control for heating and lighting.
 */
class DualDimmerController {
public:
    /**
     * @brief Construct a new Dual Dimmer Controller
     * @param heatPwmPin GPIO pin for heat dimmer PWM
     * @param lightPwmPin GPIO pin for light dimmer PWM
     * @param zeroCrossPin Shared GPIO pin for zero-cross detection
     */
    DualDimmerController(int heatPwmPin, int lightPwmPin, int zeroCrossPin);
    
    /**
     * @brief Initialize both dimmers
     * @return true if successful, false otherwise
     */
    bool begin();
    
    /**
     * @brief Set heat dimmer power
     * @param percent Power level 0-100%
     */
    void setHeatPower(int percent);
    
    /**
     * @brief Set light dimmer power
     * @param percent Power level 0-100%
     */
    void setLightPower(int percent);
    
    /**
     * @brief Get heat dimmer power
     * @return int Power level 0-100%
     */
    int getHeatPower() const { return m_heatDimmer.getPower(); }
    
    /**
     * @brief Get light dimmer power
     * @return int Power level 0-100%
     */
    int getLightPower() const { return m_lightDimmer.getPower(); }
    
    /**
     * @brief Check if heating is active
     * @return true if heat power > 0
     */
    bool isHeating() const { return m_heatDimmer.isOn(); }
    
    /**
     * @brief Check if light is on
     * @return true if light power > 0
     */
    bool isLightOn() const { return m_lightDimmer.isOn(); }
    
    /**
     * @brief Turn both dimmers off (safety)
     */
    void turnOffAll();

private:
    ACDimmer m_heatDimmer;
    ACDimmer m_lightDimmer;
};

#endif // DIMMER_H
