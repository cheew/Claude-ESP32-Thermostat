# ESP32 Thermostat - Future Features & Roadmap

**Document Version:** 2.0
**Last Updated:** February 7, 2026
**Current Project Version:** v2.5.0
**Hardware:** ESP32-S3-WROVER-1 N16R8 (16MB flash, 8MB PSRAM)

---

## 🎯 Project Vision

Transform the ESP32 thermostat from a generic heating controller into a **personal, convenient, unique, and fun reptile habitat environmental control system** supporting:
- **Animal-specific habitat profiles** with species-appropriate temperature ranges
- **Setup wizard** for guided first-boot configuration with animal selection
- **Weather integration** that adjusts habitat conditions based on outdoor weather
- Multiple heating/lighting outputs with rebuilt schedule system
- Multiple temperature sensors with day-of-week scheduling and temperature ramping
- TFT touch display for standalone operation
- Optional PIN security for web interface
- Simple/Advanced web UI modes
- LittleFS filesystem for static files, profiles, and data caching

---

## ✅ Priority 1: Multi-Output Environmental Control (COMPLETED v2.1.0)

### Overview
~~Expand from~~ **Completed:** 3 independent outputs, each with:
- Dedicated temperature sensor assignment
- Flexible control hardware (AC dimmer, SSR, or relay)
- Appropriate control modes based on hardware type
- Independent scheduling per output

### Hardware Support Matrix

| Output | Hardware Options | Control Modes Available | Typical Use Cases |
|--------|------------------|------------------------|-------------------|
| **Output 1** | AC Dimmer | PID, Pulse Width, On/Off, Schedule | Ceramic heat emitter, heat cable |
| **Output 2** | SSR (pulse control) | Pulse Width, On/Off, Schedule | Heat mat, under-tank heater |
| **Output 3** | SSR (on/off) | On/Off, Schedule | Heat lamp, basking light |

### Sensor Configuration
- **3x DS18B20 sensors supported**
- User assigns sensor to output during setup
- Default mapping: Sensor 1 → Output 1, Sensor 2 → Output 2, Sensor 3 → Output 3
- Alternative: Sensor selection dropdown per output
- Support for shared sensor (multiple outputs use same sensor)

### Technical Requirements

**Hardware Interface:**
```
Output 1 (AC Dimmer):
- PWM Pin: GPIO 5 (current)
- Zero-cross Pin: GPIO 27 (shared)
- Control: 0-100% AC phase control

Output 2 (SSR Pulse):
- Control Pin: GPIO 14
- PWM: Slow pulse (1-60 second cycle)
- Example: 50% = 30s ON, 30s OFF

Output 3 (SSR/Relay On/Off):
- Control Pin: GPIO 32
- Digital: HIGH/LOW only
- Example: Thermostat mode (on when temp < target)
```

**Temperature Sensors:**
```
DS18B20 OneWire Bus (GPIO 4):
- Supports multiple sensors on same bus
- Each sensor has unique 64-bit ROM address
- Scan and enumerate on startup
- Store sensor addresses in preferences
- User-friendly naming: "Basking Spot", "Cool Hide", "Ambient"
```

**Configuration Storage:**
```cpp
// Per-output configuration (stored in Preferences)
struct OutputConfig {
    bool enabled;
    char name[32];                    // "Basking Lamp", "Heat Mat"
    uint8_t hardwareType;             // DIMMER, SSR_PULSE, SSR_ONOFF
    uint8_t controlPin;
    uint8_t controlMode;              // PID, PULSE, ONOFF, SCHEDULE
    char sensorAddress[17];           // DS18B20 ROM address (hex string)
    float targetTemp;
    // PID/Pulse parameters
    // Schedule data
};
```

### Web Interface Changes

**Home Page:**
- Split into 3 output sections (tabs or accordion)
- Each shows: Current temp, target, mode, power %
- Independent control per output

**Schedule Page:**
- Dropdown to select output (1, 2, or 3)
- 6 time slots per output (stored separately)
- Copy schedule between outputs button

**Settings Page:**
- New "Output Configuration" section
- Per-output settings:
  - Enable/disable output
  - Name output
  - Hardware type dropdown
  - Assign sensor (dropdown of discovered sensors)
  - PID tuning parameters (if applicable)

**New: Sensor Management Page**
- List all discovered DS18B20 sensors
- Show current temperature from each
- Assign friendly names
- Test individual sensor readings
- Diagnostic info (ROM address, last read time, error count)

### MQTT Extensions

**New Topics:**
```
homeassistant/climate/thermostat_output1/config
homeassistant/climate/thermostat_output2/config
homeassistant/climate/thermostat_output3/config

thermostat/output1/temperature
thermostat/output1/setpoint
thermostat/output1/mode
thermostat/output1/power

(repeat for output2, output3)
```

**Home Assistant Integration:**
- 3 separate climate entities
- Each with own temperature sensor
- Independent mode control
- Diagnostic sensors per output (power %, uptime, errors)

---

## ✅ Priority 2: TFT Display Integration (COMPLETED v2.2.0)

### Overview
**Completed:** Local TFT touch display for standalone operation:
- **Real-time monitoring** of all 3 outputs
- **Touch controls** for temperature adjustment (+/- 0.5C)
- **Mode selection** (Off/Manual/PID/OnOff)
- **Info screen** with WiFi, MQTT, IP details
- **Offline operation** (no WiFi/network required)

### Implementation Details
- Display: ILI9341 240x320 pixels
- Touch: XPT2046 resistive (calibrated)
- Library: TFT_eSPI with build flags in platformio.ini
- Module: `src/hardware/display_manager.cpp`

### Touch Calibration (CRITICAL)
Touch controller is rotated 90 degrees from display:
```cpp
int x = map(p.y, 3800, 200, 0, SCREEN_WIDTH);   // Raw Y -> Screen X
int y = map(p.x, 200, 3800, 0, SCREEN_HEIGHT);  // Raw X -> Screen Y
```

---

## ✅ Priority 2b: Web Interface Security (COMPLETED v2.2.0)

### Overview
**Completed:** Optional PIN-based authentication:
- Toggleable "Secure Mode" in Settings
- 4-6 digit PIN stored in preferences
- Session cookie authentication
- API login endpoint for Android app

### Protected Routes
- `/settings`, `/outputs` - Configuration pages
- `/api/output/*/control` - Temperature/mode changes
- `/api/restart`, `/api/upload` - System actions

### Public Routes (always accessible)
- `/api/status`, `/api/outputs`, `/api/sensors` - Read-only data
- `/`, `/info`, `/history`, `/console` - View pages

---

## ✅ Priority 2c: Simple/Advanced Web UI (COMPLETED v2.2.0)

### Overview
**Completed:** Two web interface modes:

**Simple Mode (default):**
- Clean 3-card dashboard
- Large temperature display
- Target slider (15-35C)
- Mode dropdown
- Power slider (Manual mode)

**Advanced Mode:**
- Full navigation (Outputs, Sensors, Schedule, etc.)
- PID tuning, sensor assignment
- Schedule configuration

Mode toggle via orange dropdown in navigation bar. Preference persists.

---

---

## ✅ ESP32-S3 Migration (COMPLETED v2.5.0)

### Overview
Migrated from ESP32 to **ESP32-S3-WROVER-1 N16R8** to unlock flash/RAM constraints:
- **16MB flash** (was 4MB) - removes all flash space concerns
- **8MB PSRAM** - available for future large data structures
- **LittleFS** filesystem (~8MB partition) for static HTML, JSON profiles, weather cache
- Custom partition table: 4MB app0 + 4MB app1 (OTA) + 8MB LittleFS

### Key Changes
- Board: `esp32-s3-devkitc-1` in platformio.ini
- GPIO 25 reassigned to GPIO 6 (unavailable on S3)
- All other GPIO pins (TFT, sensors, outputs) remain compatible
- LittleFS initialized at boot, serves static files for wizard/profiles

---

## ✅ Animal Habitat Profiles (COMPLETED v2.5.0)

### Overview
JSON-based animal habitat profiles stored in LittleFS that define species-appropriate temperature ranges, humidity, photoperiod, and seasonal modifiers.

### Included Profiles (5 initial)
| Profile | Species | Day Range | Night Range | Basking | Photoperiod |
|---------|---------|-----------|-------------|---------|-------------|
| Bearded Dragon | *Pogona vitticeps* | 28-32°C | 22-25°C | 40°C | 12h |
| Ball Python | *Python regius* | 26-30°C | 24-26°C | 32°C | 12h |
| Leopard Gecko | *Eublepharis macularius* | 26-30°C | 21-24°C | 32°C | 14h |
| Crested Gecko | *Correlophus ciliatus* | 22-26°C | 18-22°C | 28°C | 12h |
| Corn Snake | *Pantherophis guttatus* | 24-28°C | 20-23°C | 30°C | 12h |

### Technical Details
- **Module:** `profile_manager.h/cpp` - init/load/apply/list pattern
- **Storage:** `/profiles/*.json` in LittleFS (ArduinoJson v6 parsing)
- **API Endpoints:**
  - `GET /api/v1/profiles` - List all available profiles
  - `GET /api/v1/profiles/active` - Get currently active profile
  - `POST /api/v1/profiles/apply` - Apply profile to all outputs
- **Profile Application:** Sets output targets, auto-generates 7-slot schedule (Night/Dawn/Morning/Basking/Afternoon/Evening/Dusk)
- **Active profile ID** persisted in Preferences across reboots

---

## ✅ Schedule System Rebuild (COMPLETED v2.5.0)

### Overview
Complete rewrite of the schedule system fixing all 8 known bugs and adding new features.

### Improvements Over v2.4.0
- **12 slots per output** (was 8) for finer-grained day/night cycles
- **Day-of-week filtering** - slots can be active only on specific days (SMTWTFS format)
- **Temperature ramping** - smooth linear interpolation between adjacent slots
- **Labels** - human-readable names per slot ("Dawn", "Basking Peak", "Night")
- **Save/load fix** - days, rampToNext, and label fields now properly persisted
- **API fix** - accepts both "targetTemp" and "target" field names
- **JSON buffer fix** - DynamicJsonDocument(4096) prevents truncation
- **CSV import/export** - download/upload schedules as CSV files
- **Always visible** - Schedule link shown in both Simple and Advanced UI modes

### Schedule Slot Structure
```cpp
typedef struct {
    bool enabled;
    uint8_t hour;        // 0-23
    uint8_t minute;      // 0-59
    float targetTemp;
    bool rampToNext;     // Smooth ramp to next slot
    char days[8];        // "SMTWTFS" format
    char label[16];      // "Dawn", "Basking", etc.
} ScheduleSlot_t;
```

---

## ✅ Setup Wizard (COMPLETED v2.5.0)

### Overview
Multi-step first-run wizard served from LittleFS, with animal profile selection as the centerpiece.

### Wizard Steps
1. **Welcome** - Device name configuration
2. **Animal Selection** - Visual card grid of animal profiles with temperature preview
3. **Climate Preset** - Auto from profile / Manual custom / Weather sync
4. **Season Mode** - Year-round / Dry season / Wet season
5. **Output Configuration** - Per-output name, control mode selection
6. **Network** - MQTT broker configuration (optional)
7. **Review & Apply** - Summary of all settings, one-click apply

### Technical Details
- **Files:** `data/wizard/wizard.html`, `wizard.css`, `wizard.js` (served from LittleFS)
- **First-boot detection:** Preferences key `wizard_done` (bool)
- **Auto-redirect:** Root `/` redirects to `/wizard` when `wizard_done` is false
- **Re-run:** Settings page "Re-run Setup Wizard" clears the flag

---

## ✅ Weather Integration (COMPLETED v2.5.0)

### Overview
OpenWeatherMap integration that fetches outdoor weather data and optionally adjusts habitat temperatures within animal profile bounds. Includes full 24-hour forecast coverage and a standalone Weather Sync control mode.

### Features
- **HTTP API client** fetching current conditions from OpenWeatherMap
- **5-day/3-hour forecast API** (`/data/2.5/forecast?cnt=8`) provides 8 forecast entries (24h coverage) in a single call
- **Configurable interval** (1-6 hours between fetches)
- **LittleFS cache** (`/cache/weather.json`, `/cache/weather_hist.json`) survives reboots - enables offline operation by repeating last 24h of forecast data
- **Timezone-aware matching** - forecast entries store remote city local time; `adjust_target()` picks the entry closest to the ESP32's current local time
- **Weather-adjusted targets:** Shifts schedule temperatures +/- 1.5°C based on outdoor temp
  - Hot outdoor day → slightly warmer basking spot
  - Cold outdoor day → slightly cooler ambient
  - Always clamped within active profile's min/max bounds
- **Graceful fallback:** Uses unmodified schedule targets when weather unavailable
- **Weather Sync control mode** (`CONTROL_MODE_WEATHER = 6`) - standalone mode that derives target from profile midpoint adjusted by outdoor forecast, using PID control
- **24-hour forecast graph** on Outputs page when Weather Sync mode is selected (HTML5 Canvas with grid, filled area, time marker, entry annotations)
- **Mini forecast graph** on Simple mode home page for weather-synced outputs
- **Weather status on Info page** - outdoor temp, conditions, fetch age, forecast entry count
- **Schedule page notice** - banner indicating when output is in Weather Sync mode

### API Endpoints
- `GET /api/v1/weather` - Current weather data (temp, humidity, description, clouds) plus forecast entries array
- `POST /api/v1/weather/config` - Configure API key, city, interval, enable/disable

### Settings Page
Weather configuration section in Settings with:
- Enable/disable toggle
- API key field (masked)
- City name input
- Fetch interval selector (1-6 hours)
- Live status display (current temp and description)

---

## ✅ UI/UX Improvements (COMPLETED v2.5.0)

### Help System
- **Contextual help modals** (? buttons) on Outputs, Sensors, and Schedule pages
- Dark mode compatible overlay with close-on-click-outside behavior

### Display Improvements
- **Real-time clock timestamps** in system logs and console (NTP-synced, uptime fallback)
- **Dark mode fixes** for simple home screen cards and elements
- **Animal symbol in header** when a profile is active
- **Wizard color scheme** updated from blue to green to match main UI
- **24-hour schedule graph** on Schedule page (live-updating Canvas)
- **Schedule mini-graphs** in Simple mode home page per-output cards
- **Weather sync indicator** per output showing when weather is influencing target
- **Re-run wizard button** in Settings page

---

## 📺 Priority 2 (Original Spec - For Reference)

### Original Hardware Options

### Hardware Options

#### Option 1: ILI9341 2.8" TFT (Recommended for Budget)
**Specifications:**
- Display: 2.8" diagonal, 240×320 pixels
- Interface: SPI (HSPI bus on ESP32)
- Touch: Resistive touch screen (XPT2046)
- Power: 3.3V/5V compatible
- Cost: ~$10-15 USD
- Libraries: TFT_eSPI, Adafruit_GFX

**Pinout (ESP32):**
```
TFT Display Pins:
- VCC  → 5V
- GND  → GND
- CS   → GPIO 15 (HSPI CS)
- RESET → GPIO 2
- DC   → GPIO 4 (OneWire moved to GPIO 25)
- MOSI → GPIO 13 (HSPI)
- SCK  → GPIO 14 (HSPI)
- LED  → 3.3V (backlight, or PWM for dimming)
- MISO → GPIO 12 (HSPI)

Touch Screen Pins (XPT2046):
- T_CLK → GPIO 14 (shared with display)
- T_CS  → GPIO 33
- T_DIN → GPIO 13 (shared with display)
- T_DO  → GPIO 12 (shared with display)
- T_IRQ → GPIO 35 (optional, for interrupt-driven touch)
```

**Important Note:**
- OneWire sensors **must be moved** from GPIO 4 to GPIO 25 (or GPIO 26/27)
- This requires hardware modification and firmware update
- Migration process should be documented in update notes

#### Option 2: ST7789 2.0" TFT (Compact Alternative)
**Specifications:**
- Display: 2.0" diagonal, 240×320 pixels
- Interface: SPI
- Touch: Optional (harder to integrate)
- Power: 3.3V
- Cost: ~$8-12 USD
- **Advantage:** Smaller footprint, no touch complexity

#### Option 3: ESP32-2432S028 (All-in-One Development Board)
**Specifications:**
- Display: 2.8" ILI9341 (240×320)
- Touch: Resistive (XPT2046)
- **Built-in:** ESP32, display, touch, SD card, speaker
- Power: USB-C or 5V barrel jack
- Cost: ~$15-20 USD
- **Advantage:** No wiring, pre-integrated
- **Disadvantage:** Locked GPIO assignments, may conflict with existing hardware

### Display Layout Design

#### Main Screen (Default View)
```
┌─────────────────────────────────────┐
│ 🏠 ESP32 Thermostat      🕐 14:35:22│ ← Header (Device name + time)
├─────────────────────────────────────┤
│                                     │
│  Output 1: Basking Lamp       🔥    │ ← Output 1 status
│  Current: 32.5°C   Target: 35.0°C   │
│  Mode: PID         Power: 75%       │
│  ┌───────────────────────────┐      │
│  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░│      │ ← Power bar
│  └───────────────────────────┘      │
│                                     │
├─────────────────────────────────────┤
│  Output 2: Heat Mat           ⏸     │ ← Output 2 status
│  Current: 28.1°C   Target: 30.0°C   │
│  Mode: ON/OFF      Power: 0%        │
│  ┌───────────────────────────┐      │
│  │░░░░░░░░░░░░░░░░░░░░░░░░░░│      │
│  └───────────────────────────┘      │
│                                     │
├─────────────────────────────────────┤
│  Output 3: Ceramic Heater     🔥    │ ← Output 3 status
│  Current: 25.8°C   Target: 27.0°C   │
│  Mode: Schedule    Power: 45%       │
│  ┌───────────────────────────┐      │
│  │▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░│      │
│  └───────────────────────────┘      │
│                                     │
└─────────────────────────────────────┘
│ [Mode] [Target] [Schedule] [Settings]│ ← Touch buttons
└─────────────────────────────────────┘
```

**Color Coding:**
- Background: Black or dark gray (OLED-style for readability)
- Text: White or light gray
- Active heating: Red/Orange indicators 🔥
- Idle: Gray indicators ⏸
- Power bars: Gradient from green (0%) → yellow (50%) → red (100%)

#### Touch Control Screen (Swipe Right or Tap "Target")
```
┌─────────────────────────────────────┐
│ ◀ Back            Adjust Target Temp │
├─────────────────────────────────────┤
│                                     │
│  Select Output:                     │
│  ┌──────┐ ┌──────┐ ┌──────┐        │
│  │ Out 1│ │ Out 2│ │ Out 3│        │ ← Tab selector
│  └──────┘ └──────┘ └──────┘        │
│                                     │
│                                     │
│        Basking Lamp                 │
│                                     │
│          32.5°C → 35.0°C            │ ← Current → Target
│                                     │
│    ┌─────────────────────────┐     │
│    │         [  -  ]          │     │
│    │                          │     │
│    │          35.0°C          │     │ ← Large target display
│    │                          │     │
│    │         [  +  ]          │     │
│    └─────────────────────────┘     │
│                                     │
│  Quick Adjustments:                 │
│  [ +0.5 ]  [ +1.0 ]  [ +2.0 ]       │ ← Quick buttons
│  [ -0.5 ]  [ -1.0 ]  [ -2.0 ]       │
│                                     │
│         [Apply Changes]             │ ← Confirm button
│                                     │
└─────────────────────────────────────┘
```

#### Mode Selection Screen (Tap "Mode")
```
┌─────────────────────────────────────┐
│ ◀ Back               Select Mode     │
├─────────────────────────────────────┤
│                                     │
│  Output 1: Basking Lamp             │
│                                     │
│  ┌─────────────────────────────┐   │
│  │     OFF                      │   │ ← Mode buttons
│  │  Turn off completely         │   │
│  └─────────────────────────────┘   │
│                                     │
│  ┌─────────────────────────────┐   │
│  │   ✓ PID (Auto)               │   │ ← Currently active
│  │  Automatic temp control      │   │
│  └─────────────────────────────┘   │
│                                     │
│  ┌─────────────────────────────┐   │
│  │     ON/OFF Thermostat        │   │
│  │  Simple on/off switching     │   │
│  └─────────────────────────────┘   │
│                                     │
│  ┌─────────────────────────────┐   │
│  │     Manual                   │   │
│  │  Fixed power percentage      │   │
│  └─────────────────────────────┘   │
│                                     │
│  ┌─────────────────────────────┐   │
│  │     Schedule                 │   │
│  │  Time-based control          │   │
│  └─────────────────────────────┘   │
│                                     │
└─────────────────────────────────────┘
```

#### Schedule View (Tap "Schedule")
```
┌─────────────────────────────────────┐
│ ◀ Back          Schedule Overview    │
├─────────────────────────────────────┤
│                                     │
│  Output: [▼ Output 1: Basking Lamp] │ ← Dropdown
│                                     │
│  📅 Active Slots:                   │
│                                     │
│  ⏰ 08:00  →  32°C   M T W T F S S  │ ← Slot 1
│  ⏰ 20:00  →  24°C   M T W T F S S  │ ← Slot 2
│                                     │
│  Next Change:                       │
│  🕐 Tomorrow at 08:00 → 32.0°C      │
│                                     │
│  ℹ️  Edit schedules via web or app  │
│                                     │
└─────────────────────────────────────┘
```

#### Settings/Info Screen (Tap "Settings")
```
┌─────────────────────────────────────┐
│ ◀ Back              System Info      │
├─────────────────────────────────────┤
│                                     │
│  Device: ESP32 Thermostat           │
│  Version: v2.1.1                    │
│                                     │
│  WiFi: Connected                    │
│  SSID: YourNetwork                  │
│  IP: 192.168.1.236                  │
│  RSSI: -65 dBm (Good)               │
│                                     │
│  MQTT: Connected                    │
│  Broker: 192.168.1.100:1883         │
│                                     │
│  Uptime: 2d 14h 35m                 │
│  Memory: 80% free                   │
│                                     │
│  📱 Web Interface:                  │
│  http://192.168.1.236               │
│                                     │
│  Display Settings:                  │
│  Brightness: ████████░░ 80%         │ ← Slider
│  Sleep timeout: [▼ 5 minutes]      │
│                                     │
│        [Screen Calibration]         │
│                                     │
└─────────────────────────────────────┘
```

### Technical Implementation

#### Libraries Required
```cpp
// Display driver
#include <TFT_eSPI.h>        // Hardware-accelerated TFT library
// OR
#include <Adafruit_GFX.h>    // Alternative (more portable)
#include <Adafruit_ILI9341.h>

// Touch screen
#include <XPT2046_Touchscreen.h>

// UI framework options:
// Option A: Custom UI (lightweight)
//   - Draw functions manually
//   - Simple button/widget system
//   - ~5-10 KB flash

// Option B: LVGL (Light and Versatile Graphics Library)
#include <lvgl.h>            // Professional UI framework
//   - Widget system, themes, animations
//   - Touch gesture support
//   - ~50-80 KB flash overhead
//   - Recommended for complex UIs
```

#### Display Manager Module

**New file:** `src/hardware/display_manager.cpp` / `include/display_manager.h`

```cpp
// display_manager.h
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Display configuration
#define TFT_CS    15
#define TFT_DC    4   // NOTE: Conflicts with OneWire!
#define TFT_RST   2
#define TFT_LED   -1  // Backlight (not used, connected to 3.3V)

#define TOUCH_CS  33
#define TOUCH_IRQ 35

// Screen dimensions
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

// UI screens
enum DisplayScreen {
    SCREEN_MAIN,         // Main status dashboard
    SCREEN_CONTROL,      // Temperature adjustment
    SCREEN_MODE,         // Mode selection
    SCREEN_SCHEDULE,     // Schedule view
    SCREEN_SETTINGS      // System info and settings
};

// Function declarations
void display_init(void);
void display_task(void);  // Call from main loop
void display_update_output(int outputId, float temp, float target,
                           const char* mode, int power, bool heating);
void display_set_screen(DisplayScreen screen);
void display_set_brightness(uint8_t percent);  // 0-100%
void display_sleep(bool enable);

// Callback for user interactions
typedef void (*DisplayControlCallback_t)(int outputId, float newTarget);
typedef void (*DisplayModeCallback_t)(int outputId, const char* mode);
void display_set_control_callback(DisplayControlCallback_t callback);
void display_set_mode_callback(DisplayModeCallback_t callback);

#endif // DISPLAY_MANAGER_H
```

**Implementation Notes:**

1. **Non-Blocking Design:**
   ```cpp
   void display_task(void) {
       static unsigned long lastUpdate = 0;
       static unsigned long lastTouch = 0;

       // Update display at 10 Hz (smooth animations)
       if (millis() - lastUpdate >= 100) {
           display_refresh();  // Redraw current screen
           lastUpdate = millis();
       }

       // Check touch input at 20 Hz (responsive)
       if (millis() - lastTouch >= 50) {
           display_handle_touch();
           lastTouch = millis();
       }

       // Auto-sleep after timeout
       if (sleepEnabled && (millis() - lastInteraction > sleepTimeout)) {
           display_sleep(true);
       }
   }
   ```

2. **Memory-Efficient Graphics:**
   ```cpp
   // Use frame buffer sparingly (240×320×2 bytes = 150 KB!)
   // Instead: Direct drawing with minimal buffer

   // Draw output status card (reusable function)
   void drawOutputCard(int x, int y, int w, int h, OutputData* data) {
       tft.fillRoundRect(x, y, w, h, 5, TFT_DARKGREY);
       tft.setTextColor(TFT_WHITE);
       tft.drawString(data->name, x+10, y+5);
       tft.drawFloat(data->currentTemp, 1, x+10, y+25);
       // ... etc

       // Power bar
       int barWidth = (w - 20) * data->power / 100;
       uint16_t barColor = getHeatColor(data->power);
       tft.fillRect(x+10, y+h-20, barWidth, 10, barColor);
   }
   ```

3. **Touch Calibration:**
   ```cpp
   // Store calibration data in Preferences
   Preferences prefs;
   prefs.begin("display", false);
   touchCalibration[0] = prefs.getInt("cal_x1", 300);
   touchCalibration[1] = prefs.getInt("cal_y1", 300);
   // ... etc

   // Calibration routine on first boot
   void display_calibrate(void) {
       tft.fillScreen(TFT_BLACK);
       tft.drawString("Touch each corner", 60, 100);
       // ... collect 4 corner points
       // ... calculate mapping
       // ... save to preferences
   }
   ```

#### Integration with Main System

**In `main.cpp`:**
```cpp
#include "display_manager.h"

void setup() {
    // ... existing initialization

    display_init();

    // Set callback for display control changes
    display_set_control_callback([](int outputId, float newTarget) {
        OutputControl_t* output = output_manager_get_output(outputId);
        if (output) {
            output_manager_set_target(outputId, newTarget);
            Serial.printf("[Display] Output %d target changed to %.1f°C\n",
                         outputId, newTarget);
        }
    });

    display_set_mode_callback([](int outputId, const char* mode) {
        output_manager_set_mode(outputId, mode);
        Serial.printf("[Display] Output %d mode changed to %s\n",
                     outputId, mode);
    });
}

void loop() {
    // ... existing tasks

    display_task();  // Update display and handle touch

    // Push output status to display (every 2 seconds)
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate >= 2000) {
        for (int i = 0; i < MAX_OUTPUTS; i++) {
            OutputControl_t* output = output_manager_get_output(i);
            if (output && output->config.enabled) {
                display_update_output(
                    i,
                    output->currentTemp,
                    output->targetTemp,
                    output_manager_get_mode_string(i),
                    output->powerPercent,
                    output->isHeating
                );
            }
        }
        lastDisplayUpdate = millis();
    }
}
```

### Hardware Migration Plan

#### Issue: GPIO 4 Conflict
**Current:** GPIO 4 is used for OneWire temperature sensors (DS18B20)
**New:** GPIO 4 needed for TFT DC (Data/Command) pin

**Solutions:**

**Option A: Move OneWire to GPIO 25** (Recommended)
- GPIO 25 is unused and supports OneWire
- Requires physical hardware change (rewire sensor bus)
- Update firmware constant: `#define ONEWIRE_PIN 25`
- **Migration process:**
  1. Flash new firmware with both GPIO 4 and GPIO 25 support
  2. Add "OneWire Migration" web page showing instructions
  3. User moves wire from GPIO 4 → GPIO 25
  4. Web page shows "sensors detected" when successful
  5. Future firmwares only support GPIO 25

**Option B: Alternative TFT DC Pin**
- Use different GPIO for DC (e.g., GPIO 26 or GPIO 27)
- Requires custom TFT_eSPI configuration
- No hardware change for existing users
- **Easier migration but less standardized**

**Option C: Conditional Compilation**
```cpp
// platformio.ini
[env:esp32dev]
build_flags =
    -DUSE_TFT_DISPLAY    ; Enable display support
    -DONEWIRE_PIN=25     ; Move OneWire to GPIO 25

[env:esp32dev-no-display]
build_flags =
    -DONEWIRE_PIN=4      ; Keep OneWire on GPIO 4 (legacy)
```

### Development Phases

**Phase 1: Basic Display (8-10 hours)**
- [x] Display driver integration (TFT_eSPI)
- [x] Touch screen driver (XPT2046)
- [x] Display manager module structure
- [x] Main screen layout (3 outputs status)
- [x] Basic touch detection
- [x] Update display with live data

**Phase 2: Interactive Controls (10-12 hours)**
- [x] Target temperature adjustment screen
- [x] Mode selection screen
- [x] Touch button system (reusable widgets)
- [x] Callbacks to main system
- [x] Visual feedback (button press animations)

**Phase 3: Advanced Features (8-10 hours)**
- [x] Schedule overview screen
- [x] Settings/info screen
- [x] WiFi/MQTT status indicators
- [x] Brightness control
- [x] Sleep mode (auto-dim after inactivity)
- [x] Screen calibration routine

**Phase 4: Polish (5-8 hours)**
- [x] Smooth animations (fade in/out)
- [x] Better fonts (anti-aliased)
- [x] Color themes (light/dark mode)
- [x] Error notifications on display
- [x] Boot splash screen
- [x] Low-memory optimizations

### Memory Impact Estimate

**Flash Usage:**
- TFT_eSPI library: ~20 KB
- XPT2046 library: ~3 KB
- Display manager code: ~15 KB
- UI rendering functions: ~10 KB
- Fonts (if custom): ~5-10 KB
- **Total:** ~50-60 KB

**RAM Usage:**
- Display buffer (partial): ~10 KB (not full framebuffer)
- Touch state: ~500 bytes
- UI state machine: ~1 KB
- **Total:** ~12 KB

**Projected Totals (after display integration):**
- Flash: 92% + 4% = **96%** ⚠️ **Tight but acceptable**
- RAM: 22% + 4% = **26%** ✅ **Safe**

**Optimization if needed:**
- Remove debug logging strings
- Compress font data
- Use smaller icon set
- Disable OTA update (free ~15 KB flash)

### User Benefits

1. **Standalone Operation**
   - No phone or computer required
   - Quick glance at status
   - Immediate adjustments

2. **Professional Appearance**
   - Looks like commercial product
   - Suitable for visible installations
   - Impresses visitors

3. **Backup Control**
   - Works when WiFi is down
   - No app installation required
   - Physical interaction (reassuring for some users)

4. **Speed**
   - Faster than loading web page
   - Touch response is instant
   - No network latency

### Potential Issues & Solutions

**Issue 1: Flash Space Constraints (96% usage)**
- **Solution:** Conditional compilation (display as optional feature)
- **Solution:** Optimize existing code (remove unused features)
- **Solution:** Consider ESP32-WROVER (4MB flash) for "premium" version

**Issue 2: OneWire Migration Complexity**
- **Solution:** Provide clear migration guide with photos
- **Solution:** Dual-mode firmware (detect which GPIO has sensors)
- **Solution:** Web interface shows "Display Ready" after migration

**Issue 3: Touch Calibration**
- **Solution:** Auto-calibration on first boot
- **Solution:** Store calibration in Preferences (persistent)
- **Solution:** Recalibration option in settings

**Issue 4: Brightness Flicker**
- **Solution:** Use hardware PWM for backlight (not digital on/off)
- **Solution:** Smooth fade transitions (0-100% over 500ms)

---

## ✅ Priority 3: Setup Wizard (COMPLETED v2.5.0 - See above)

### Overview (Original Spec)
Guided first-run setup wizard to configure:
1. Device name and WiFi credentials
2. MQTT broker settings (optional)
3. Sensor discovery and naming
4. Output hardware configuration
5. Initial temperature targets
6. Verification and test

### Implementation Approach

**Trigger Conditions:**
- No saved WiFi credentials (first boot)
- User manually enters setup mode (web button)
- Factory reset command

**Wizard Flow:**

```
┌─────────────────────────────────────┐
│  Step 1: Welcome & Device Name      │
│  - Enter friendly name              │
│  - Brief overview of wizard         │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 2: WiFi Configuration         │
│  - Scan for networks                │
│  - Select SSID or enter manually    │
│  - Enter password                   │
│  - Test connection                  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 3: MQTT Setup (Optional)      │
│  - Skip or configure                │
│  - Broker address                   │
│  - Credentials (if required)        │
│  - Test connection                  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 4: Sensor Discovery           │
│  - Scan OneWire bus                 │
│  - Display found sensors            │
│  - Name each sensor                 │
│  - Verify readings                  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 5: Output Configuration       │
│  - For each output (1, 2, 3):       │
│    - Enable/disable                 │
│    - Name output                    │
│    - Select hardware type           │
│    - Assign sensor                  │
│    - Set initial target temp        │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  Step 6: Verification & Test        │
│  - Summary of configuration         │
│  - Test each output (manual trigger)│
│  - Confirm and save                 │
│  - Option to restart wizard         │
└──────────────┬──────────────────────┘
               │
               ▼
          [ Complete! ]
```

**Technical Implementation:**

```cpp
// Setup wizard state machine
enum WizardState {
    WIZARD_WELCOME,
    WIZARD_WIFI,
    WIZARD_MQTT,
    WIZARD_SENSORS,
    WIZARD_OUTPUTS,
    WIZARD_VERIFY,
    WIZARD_COMPLETE
};

// Web routes
server.on("/setup", handleSetupWizard);
server.on("/api/setup/networks", handleWiFiScan);
server.on("/api/setup/sensors", handleSensorScan);
server.on("/api/setup/test-output", handleOutputTest);
server.on("/api/setup/save", handleSetupSave);
```

**User Experience:**
- Progress indicator (Step 1 of 6)
- Previous/Next navigation
- Skip optional steps (MQTT, unused outputs)
- Validation on each step before proceeding
- Ability to go back and change settings
- Auto-save to preferences on completion
- Automatic reboot and connection test

---

## 📱 Priority 4: Android App Development

### Overview
Professional Android app for remote thermostat management:
- **Device Discovery:** mDNS/Bonjour auto-discovery on local network
- **Multi-Device Support:** Manage multiple thermostats (multiple reptile enclosures)
- **Real-Time Monitoring:** Live temperature readings and status
- **Quick Controls:** Adjust targets, change modes
- **Notifications:** Alerts for temperature warnings
- **Dark Mode:** Match system theme

### Features

**Core Functionality:**
1. **Auto-Discovery**
   - Scan local network for thermostats using mDNS
   - Display list of found devices with names
   - Add manually by IP address if discovery fails
   - Save discovered devices for quick access

2. **Dashboard**
   - Grid/list view of all managed devices
   - Current temp, target temp, mode
   - Status indicators (heating, connected, errors)
   - Quick tap to open device detail

3. **Device Detail View**
   - Real-time temperature graph (last 24 hours)
   - Current status and power output
   - Quick adjust target temperature (slider or +/- buttons)
   - Mode switcher (Auto, Manual On/Off, Schedule)
   - Access to full schedule editor
   - View logs and console

4. **Settings Management**
   - Edit device name
   - Configure WiFi and MQTT from app
   - PID tuning interface
   - Access to all web interface features

5. **Notifications**
   - Temperature alerts (too high/low)
   - Connection loss warnings
   - Schedule activation reminders
   - Option to configure per-device

### Technical Stack

**Android (Kotlin):**
```
Dependencies:
- Jetpack Compose (UI)
- Retrofit + OkHttp (REST API calls)
- Paho MQTT Android (real-time updates)
- JmDNS (mDNS discovery)
- MPAndroidChart (temperature graphing)
- Room Database (local device storage)
- WorkManager (background updates)
```

**Communication Methods:**
1. **REST API (Primary):**
   - Use existing `/api/*` endpoints
   - Polling for status updates (every 5 seconds)
   - Control commands via POST

2. **MQTT (Optional):**
   - Subscribe to status topics for real-time updates
   - Requires MQTT broker accessible from phone (local or cloud)

3. **WebSocket (Future Enhancement):**
   - Add WebSocket endpoint to ESP32
   - Push updates to app instantly
   - More efficient than polling

**mDNS Discovery:**
```kotlin
// Android mDNS service discovery
val serviceType = "_http._tcp.local."
val txtQuery = "type=reptile-thermostat"

// ESP32 advertises:
// Service: _http._tcp
// Port: 80
// TXT Records:
//   - type=reptile-thermostat
//   - version=1.9.0
//   - name=Havoc's Thermostat
//   - outputs=3
```

### Development Phases

**Phase 1: MVP (Minimum Viable Product)**
- [x] mDNS device discovery
- [x] Add device manually by IP
- [x] Dashboard showing all devices
- [x] Device detail with current status
- [x] Adjust target temperature
- [x] View temperature graph
- [x] Basic error handling

**Phase 2: Enhanced Controls**
- [x] Mode switching (Auto/Manual/Schedule)
- [x] Schedule viewer
- [x] Schedule editor
- [x] Settings access
- [x] Dark mode support

**Phase 3: Advanced Features**
- [x] Push notifications
- [x] MQTT real-time updates
- [x] Multi-output support (3 outputs)
- [x] Sensor management from app
- [x] Background monitoring
- [x] Widget for home screen

**Phase 4: Polish**
- [x] Material Design 3
- [x] Animations and transitions
- [x] Offline mode (cache last known state)
- [x] App settings (update frequency, notification preferences)
- [x] Help/tutorial screens

### API Requirements for App

The ESP32 firmware must provide:

**Current APIs (Already Available):**
- ✅ `/api/info` - Device information
- ✅ `/api/logs` - System logs
- ✅ `/api/history` - Temperature history
- ✅ `/api/control` - Control endpoint (mode, target, power)

**New APIs Needed:**
```
GET /api/status - Quick status (all outputs)
{
  "outputs": [
    {
      "id": 1,
      "name": "Basking Lamp",
      "enabled": true,
      "temp": 32.5,
      "target": 35.0,
      "mode": "auto",
      "power": 75,
      "sensor": "Basking Spot"
    },
    // ... output 2, 3
  ]
}

POST /api/output/{id}/control - Per-output control
{
  "target": 35.0,
  "mode": "auto"
}

GET /api/sensors - List all sensors
POST /api/sensors/{address}/name - Rename sensor
```

---

## 🐛 Known Issues to Address

### 1. WiFi Reconnection Bug

**Issue:** WiFi does not automatically reconnect after connection loss. Device stays in AP mode even when WiFi becomes available.

**Current Behavior:**
- WiFi disconnects (e.g., router reboot)
- Device switches to AP mode
- When WiFi returns, device stays in AP mode
- Requires power cycle to reconnect

**Root Cause:**
Looking at [wifi_manager.cpp:60-86](wifi_manager.cpp#L60-L86), the `wifi_task()` function has a flaw:

```cpp
void wifi_task(void) {
    // Don't reconnect if in AP mode
    if (apMode) {
        return;  // ← PROBLEM: Never exits AP mode!
    }
    // ... reconnection logic
}
```

**Fix Required:**
```cpp
void wifi_task(void) {
    // In AP mode: periodically check if saved WiFi is available
    if (apMode) {
        if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
            // Try to reconnect to saved WiFi
            Serial.println("[WiFi] AP mode: Attempting to reconnect to WiFi");
            wifi_connect(NULL, NULL);  // Will switch out of AP mode if successful
        }
        return;
    }

    // Normal reconnection logic when not in AP mode
    if (WiFi.status() != WL_CONNECTED) {
        if (currentState != WIFI_STATE_DISCONNECTED) {
            Serial.println("[WiFi] Connection lost");
            currentState = WIFI_STATE_DISCONNECTED;
        }

        if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
            Serial.println("[WiFi] Attempting reconnection...");
            wifi_connect(NULL, NULL);
        }
    }
    // ...
}
```

**Additional Improvements:**
- Add user setting: "AP mode timeout" (default: stay in AP permanently, or set minutes)
- Add web button in AP mode: "Retry WiFi Connection"
- Add visual indicator on home page showing AP mode vs normal WiFi
- Log WiFi reconnection attempts to console

**Testing Plan:**
1. Connect device to WiFi
2. Disable WiFi router
3. Wait for device to enter AP mode
4. Re-enable WiFi router
5. Verify device automatically reconnects within 30 seconds
6. Check console log for reconnection attempts

---

## 📊 Technical Considerations

### Memory Usage (ESP32-S3-WROVER-1 N16R8)

**Hardware Upgrade (v2.5.0):**
- **Flash:** 16MB total (was 4MB) - flash constraints eliminated
  - app0: 4MB, app1: 4MB (OTA), LittleFS: ~8MB
- **PSRAM:** 8MB available for large data structures
- **RAM:** ~520 KB SRAM (was 327 KB)

**LittleFS Usage:**
- Animal profiles (5 JSON files): ~5 KB
- Setup wizard (HTML/CSS/JS): ~15 KB
- Weather cache: ~1 KB
- **Total used:** ~21 KB of 8MB (0.3%) - extensive room for growth

**Firmware Size:**
- All modules including profiles, weather, wizard backend: well within 4MB app partition

### Performance Impact

**Temperature Reading:**
- Current: 1 sensor every 2 seconds
- Multi-sensor: 3 sensors every 2 seconds
- DS18B20 read time: ~750ms per sensor (sequential)
- **Solution:** Stagger reads (sensor 1 at 0s, sensor 2 at 1s, sensor 3 at 2s)

**PID Calculation:**
- Current: 1 PID loop @ 1 Hz
- Multi-output: 3 PID loops @ 1 Hz
- PID calc time: <1ms each
- **Impact:** Negligible

**Web Interface:**
- Current: Single output status update
- Multi-output: 3× data in JSON response
- **Impact:** Minimal (gzipped JSON, still <1 KB)

### Development Complexity

**Low Complexity:**
- ✅ WiFi reconnection fix (1 hour)
- ✅ Sensor management page (2-3 hours)

**Medium Complexity:**
- 🟡 Multi-output control (8-10 hours)
  - Duplicate existing logic × 3
  - Refactor web pages for tabs/accordion
  - Update MQTT topics
  - Preferences storage schema

**High Complexity:**
- 🔴 Setup wizard (20-25 hours)
  - Multi-step state machine
  - Network scanning APIs
  - Sensor enumeration
  - Comprehensive testing

- 🔴 Android app (80-120 hours)
  - mDNS discovery
  - REST API integration
  - UI/UX design and implementation
  - Testing on multiple devices

---

## 🗓️ Recommended Implementation Order

### ✅ Phase 1: Foundation Fixes (COMPLETED v1.9.0)
1. ~~**Fix WiFi reconnection bug**~~ (1 hour)
   - High user impact
   - Critical for reliability
   - Simple fix

2. ~~**Add sensor management page**~~ (3 hours)
   - Required for multi-output
   - Useful even with single output
   - Lays groundwork for setup wizard

### ✅ Phase 2: Multi-Output Support (COMPLETED v2.1.0)
3. ~~**Implement 3-output system**~~ (10 hours)
   - Core feature expansion
   - Enables professional reptile setups
   - Most requested feature

4. ~~**Update MQTT for multi-output**~~ (2 hours)
   - Maintain Home Assistant integration
   - 3 separate climate entities

### ✅ Phase 3: TFT Display Integration (COMPLETED v2.3.0)
5. **TFT display hardware integration**
   - Standalone operation without web/app
   - Touch controls for temperature adjustment
   - Partial display updates (no flashing)

### ✅ Phase 4: Animal Habitat Features (COMPLETED v2.5.0)
6. **ESP32-S3 migration** - 16MB flash, 8MB PSRAM, LittleFS
7. **Animal profile system** - 5 species profiles with auto-schedule generation
8. **Schedule rebuild** - 12 slots, day-of-week, ramping, CSV import/export
9. **Setup wizard** - 7-step first-boot wizard with animal selection
10. **Weather integration** - OpenWeatherMap outdoor temp adjustment

### 🔜 Phase 5: Remote Management (NEXT)
11. **Android app development** (100+ hours)
    - Significant time investment
    - High user value
    - Enables remote management
    - Opens door to monetization (premium features)

### Phase 6: Polish & Optimization
12. **Performance tuning** (5 hours)
    - Optimize multi-sensor reads
    - Reduce memory usage where possible
    - Improve web page load times

13. **Documentation & guides** (8 hours)
    - User manual
    - Setup guide with photos
    - Troubleshooting guide
    - Video tutorials

---

## 💡 Future Enhancements (Post-Roadmap)

### Animal Habitat Expansion
- **More species profiles** - Community-contributed JSON profiles
- **Custom profile editor** - Web UI for creating/editing profiles
- **Breeding mode** - Season cycling with gradual temperature changes
- **Photoperiod control** - Automatic light scheduling from profile data

### Hardware Additions
- **Humidity Control:**
  - DHT22 or SHT31 sensors
  - Misting system control (relay or SSR)
  - Humidity scheduling from animal profiles

- **Lighting Control:**
  - RGB LED strips (PWM control)
  - Sunrise/sunset simulation using photoperiod from profiles
  - UVB timer management

- **Remote Sensors:**
  - ESP-NOW for wireless sensors
  - Multiple enclosure monitoring from single controller

### Software Features
- **Cloud Integration:**
  - Remote access without local network
  - Data logging to cloud service
  - Mobile app notifications over internet

- **Weather Enhancements:**
  - 7-day forecast caching for predictive adjustment
  - Seasonal auto-detection from weather trends
  - Multiple weather provider support

- **Machine Learning:**
  - Adaptive PID tuning based on thermal mass
  - Predictive scheduling (adjust heat before scheduled time)
  - Anomaly detection (alert if temp doesn't stabilize)

- **Data Export:**
  - CSV export of temperature logs
  - PDF reports (daily/weekly/monthly)
  - Integration with Google Sheets

### Advanced Features
- **Multi-Zone System:**
  - 6+ outputs for commercial breeders
  - Hierarchical control (room ambient + individual enclosures)
  - Modbus or RS485 expansion

- **Voice Control:**
  - Amazon Alexa integration (via HA or custom skill)
  - Google Assistant support
  - Custom voice commands

---

## 📞 Support & Collaboration

**Questions or suggestions?**
- Open an issue on GitHub
- Discussion board for feature requests
- Pull requests welcome!

**Contributors:**
- Looking for Android developers
- Looking for UI/UX designers
- Documentation writers needed

---

**Document Status:** Living document - updated as features are implemented and new ideas emerge.

**Next Review:** After Phase 5 completion (Android app / remote management)
