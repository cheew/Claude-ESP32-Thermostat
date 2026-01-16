# ESP32 Multi-Output Thermostat - Quick Reference

**Version:** 2.2.0-dev | **Platform:** ESP32 + PlatformIO

## What This Is
Reptile habitat controller with 3 independent heating outputs, TFT touchscreen, web UI, MQTT/Home Assistant integration.

## Project Structure
```
src/
├── main.cpp              # Entry point, setup/loop
├── control/
│   ├── output_manager.cpp   # 3-output control logic, PID, safety (960 lines)
│   └── system_state.cpp     # State management
├── hardware/
│   ├── display_manager.cpp  # ILI9341 TFT + touch (704 lines)
│   └── sensor_manager.cpp   # DS18B20 temperature sensors
├── network/
│   ├── web_server.cpp       # REST API + Web UI (2557 lines - largest file)
│   ├── mqtt_manager.cpp     # Home Assistant auto-discovery
│   └── wifi_manager.cpp     # WiFi management
└── utils/                   # Console, logger, temp history

include/                     # All .h header files
platformio.ini               # Build config (ESP32)
```

## Key Files by Task

| Task | Primary File(s) |
|------|-----------------|
| Output control/PID logic | `src/control/output_manager.cpp`, `include/output_manager.h` |
| Web UI / REST API | `src/network/web_server.cpp` |
| TFT display | `src/hardware/display_manager.cpp` |
| Temperature sensors | `src/hardware/sensor_manager.cpp` |
| MQTT / Home Assistant | `src/network/mqtt_manager.cpp` |
| Hardware pins | `include/config.h` |
| System state structs | `include/system_state.h` |

## Hardware
- **MCU:** ESP32-WROOM-32
- **Display:** ILI9341 320x240 TFT + XPT2046 touch
- **Sensors:** DS18B20 (3x) on OneWire bus (GPIO 4)
- **Outputs:** GPIO 5 (AC dimmer), GPIO 14 (SSR pulse), GPIO 32 (relay on/off)

## API Endpoints (prefix: `/api/v1/`)
- `GET /status` - Full system state
- `GET /health` - System health + sensor status
- `POST /output/{n}/setpoint` - Set temperature target
- `POST /output/{n}/mode` - Change mode (off/manual/pid/schedule)
- See `ANDROID_APP_INTEGRATION.md` for full API docs

## Current Development Status
- ✅ 3-output control with independent modes
- ✅ Sensor fault detection + safety cutoffs (v2.2.0)
- ✅ Web UI (simple + advanced modes, dark mode)
- ✅ MQTT with Home Assistant auto-discovery
- 🔄 TFT display (basic implementation)

## Roadmap / TODO
See `additions.md` for planned features:
- Stuck heater detection
- Setup wizard
- WebSocket live updates
- Event logging system

## Build
```bash
pio run              # Build
pio run -t upload    # Flash to ESP32
pio device monitor   # Serial console
```

## Archived Files
Old documentation and deprecated code moved to `Archive/` folder:
- `Archive/docs/` - SESSION_SUMMARY, QUICK_START, etc.
- `Archive/releases/` - Release notes v2.1.0, v2.1.1
- `Archive/deprecated/` - Old tft_display.* files
- `Archive/Phase 2 files/` - Previous architecture (v1.4.0)
