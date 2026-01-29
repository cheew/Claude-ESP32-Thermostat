# ESP32 Multi-Output Thermostat - Quick Reference

**Version:** 2.3.0 | **Platform:** ESP32 + PlatformIO

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
│   ├── web_server.cpp       # REST API + Web UI + Safety page (~2900 lines)
│   ├── mqtt_manager.cpp     # Home Assistant auto-discovery
│   └── wifi_manager.cpp     # WiFi management
└── utils/
    ├── safety_manager.cpp   # Watchdog, boot loop detection, safe mode
    ├── console.cpp          # Event logging
    ├── logger.cpp           # System logger
    └── temp_history.cpp     # Temperature history

include/                     # All .h header files
platformio.ini               # Build config (ESP32)
```

## Key Files by Task

| Task | Primary File(s) |
|------|-----------------|
| Output control/PID logic | `src/control/output_manager.cpp`, `include/output_manager.h` |
| Safety features | `src/utils/safety_manager.cpp`, `include/safety_manager.h` |
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
- `POST /output/{n}/safety` - Configure safety parameters
- `GET /api/safety/state` - Watchdog/boot loop/safe mode status
- See `ANDROID_APP_INTEGRATION.md` for full API docs

## Current Development Status
- ✅ 3-output control with independent modes
- ✅ Time-proportional control mode (v2.3.0)
- ✅ TFT partial updates - no screen flashing (v2.3.0)
- ✅ Sensor fault detection + safety cutoffs (v2.2.0)
- ✅ Hardware watchdog + boot loop detection (v2.2.1)
- ✅ Safety Settings page with emergency stop (v2.2.1)
- ✅ Web UI (simple + advanced modes, dark mode)
- ✅ MQTT with Home Assistant auto-discovery
- 🔄 TFT display (basic implementation)

## Safety Features (v2.2.1)
- **Hardware Watchdog**: 30-second timeout, auto-reset on hang
- **Boot Loop Detection**: Safe mode after 3 rapid reboots
- **Emergency Stop**: Single-button all outputs OFF
- **Per-output limits**: Max/min temp cutoffs, fault modes
- See `SAFETY_FEATURES.md` for complete documentation

## Roadmap / TODO
See `additions.md` for planned features:
- Stuck heater detection
- Setup wizard
- WebSocket live updates
- Rate-of-change monitoring

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
