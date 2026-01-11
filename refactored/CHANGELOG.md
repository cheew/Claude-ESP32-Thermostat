# Changelog

All notable changes to the ESP32 Multi-Output Thermostat project are documented here.

## [2.1.0] - 2026-01-11

### Added
- **MQTT Multi-Output Support**: Complete MQTT integration for all 3 outputs
  - Separate Home Assistant climate entities for each output
  - Per-output MQTT topics (e.g., `thermostat/output1/temperature`, `thermostat/output2/setpoint`, etc.)
  - Auto-discovery configuration for all 3 climate entities
  - Grouped under single device in Home Assistant
  - Independent control from Home Assistant for each output
  - New `mqtt_publish_all_outputs()` function publishes all outputs every 30 seconds
- **Schedule Page Multi-Output**: Per-output scheduling interface
  - Output selector dropdown to switch between outputs
  - 8 independent schedule slots per output
  - Time-based temperature control (hour, minute, target temp)
  - Day selection for each slot (any combination of days)
  - Visual feedback for active slots (green border)
  - Saves to correct output via REST API
  - Tips section explaining schedule functionality
- **Mobile Responsive Design**: Comprehensive mobile optimizations
  - Media queries for tablet (≤768px), phone (≤480px), and tablet landscape (769-1024px)
  - Touch-friendly controls (44x44px minimum touch targets per Apple/Google guidelines)
  - Font size 16px on inputs to prevent iOS auto-zoom
  - Single column layout on mobile devices
  - 2-column output grid on tablet landscape
  - Compact navigation on small screens
  - No horizontal scrolling required
  - Dark mode compatible on all screen sizes
  - Optimized spacing and padding for small screens

### Changed
- **MQTT Manager**: Rewrote MQTT publishing and subscription logic
  - Updated `mqtt_connect()` to subscribe to all 3 output command topics
  - Modified `mqttCallback()` to route commands to correct output (1, 2, or 3)
  - Replaced single-output `mqtt_publish_status_extended()` with `mqtt_publish_all_outputs()`
  - Rewrote `mqtt_send_ha_discovery()` to create 3 separate climate entities
- **Schedule Page**: Complete redesign from single-output to multi-output
  - JavaScript-based dynamic loading from `/api/output/{id}` endpoint
  - Removed dependency on old scheduler module functions
  - Uses output_manager schedules directly
  - Client-side rendering of schedule slots
- **CSS Styles**: Enhanced with comprehensive media queries
  - Added `box-sizing: border-box` globally for consistent sizing
  - Responsive grid layouts for different screen sizes
  - Touch-optimized button sizes and spacing
  - Active state styling for better touch feedback

### Fixed
- MQTT topics now correctly handle multi-output architecture
- Schedule page now works with per-output schedules instead of global schedule
- Mobile navigation wraps properly on small screens
- Touch targets meet accessibility guidelines

### Memory Usage
- Flash: 89.1% (1,167,717 / 1,310,720 bytes) - ~143 KB remaining
- RAM: 20.2% (66,064 / 327,680 bytes) - ~261 KB free
- Increased from 89.0% flash due to MQTT multi-output code + mobile CSS

### Files Modified
- `src/network/mqtt_manager.cpp` - Multi-output MQTT publishing and subscriptions
- `include/mqtt_manager.h` - Added `mqtt_publish_all_outputs()` declaration
- `src/main.cpp` - Updated to use new MQTT multi-output function
- `src/network/web_server.cpp` - Schedule page rewrite + mobile CSS enhancements

---

## [2.0.0] - 2026-01-11

### Added
- **Multi-Output Architecture**: Support for 3 independent outputs
  - Output 1 (GPIO 5): AC Dimmer for lights
  - Output 2 (GPIO 14): SSR for heat devices
  - Output 3 (GPIO 32): SSR for heat devices
- **Multi-Sensor Support**: Up to 6 DS18B20 temperature sensors
  - Auto-discovery on boot
  - User-friendly naming with persistence
  - Auto-assignment to outputs (1:1 mapping)
- **Output Manager Module**: Centralized output control
  - 5 control modes per output: Off, Manual, PID, On/Off, Schedule
  - Per-output PID tuning
  - Per-output scheduling (8 slots each)
  - Hardware type enforcement (dimmer vs SSR)
  - Device type restrictions (lights vs heat)
- **Sensor Manager Module**: Centralized sensor management
  - Sensor discovery and enumeration
  - Individual sensor naming
  - Bulk read all sensors
  - Error tracking per sensor
- **REST API**: Complete multi-output control
  - `GET /api/outputs` - Get all outputs status
  - `GET /api/output/{id}` - Get single output details
  - `POST /api/output/{id}/control` - Control output (mode, target, power)
  - `POST /api/output/{id}/config` - Configure output (name, sensor, PID, schedule)
  - `GET /api/sensors` - List all sensors
  - `POST /api/sensor/name` - Rename sensor
- **Web Interface Redesign**:
  - **Home Page**: 3-output dashboard with real-time updates
    - Individual output cards with status
    - Quick control buttons (Off, Manual 50%, PID)
    - Animated power bars
    - Auto-refresh every 2 seconds
  - **Outputs Page**: Full configuration interface
    - Tab-based switching between outputs
    - Sensor assignment dropdown
    - PID parameter tuning
    - Enable/disable toggles
  - **Sensors Page**: Sensor management
    - Live temperature readings
    - Rename functionality
    - ROM address display
    - Auto-refresh every 3 seconds

### Changed
- **Core Architecture**: Refactored from single-output to multi-output
  - Replaced old modules: `pid_controller`, `temp_sensor`, `dimmer_control`, `system_state`
  - Unified control via `output_manager` and `sensor_manager`
- **Configuration Storage**: Per-output preferences
  - Separate namespace for each output (output1, output2, output3)
  - Sensor names stored in separate namespace
- **Control Loop**: Updated to handle all 3 outputs
  - Sensors read every 2 seconds
  - Outputs updated every 100ms
  - Web refresh every 2 seconds
  - MQTT publish every 30 seconds

### Fixed
- Build errors with `MAX_SCHEDULE_SLOTS` redefinition
- `ScheduleSlot_t` type conflicts
- `SystemState_t` initialization issues
- String assignment errors in legacy compatibility layer
- Mode buffer overflow (increased from 8 to 12 bytes)

### Breaking Changes
- Old single-output API endpoints deprecated
- Preferences structure changed (settings reset on upgrade)
- MQTT topic structure incompatible with v1.x (see v2.1.0 for full multi-output MQTT)

### Memory Usage
- Flash: 89.0% (1,166,825 / 1,310,720 bytes)
- RAM: 20.2% (66,172 / 327,680 bytes)

### Migration Notes
- Existing settings will be lost on upgrade to v2.0.0
- Reconfigure device name, WiFi, MQTT after upgrade
- Reassign sensors to outputs
- Old MQTT topics will not work (update to v2.1.0 for full MQTT support)

---

## [1.9.1] - 2026-01-11

### Fixed
- **WiFi Reconnection Bug**: Device now automatically reconnects to WiFi after connection loss, even when in AP mode
  - Previously, device would stay in AP mode indefinitely after WiFi loss
  - Now attempts reconnection every 30 seconds when in AP mode
  - Modified [wifi_manager.cpp](src/network/wifi_manager.cpp) `wifi_task()` function

### Changed
- Updated firmware version to 1.9.1 in [main.cpp](src/main.cpp)

---

## [1.9.0] - 2026-01-11

### Added
- **Dark Mode**: System-aware theme with manual toggle button in navigation
  - Auto-detects system preference (prefers-color-scheme)
  - Persists preference in localStorage
  - Smooth transitions between themes
- **Live Console**: Real-time system event logging
  - 8 event types: SYSTEM, MQTT, WIFI, TEMP, PID, SCHEDULE, ERROR, DEBUG
  - Color-coded messages
  - Auto-refresh every 2 seconds
  - 50-event circular buffer
  - New `/console` page
- **Temperature History**: 24-hour temperature graphing
  - 288 samples @ 5-minute intervals
  - Chart.js visualization
  - JSON API endpoint: `/api/history`
- **Extended MQTT Status**: Added WiFi RSSI, free heap, uptime metrics
  - 3 new Home Assistant diagnostic sensors
  - Uptime breakdown (days/hours/minutes)
- **Enhanced REST API**: New endpoints for monitoring
  - `/api/info` - Device information
  - `/api/logs` - System logs as JSON
  - `/api/console` - Console events
  - `/api/console-clear` - Clear console
  - `/api/control` - Unified control endpoint

### Documentation
- Created [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) - Complete architecture documentation
- Created [CONSOLE_IMPLEMENTATION.md](CONSOLE_IMPLEMENTATION.md) - Console feature details

---

## [1.8.0] - 2026-01-11

### Added
- Live console feature (initial implementation)
- Console event buffer module
- MQTT activity logging

---

## [1.7.1] - 2026-01-11

### Added
- Extended MQTT status publishing
- Home Assistant diagnostic sensors (WiFi signal, free memory, uptime)

---

## [1.7.0] - 2026-01-11

### Added
- API enhancement: /api/info, /api/logs, /api/control endpoints
- Unified control interface

---

## [1.6.0] - 2026-01-11

### Added
- Temperature history tracking (24 hours)
- Temperature graph visualization (Chart.js)
- History API endpoint

---

## [1.5.0] - 2026-01-11

### Changed
- **Complete Modular Refactoring**: Phases 2-6 complete
  - Phase 2: Network stack (WiFi, MQTT, Web Server)
  - Phase 3: Control logic (PID, State, Scheduler)
  - Phase 4: TFT Display
  - Phase 5: Hardware abstraction (temp sensor, dimmer)
  - Phase 6: Utilities (logger module)

### Architecture
- Migrated from monolithic to modular C architecture
- Functional programming style (C-style functions)
- Clear module boundaries
- Improved maintainability

---

## [1.4.0] - 2026-01-10

### Added
- Initial modular architecture implementation
- Hardware driver modules

---

## [1.3.3] - Previous

### Features
- Basic thermostat functionality
- PID temperature control
- Web interface
- MQTT integration
- Temperature scheduling
- TFT display support

---

## Format

**Version Format**: MAJOR.MINOR.PATCH

- **MAJOR**: Significant architecture changes or breaking changes
- **MINOR**: New features, non-breaking changes
- **PATCH**: Bug fixes, minor improvements

**Categories**:
- **Added**: New features
- **Changed**: Changes to existing functionality
- **Deprecated**: Soon-to-be removed features
- **Removed**: Removed features
- **Fixed**: Bug fixes
- **Security**: Security fixes
