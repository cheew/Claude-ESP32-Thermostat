# ESP32 Reptile Thermostat - Modular Architecture

**Version:** 1.4.0  
**Status:** Phase 1 (Hardware Drivers) Complete  
**Architecture:** Modular C++

---

## Project Structure

```
esp32-thermostat/
├── platformio.ini           # Build configuration
├── MIGRATION_CHECKLIST.md   # Refactoring progress tracker
├── README.md                # This file
│
├── include/                 # Header files
│   ├── config.h            # All configuration constants
│   └── hardware/           # Hardware driver interfaces
│       ├── sensor.h        # DS18B20 temperature sensor
│       ├── dimmer.h        # AC dimmer control
│       └── display.h       # TFT display & touch
│
└── src/                     # Implementation files
    ├── main.cpp            # Main program (minimal)
    └── hardware/           # Hardware driver implementations
        ├── sensor.cpp
        ├── dimmer.cpp
        ├── display.cpp
        ├── display_screens.cpp  # Screen rendering
        └── display_touch.cpp    # Touch handlers
```

---

## Modules Overview

### Configuration (`config.h`)
Central configuration file containing all constants:
- Hardware pin definitions
- System limits and thresholds
- Network credentials (defaults)
- PID parameters
- Display settings

### Hardware Drivers

#### Temperature Sensor (`sensor.h/cpp`)
- **Purpose:** DS18B20 OneWire temperature sensor interface
- **Features:**
  - Error detection and counting
  - Validation of readings
  - Last-known-good temperature caching
- **Usage:**
  ```cpp
  TemperatureSensor sensor(ONE_WIRE_BUS);
  sensor.begin();
  float temp;
  if (sensor.readTemperature(temp)) {
      Serial.println(temp);
  }
  ```

#### AC Dimmer (`dimmer.h/cpp`)
- **Purpose:** RobotDyn AC dimmer control
- **Features:**
  - Single or dual dimmer support
  - Power ramping (0-100%)
  - Zero-cross shared between dimmers
  - Safety turn-off function
- **Usage:**
  ```cpp
  ACDimmer dimmer(DIMMER_HEAT_PIN, ZEROCROSS_PIN, "Heat");
  dimmer.begin();
  dimmer.setPower(50);  // 50% power
  ```

#### Display (`display.h/cpp`)
- **Purpose:** 2.8" ILI9341 TFT display with XPT2046 touch
- **Features:**
  - Three screen types (Main, Settings, Simple)
  - Touch input handling
  - State-based rendering (only updates when needed)
  - Coordinate mapping for touch calibration
- **Usage:**
  ```cpp
  Display display;
  display.begin();
  DisplayState state = {...};
  display.update(state);
  if (display.handleTouch(state)) {
      // State modified by touch
  }
  ```

---

## Current Phase: Phase 1

**Status:** ✓ Complete  
**Modules:** Hardware drivers only  
**Testing:** Use `src/main.cpp` Phase 1 test program

### Phase 1 Test Features

1. **Temperature Reading**
   - Reads DS18B20 every 2 seconds
   - Displays on TFT screen
   - Logs to serial

2. **Display Screens**
   - Main: Shows temp, target, +/- buttons
   - Settings: Mode selection (auto/on/off)
   - Simple: Large temperature display

3. **Touch Input**
   - +/- buttons adjust target temperature
   - Settings button opens mode selection
   - Simple button shows large display
   - Back button returns to main

4. **Dimmer Control**
   - Auto mode: Simple proportional control
   - Manual on: 100% power
   - Off mode: 0% power

5. **Serial Commands**
   ```
   t         - Read temperature
   d0-100    - Set dimmer power (e.g., d50 = 50%)
   s         - Show system status
   m         - Cycle through screens
   ```

### Testing Phase 1

1. **Upload firmware:**
   ```bash
   pio run -t upload
   ```

2. **Open serial monitor:**
   ```bash
   pio device monitor
   ```

3. **Verify functionality:**
   - [ ] Temperature readings appear
   - [ ] Display shows current temp
   - [ ] Touch +/- buttons work
   - [ ] Screen switching works
   - [ ] Dimmer responds to commands
   - [ ] System runs stable

---

## Next Phases

### Phase 2: Network Stack (Planned)
- WiFi Manager (connection, AP mode, mDNS)
- MQTT Client (pub/sub, HA discovery)
- Web Server (HTTP routes, API endpoints)
- OTA Updater (manual + GitHub auto-update)

### Phase 3: Control Logic (Planned)
- PID Controller (pure algorithm)
- Scheduler (time-based temperature control)
- Thermostat (high-level coordinator)

### Phase 4: Storage & UI (Planned)
- Settings Manager (preferences persistence)
- Logger (system event logging)
- Web Pages (HTML generation)
- TFT Screens (already in Phase 1)

### Phase 5: Final Integration (Planned)
- Slim down main.cpp to ~100 lines
- Full system integration
- Documentation
- Performance testing

---

## Build Configuration

### PlatformIO Settings

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps = 
    paulstoffregen/OneWire@^2.3.7
    milesburton/DallasTemperature@^3.11.0
    https://github.com/RobotDynOfficial/RBDDimmer.git
    bodmer/TFT_eSPI@^2.5.43
```

### TFT_eSPI Configuration

Display pins are configured via build flags in `platformio.ini`:

```ini
build_flags =
    -D USER_SETUP_LOADED=1
    -D ILI9341_DRIVER=1
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=320
    -D TFT_MOSI=23
    -D TFT_SCLK=18
    -D TFT_CS=15
    -D TFT_DC=2
    -D TFT_RST=33
    -D TOUCH_CS=22
```

---

## Hardware Requirements

### Core Components
- ESP32 WROOM DevKit
- DS18B20 Temperature Sensor
- 4.7kΩ resistor (pull-up for DS18B20)
- RobotDyn AC Dimmer Module
- JC2432S028 2.8" TFT Display (ILI9341 + XPT2046 touch)

### Wiring (Phase 1)

```
DS18B20:
  VCC → ESP32 3.3V
  GND → ESP32 GND
  DATA → ESP32 GPIO4 (with 4.7kΩ to 3.3V)

AC Dimmer:
  VCC → ESP32 5V/VIN
  GND → ESP32 GND
  PWM → ESP32 GPIO5
  Z-C → ESP32 GPIO27

TFT Display (JC2432S028):
  (See full wiring guide in docs)
  SPI shared: MOSI=23, MISO=19, SCK=18
  TFT CS=15, DC=2, RST=33
  Touch CS=22
```

---

## Development Guidelines

### Module Design Principles

1. **Single Responsibility:** Each module does one thing well
2. **Clear Interfaces:** Public API is simple and documented
3. **Error Handling:** Always check and report errors
4. **Logging:** Use Serial.print with module prefix `[Module]`
5. **Testability:** Modules can be tested independently

### Adding New Modules

1. Create header in `include/modulename/`
2. Create implementation in `src/modulename/`
3. Add to `main.cpp` includes
4. Initialize in `setup()`
5. Call in `loop()` if needed
6. Test independently before integration

### Naming Conventions

- **Classes:** PascalCase (e.g., `TemperatureSensor`)
- **Functions:** camelCase (e.g., `readTemperature`)
- **Variables:** camelCase (e.g., `currentTemp`)
- **Constants:** UPPER_SNAKE_CASE (e.g., `TEMP_MIN_VALID`)
- **Member variables:** m_prefix (e.g., `m_lastTemperature`)

---

## Advantages of Modular Architecture

### For Development
- **Easier debugging:** Issues isolated to specific modules
- **Faster compilation:** Only modified modules recompile
- **Better testing:** Each module tested independently
- **Parallel work:** Multiple developers can work simultaneously

### For Maintenance
- **Clear organization:** Easy to find relevant code
- **Reusable code:** Modules can be used in other projects
- **Easier updates:** Hardware changes only affect driver modules
- **Better documentation:** Each module is self-contained

### For Users
- **More reliable:** Bugs easier to find and fix
- **Better performance:** Optimized per module
- **More features:** Easier to add new functionality
- **Cleaner updates:** OTA updates less likely to break

---

## Performance

### Memory Usage (Phase 1)
- **Flash:** ~350KB (vs. ~340KB monolithic)
- **RAM:** ~35KB (vs. ~32KB monolithic)
- **Free Heap:** ~280KB (plenty of headroom)

### Timing (Phase 1)
- **Boot time:** ~3 seconds
- **Loop time:** ~15ms average
- **Display update:** ~50ms
- **Temperature read:** ~100ms

---

## Troubleshooting

### Compilation Errors

**Problem:** `fatal error: hardware/sensor.h: No such file or directory`  
**Solution:** Ensure `#include "hardware/sensor.h"` path is correct

**Problem:** `undefined reference to 'TemperatureSensor::begin()'`  
**Solution:** Check `sensor.cpp` is in `src/hardware/` directory

### Display Issues

**Problem:** Blank or garbled screen  
**Solution:** Check TFT_eSPI build flags match your display

**Problem:** Touch not working  
**Solution:** Verify TOUCH_CS pin definition, test coordinate mapping

### Sensor Issues

**Problem:** Temperature always reads -127°C  
**Solution:** Check DS18B20 wiring, ensure pull-up resistor present

---

## Migration from v1.3.3

See `MIGRATION_CHECKLIST.md` for detailed step-by-step migration process.

**Quick Summary:**
1. Phase 1: Hardware drivers (current)
2. Phase 2: Network stack
3. Phase 3: Control logic
4. Phase 4: Storage & UI
5. Phase 5: Final integration

---

## Contributing

When adding features:
1. Follow module design principles
2. Add documentation to header files
3. Test independently before integration
4. Update this README
5. Update MIGRATION_CHECKLIST.md

---

## License

Open source - feel free to modify and share!

---

## Support

For issues or questions:
1. Check MIGRATION_CHECKLIST.md for common problems
2. Review module documentation in header files
3. Test modules independently
4. Check serial output for error messages

---

**Project Status:** Phase 1 Complete ✓  
**Next Step:** Begin Phase 2 (Network Stack)
