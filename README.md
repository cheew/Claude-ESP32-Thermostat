# ESP32 Multi-Output Thermostat

**Version:** 2.2.0 (dev)
**Status:** Feature complete - TFT Display, Security, Simple/Advanced modes
**Architecture:** Modular C (functional style)

---

## Features

### Core Functionality
- **3 Independent Outputs** - Each with dedicated sensor, mode, and schedule
- **Multiple Control Modes** - PID, Manual, On/Off, Schedule
- **TFT Touch Display** - 2.8" ILI9341 with touch controls
- **Web Interface** - Simple mode (dashboard) + Advanced mode (full config)
- **PIN Security** - Optional authentication for settings/control
- **MQTT Integration** - Home Assistant auto-discovery (3 climate entities)
- **REST API** - Full control via JSON endpoints

### Display Features (TFT)
- 3-output status dashboard
- Touch temperature adjustment (+/- 0.5C)
- Mode selection screen
- Info screen (IP, WiFi, MQTT status)
- Auto-refresh every 2 seconds

### Web Interface
- **Simple Mode:** Clean 3-card dashboard with sliders and dropdowns
- **Advanced Mode:** Full navigation (Outputs, Sensors, Schedule, History, etc.)
- Dark mode support
- Mobile responsive design

### Security
- Optional PIN protection (4-6 digits)
- Session cookie authentication
- Protected: Settings, Output control, Firmware upload
- Public: Status, Sensor readings (read-only)

---

## Project Structure

```
refactored/
├── platformio.ini              # Build configuration
├── README.md                   # This file
├── SCOPE.md                    # Feature roadmap
├── ANDROID_APP_INTEGRATION.md  # Android app API guide
├── START_HERE_TOMORROW.md      # Quick start for next session
│
├── include/                    # Header files
│   ├── output_manager.h        # Multi-output control
│   ├── sensor_manager.h        # DS18B20 sensor management
│   ├── display_manager.h       # TFT display control
│   ├── wifi_manager.h
│   ├── mqtt_manager.h
│   ├── web_server.h
│   └── ...
│
└── src/                        # Implementation files
    ├── main.cpp                # Main program
    ├── network/                # Network modules
    │   ├── wifi_manager.cpp
    │   ├── mqtt_manager.cpp
    │   └── web_server.cpp      # Web UI + Security + API
    ├── control/                # Control logic
    │   ├── output_manager.cpp  # 3-output management
    │   └── sensor_manager.cpp  # Multi-sensor support
    └── hardware/               # Hardware abstraction
        ├── display_manager.cpp # TFT + touch
        └── dimmer_control.cpp
```

---

## Hardware

### Core Components
- ESP32 WROOM DevKit
- DS18B20 Temperature Sensors (up to 6)
- RobotDyn AC Dimmer Module
- 2.8" ILI9341 TFT Display with XPT2046 touch

### Pin Configuration

```
DS18B20 Sensors (OneWire bus):
  DATA → GPIO 4 (with 4.7k pull-up to 3.3V)

AC Dimmer (Output 1):
  PWM  → GPIO 5
  Z-C  → GPIO 27

TFT Display:
  MOSI → GPIO 23
  SCLK → GPIO 18
  CS   → GPIO 15
  DC   → GPIO 2
  RST  → GPIO 33

Touch Screen (XPT2046):
  T_CS → GPIO 22
  (shares MOSI/SCLK with display)
```

---

## Quick Start

### Build and Upload
```bash
cd "c:\Users\admin\Documents\PlatformIO\Projects\Claude-ESP32-Thermostat\refactored"
pio run -t upload
```

### Access Web Interface
- mDNS: `http://havoc.local/`
- Direct IP: `http://192.168.1.236/`
- AP mode (if WiFi fails): `http://192.168.4.1/`

### API Examples
```bash
# Get all outputs status
curl http://havoc.local/api/outputs

# Set temperature
curl -X POST http://havoc.local/api/output/1/control \
  -H "Content-Type: application/json" \
  -d '{"target": 25.0, "mode": "pid"}'

# Check secure mode status
curl http://havoc.local/api/status
```

---

## Web Interface Modes

### Simple Mode (Default)
Clean dashboard for everyday use:
- 3 output cards with large temperature display
- Target temperature slider (15-35C)
- Mode dropdown (Off/Manual/PID/On-Off)
- Power slider when in Manual mode
- Real-time updates every 3 seconds

### Advanced Mode
Full configuration access:
- Home, Outputs, Sensors, Schedule, History, Info, Logs, Console, Settings
- PID tuning parameters
- Sensor assignment
- Schedule configuration (8 slots per output)

Switch modes via the orange dropdown in the navigation bar.

---

## Security

### Enable PIN Protection
1. Go to Settings page
2. Check "Enable PIN Protection"
3. Enter 4-6 digit PIN
4. Save settings

### Protected Routes
- `/settings` - Device configuration
- `/outputs` - Output configuration
- `/api/output/*/control` - Temperature/mode changes
- `/api/restart` - Restart device
- `/api/upload` - Firmware upload

### Android App Authentication
See [ANDROID_APP_INTEGRATION.md](ANDROID_APP_INTEGRATION.md) for API login flow.

---

## MQTT / Home Assistant

### Auto-Discovery
Device publishes Home Assistant discovery config for:
- 3 climate entities (one per output)
- Temperature sensors
- Diagnostic sensors (WiFi RSSI, uptime, etc.)

### Topics
```
thermostat/havoc/output1/temperature
thermostat/havoc/output1/setpoint
thermostat/havoc/output1/mode
thermostat/havoc/output1/power
(repeat for output2, output3)
```

---

## Memory Usage

| Resource | Used | Total | Percent |
|----------|------|-------|---------|
| Flash | 1,192 KB | 1,310 KB | 90.9% |
| RAM | 66 KB | 328 KB | 20.2% |

---

## Development

### Adding New Features
1. Follow modular design (one module = one responsibility)
2. Create header in `include/`, implementation in `src/`
3. Initialize in `setup()`, call in `loop()` if needed
4. Update README and SCOPE.md

### Naming Conventions
- Functions: `module_function_name()` (snake_case)
- Types: `TypeName_t` (PascalCase with _t suffix)
- Constants: `CONSTANT_NAME` (UPPER_SNAKE_CASE)

---

## Troubleshooting

### TFT Display Issues
- **Blank screen:** Check SPI wiring, verify TFT_eSPI build flags
- **Touch not working:** Verify TOUCH_CS pin, check touch calibration
- **Wrong touch coordinates:** DO NOT modify the touch mapping code

### Sensor Issues
- **Reads -127C:** Check wiring, ensure pull-up resistor present
- **Sensor not found:** Power cycle, check OneWire bus

### Web Interface Issues
- **Can't access:** Check WiFi connection, try direct IP
- **Login required:** Enter PIN or disable secure mode in Settings

---

## Documentation

- [SCOPE.md](SCOPE.md) - Feature roadmap and specifications
- [ANDROID_APP_INTEGRATION.md](ANDROID_APP_INTEGRATION.md) - Android app API guide
- [START_HERE_TOMORROW.md](START_HERE_TOMORROW.md) - Quick start for development
- [CHANGELOG.md](CHANGELOG.md) - Version history

---

## License

Open source - feel free to modify and share!

---

## Support

1. Check `/console` page for debug messages
2. Review documentation files
3. Open an issue on GitHub: https://github.com/cheew/Claude-ESP32-Thermostat
