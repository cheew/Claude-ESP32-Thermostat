# ESP32 Reptile Thermostat - Modular Architecture

**Version:** 1.9.0
**Status:** Modular refactoring complete (Phases 2-6)
**Architecture:** Modular C (functional style)

---

## Project Structure

```
refactored/
├── platformio.ini           # Build configuration
├── README.md                # This file
├── SCOPE.md                 # Future features and roadmap
├── PROJECT_STRUCTURE.md     # Detailed architecture documentation
│
├── include/                 # Header files
│   ├── wifi_manager.h
│   ├── mqtt_manager.h
│   ├── web_server.h
│   ├── pid_controller.h
│   ├── system_state.h
│   ├── scheduler.h
│   ├── temp_sensor.h
│   ├── dimmer_control.h
│   ├── logger.h
│   ├── temp_history.h
│   └── console.h
│
└── src/                     # Implementation files
    ├── main.cpp            # Main program (minimal coordination)
    ├── network/            # Network modules
    │   ├── wifi_manager.cpp
    │   ├── mqtt_manager.cpp
    │   └── web_server.cpp
    ├── control/            # Control logic
    │   ├── pid_controller.cpp
    │   ├── system_state.cpp
    │   └── scheduler.cpp
    ├── hardware/           # Hardware abstraction
    │   ├── temp_sensor.cpp
    │   └── dimmer_control.cpp
    └── utils/              # Utilities
        ├── logger.cpp
        ├── temp_history.cpp
        └── console.cpp
```

---

## Current Features

### Core Functionality
- **Temperature Control:** PID-based heating control with DS18B20 sensor
- **Multiple Modes:** Auto (PID), Manual On/Off, Schedule-based
- **Web Interface:** Full control and monitoring via browser
- **MQTT Integration:** Home Assistant auto-discovery and control
- **Temperature Scheduling:** 6 programmable time slots per day
- **Dark Mode:** System-aware theme with toggle button
- **Live Console:** Real-time event logging (MQTT, WiFi, temp, PID)
- **Temperature History:** 24-hour graphing (288 samples @ 5min intervals)

### Network & Connectivity
- **WiFi Manager:** Automatic connection with AP fallback
- **mDNS Support:** Access via hostname.local
- **MQTT Client:** Publish/subscribe with extended status
- **REST API:** JSON endpoints for device info, logs, history, control
- **Web Server:** Responsive HTML interface with multiple pages

### Monitoring & Debugging
- **System Logs:** Timestamped event logging
- **Live Console:** Real-time system events with color coding
- **Temperature Graphs:** Chart.js visualization of history
- **MQTT Status:** Extended metrics (WiFi RSSI, free heap, uptime)
- **Home Assistant:** Diagnostic sensors for system health

### User Interface
- **Web Pages:** Home, Schedule, Temperature, Info, Logs, Console, Settings
- **Dark Mode:** Automatic system preference detection + manual toggle
- **Responsive Design:** Works on desktop and mobile browsers
- **Auto-refresh:** Console updates every 2 seconds

---

## Next Steps & Future Enhancements

**See [SCOPE.md](SCOPE.md) for detailed roadmap and feature specifications.**

### Priority Features:

1. **WiFi Reconnection Fix** (Critical)
   - Device currently stays in AP mode after WiFi loss
   - Should automatically reconnect when WiFi becomes available
   - Simple fix in [wifi_manager.cpp](src/network/wifi_manager.cpp)

2. **Multi-Output Environmental Control**
   - Expand from 1 to 3 independent heating/lighting outputs
   - Support for AC dimmer, SSR pulse control, and relay on/off
   - Multiple DS18B20 sensors (assign per output)
   - Independent scheduling per output

3. **Setup Wizard**
   - Guided first-run configuration
   - WiFi setup with network scanning
   - MQTT configuration (optional)
   - Sensor discovery and naming
   - Output hardware configuration

4. **Android App**
   - mDNS device discovery
   - Multi-device management
   - Real-time monitoring and control
   - Temperature graphs and alerts

### Testing Current Version

**Build and upload:**
```bash
pio run -t upload
pio device monitor
```

**Access web interface:**
- Local network: `http://192.168.1.236`
- mDNS hostname: `http://havoc.local`
- AP mode (if WiFi fails): `http://192.168.4.1` (SSID: ReptileThermostat)

**Verify functionality:**
- Temperature readings update every 2 seconds
- Web interface accessible on all pages
- Dark mode toggle works
- Console shows real-time events
- Temperature graph displays 24-hour history
- MQTT publishes to Home Assistant (if configured)
- Schedule can be edited and saved

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

- **Functions:** snake_case with module prefix (e.g., `wifi_connect`, `temp_sensor_read`)
- **Types:** PascalCase with _t suffix (e.g., `WiFiState_t`, `ConsoleEventType_t`)
- **Variables:** camelCase (e.g., `currentTemp`)
- **Constants:** UPPER_SNAKE_CASE (e.g., `TEMP_MIN_VALID`)
- **Static variables:** camelCase with static keyword (e.g., `static int eventCount`)

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

## Known Issues

### WiFi Reconnection Bug
**Issue:** Device does not automatically reconnect to WiFi after connection loss. It stays in AP mode even when WiFi becomes available again.

**Workaround:** Power cycle the device.

**Fix:** See [SCOPE.md - Known Issues](SCOPE.md#1-wifi-reconnection-bug) for detailed fix plan.

---

## Contributing

When adding features:
1. Follow module design principles (see [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md))
2. Add documentation to header files
3. Test independently before integration
4. Update this README and SCOPE.md
5. Commit with descriptive messages

---

## License

Open source - feel free to modify and share!

---

## Support

For issues or questions:
1. Check [SCOPE.md](SCOPE.md) for known issues and workarounds
2. Review module documentation in header files
3. Check [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) for architecture details
4. Check serial output and `/console` page for debug messages
5. Open an issue on GitHub

---

**Project Status:** Modular refactoring complete ✓
**Current Version:** v1.9.0
**Next Steps:** See [SCOPE.md](SCOPE.md) for roadmap
