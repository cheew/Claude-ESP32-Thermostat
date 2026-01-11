#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>
#include <SPI.h>

/**
 * @brief Screen types for the display
 */
enum class ScreenType {
    MAIN,       // Main control screen with +/- buttons
    SETTINGS,   // Settings screen with mode selection
    SIMPLE      // Large temperature display
};

/**
 * @brief Touch areas for button detection
 */
struct TouchArea {
    int x_min;
    int y_min;
    int x_max;
    int y_max;
    const char* name;
};

/**
 * @brief Display state data structure
 */
struct DisplayState {
    float currentTemp;
    float targetTemp;
    bool heating;
    int powerOutput;
    String mode;
    String deviceName;
    bool wifiConnected;
    bool mqttConnected;
    bool apMode;
};

/**
 * @brief TFT Display and Touch Input Manager
 * 
 * Manages the 2.8" ILI9341 TFT display with XPT2046 touch controller.
 * Provides screen rendering and touch input handling.
 */
class Display {
public:
    /**
     * @brief Construct a new Display object
     */
    Display();
    
    /**
     * @brief Initialize the display and touch controller
     * @return true if successful, false otherwise
     */
    bool begin();
    
    /**
     * @brief Set the current screen
     * @param screen Screen type to display
     */
    void setScreen(ScreenType screen);
    
    /**
     * @brief Get current screen type
     * @return ScreenType Current screen
     */
    ScreenType getCurrentScreen() const { return m_currentScreen; }
    
    /**
     * @brief Update display (call periodically in loop)
     * @param state Current system state data
     */
    void update(const DisplayState& state);
    
    /**
     * @brief Check for touch input and handle it
     * @param state Current system state (may be modified by touch)
     * @return true if touch handled, false if no touch
     */
    bool handleTouch(DisplayState& state);
    
    /**
     * @brief Show startup message
     * @param message Message to display
     */
    void showStartupMessage(const char* message);
    
    /**
     * @brief Force screen redraw on next update
     */
    void forceRedraw() { m_needsRedraw = true; }

private:
    TFT_eSPI m_tft;
    ScreenType m_currentScreen;
    bool m_needsRedraw;
    unsigned long m_lastUpdate;
    DisplayState m_lastState;  // Cache to detect changes
    
    // Touch calibration mapping
    int mapTouchX(uint16_t raw_x);
    int mapTouchY(uint16_t raw_y);
    
    // Screen drawing functions
    void drawMainScreen(const DisplayState& state, bool fullRedraw);
    void drawSettingsScreen(const DisplayState& state, bool fullRedraw);
    void drawSimpleScreen(const DisplayState& state, bool fullRedraw);
    
    // Touch handling per screen
    bool handleMainScreenTouch(int x, int y, DisplayState& state);
    bool handleSettingsScreenTouch(int x, int y, DisplayState& state);
    bool handleSimpleScreenTouch(int x, int y, DisplayState& state);
    
    // Helper functions
    bool isInTouchArea(int x, int y, const TouchArea& area);
    bool stateChanged(const DisplayState& state);
    void cacheState(const DisplayState& state);
    
    // Touch area definitions
    static const TouchArea MAIN_MINUS_BUTTON;
    static const TouchArea MAIN_PLUS_BUTTON;
    static const TouchArea MAIN_SETTINGS_BUTTON;
    static const TouchArea MAIN_SIMPLE_BUTTON;
    
    static const TouchArea SETTINGS_AUTO_BUTTON;
    static const TouchArea SETTINGS_ON_BUTTON;
    static const TouchArea SETTINGS_OFF_BUTTON;
    static const TouchArea SETTINGS_BACK_BUTTON;
};

#endif // DISPLAY_H
