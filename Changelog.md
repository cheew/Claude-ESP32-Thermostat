# Changelog

All notable changes to the ESP32 Multi-Output Thermostat project are documented here.

## [2.5.1] - 2026-02-07

### Fixed
- **MQTT Naming Conflicts**: Each device now uses unique MQTT client IDs, base topics, and Home Assistant device IDs derived from the chip ID to prevent disconnects when multiple thermostats run on the same broker.

---

## [2.5.0] - 2026-02-07

### Added
- **ESP32-S3 Migration**: Upgraded from ESP32 to ESP32-S3-WROVER-1 N16R8
  - 16MB flash (was 4MB), 8MB PSRAM
  - Custom partition table: 4MB app0 + 4MB app1 (OTA) + 8MB LittleFS
  - LittleFS filesystem for static files, profiles, and data caching
- **Animal Habitat Profiles**: JSON-based species profiles stored in LittleFS
  - 5 built-in profiles: Bearded Dragon, Ball Python, Leopard Gecko, Crested Gecko, Corn Snake
  - Profile manager module (`profile_manager.h/cpp`) with load/apply/list API
  - Auto-generates 7-slot schedule (Night/Dawn/Morning/Basking/Afternoon/Evening/Dusk)
  - Active profile persisted in Preferences across reboots
- **Setup Wizard**: 7-step first-boot configuration wizard served from LittleFS
  - Welcome, Animal Selection, Climate Preset, Season Mode, Output Config, Network, Review
  - Visual animal card grid with temperature preview
  - Auto-redirect on first boot, re-run button in Settings
  - Green color scheme matching main UI
- **Weather Integration**: OpenWeatherMap outdoor weather data
  - Current conditions API + 5-day/3-hour forecast API (8 entries, 24h coverage)
  - Timezone-aware matching: forecast entries use remote city local time
  - LittleFS cache (`/cache/weather.json`, `/cache/weather_hist.json`) for offline resilience
  - Weather-adjusted targets shift +/- 1.5C within animal profile bounds
  - Configurable fetch interval (1-6 hours)
- **Weather Sync Control Mode**: New `CONTROL_MODE_WEATHER` (mode 6)
  - Standalone control mode deriving target from profile midpoint adjusted by outdoor forecast
  - PID control with weather-adjusted target temperature
  - 24-hour forecast graph on Outputs page (HTML5 Canvas)
  - Mini forecast graph on Simple mode home page
  - Weather status section on Info page (outdoor temp, conditions, fetch age, forecast count)
  - Weather mode notice banner on Schedule page
- **Help System**: Contextual help modals (? buttons) on Outputs, Sensors, and Schedule pages
  - Dark mode compatible overlay with close-on-click-outside behavior
- **Schedule System Rebuild**: Complete rewrite of scheduling
  - 12 slots per output (was 8)
  - Day-of-week filtering (SMTWTFS format)
  - Temperature ramping between adjacent slots
  - Labels per slot ("Dawn", "Basking Peak", "Night")
  - CSV import/export
  - 24-hour schedule graph on Schedule page (live-updating Canvas)
  - Schedule mini-graphs in Simple mode per-output cards

### Changed
- Firmware version updated to 2.5.0
- Board target changed to `esp32-s3-devkitc-1` in platformio.ini
- GPIO 25 reassigned to GPIO 6 (unavailable on S3)
- Weather API endpoint now includes forecast entries array
- `is_syncing_output()` accepts both Schedule and Weather control modes

### Fixed
- **Dark Mode Simple Home Screen**: Fixed remaining dark mode issues on simple mode cards
- **Real-Time Clock Timestamps**: Logs and console now use NTP-synced timestamps with uptime fallback
- **Schedule Save/Load**: days, rampToNext, and label fields now properly persisted
- **Schedule API**: Accepts both "targetTemp" and "target" field names
- **Schedule JSON Buffer**: DynamicJsonDocument(4096) prevents truncation

### UI Improvements
- Animal symbol displayed in header when a profile is active
- Weather sync indicator per output showing when weather influences target
- Re-run wizard button in Settings page
- Schedule page always visible in both Simple and Advanced UI modes

### Files Added
- `include/profile_manager.h` - Profile manager header
- `src/control/profile_manager.cpp` - Profile manager implementation
- `include/weather_client.h` - Weather client header
- `src/network/weather_client.cpp` - Weather client implementation
- `data/profiles/*.json` - 5 animal habitat profiles
- `data/wizard/wizard.html` - Setup wizard page
- `data/wizard/wizard.css` - Setup wizard styles
- `data/wizard/wizard.js` - Setup wizard logic
- `partitions.csv` - Custom partition table for ESP32-S3

### Files Modified
- `include/config.h` - ESP32-S3 GPIO assignments
- `include/display_manager.h` - S3 pin updates
- `include/output_manager.h` - Added `CONTROL_MODE_WEATHER` enum
- `platformio.ini` - ESP32-S3 board config, LittleFS, partition table
- `src/control/output_manager.cpp` - Weather mode control logic
- `src/main.cpp` - Profile/weather init, LittleFS setup
- `src/network/web_server.cpp` - Help modals, weather graphs, forecast API, wizard support
- `src/network/wifi_manager.cpp` - S3 compatibility
- `src/utils/console.cpp` - NTP timestamps
- `src/utils/logger.cpp` - NTP timestamps
- `src/utils/safety_manager.cpp` - S3 compatibility

### Memory Usage
- Flash: 30.7% (1,286,853 / 4,194,304 bytes) - ~2.9 MB remaining
- RAM: 20.9% (68,484 / 327,680 bytes) - ~259 KB free

---

## [2.4.0] - 2026-02-04

### Added
- **Cloud MQTT Broker**: HiveMQ Cloud support with TLS encryption (port 8883)
  - WiFiClientSecure with ISRG Root X1 CA certificate
  - Cloud broker configuration in Settings page (host, port, user, password)
  - Independent telemetry publishing to cloud namespace
  - Remote setpoint and mode control via cloud topics
  - Last Will and Testament (LWT) for online/offline status
- **Login Brute Force Protection**: Progressive lockout on failed PIN attempts
  - 3 failures: 30-second lockout
  - 5 failures: 2-minute lockout
  - 10 failures: 10-minute lockout
  - Lockout status shown on login page and API (HTTP 429)
- **Session Expiry**: 1-hour session TTL with automatic invalidation
- **API Authentication**: Control endpoints (`/api/set`, `/api/control`) now require authentication
- **Setup Wizard Spec**: `SETUP_WIZARD.md` - living document tracking all wizard features

### Changed
- Session tokens now generated using hardware RNG (`esp_random`) instead of `random()`
- MQTT publishing refactored: local and cloud brokers share timing loop with independent connections

### Fixed
- **Dark Mode Flash (FOUC)**: Eliminated flash of light mode when navigating between pages
  - Moved dark mode initialization from `body onload` to inline `<script>` in `<head>`
  - Dark mode class applied to `<html>` element (renders before body)
  - CSS selectors updated from `body.dark-mode` to `.dark-mode`
  - Toggle function updated to use `document.documentElement`

### Security
- Hardware RNG for session tokens (cryptographically stronger)
- Brute force protection prevents PIN enumeration
- Session expiry limits window of stolen session reuse
- API endpoints enforce authentication

### Files Added
- `SETUP_WIZARD.md` - Setup wizard feature checklist
- `PINOUT_WIRING.md` - Hardware wiring documentation
- `mqtt_security.md` - MQTT security notes

### Files Modified
- `include/config.h` - Version bump to 2.4.0
- `include/mqtt_manager.h` - Cloud MQTT function declarations
- `src/main.cpp` - Cloud MQTT init/task, shared publish timing
- `src/network/mqtt_manager.cpp` - Cloud MQTT implementation (TLS, publish, subscribe, callbacks)
- `src/network/web_server.cpp` - Brute force protection, session expiry, cloud MQTT settings UI, dark mode FOUC fix, API auth

### Memory Usage
- Flash: 94.4% (1,236,669 / 1,310,720 bytes) - ~74 KB remaining
- RAM: 20.3% (66,572 / 327,680 bytes) - ~261 KB free

---

## [2.3.1] - 2026-01-30

### Fixed
- **Schedule Page Bug**: Fixed JavaScript error preventing schedule from loading when selecting different outputs
  - Issue: `days` field returned as `undefined` from API, causing `indexOf` error
  - Solution: Ensured `days` field is always a valid string in both API response and JavaScript
  - Schedule slots now properly display when changing output selection
- **Dark Mode Readability**: Comprehensive dark mode improvements across all pages
  - Added CSS overrides for inline styles (panels, cards, help text)
  - Made output cards dark-mode aware (heating: dark red, not heating: dark green)
  - Made schedule slots and day selectors dark-mode aware
  - Fixed light backgrounds on: output selector box, config panels, info boxes
  - All text now readable in dark mode (no more dark text on dark backgrounds)

### Changed
- JavaScript-generated elements (output cards, schedule slots) now detect and adapt to dark mode
- Improved contrast ratios for better accessibility in dark mode

### Files Modified
- `src/network/web_server.cpp` - Dark mode CSS rules, JavaScript dark mode detection, schedule bug fix

---

## [2.3.0] - 2026-01-17

### Added
- **Time-Proportional Control Mode**: New control mode for equipment-friendly temperature control
  - Converts PID output (0-100%) into timed ON/OFF cycles
  - Configurable cycle time (5-120 seconds, default 30s)
  - Configurable minimum ON/OFF times to prevent rapid cycling
  - Longer cycles reduce relay wear and are better for heaters
  - PID runs continuously (~100ms) for responsive temperature tracking
  - Cycle timer runs independently for stable ON/OFF switching
  - Added to web UI mode dropdown and TFT display mode cycling
- **4-Channel SSR GPIO Wiring Schedule**: Documented GPIO assignments for 4-channel high-trigger SSR modules
  - GPIO 16, 17, 26, 12 identified as suitable output pins
  - Avoids strapping pins and input-only GPIOs

### Changed
- **TFT Display Partial Updates**: Eliminated screen flashing during updates
  - Only redraws elements that have actually changed
  - Tracks previous values for temperature, target, power, heating state, and mode
  - Implemented `drawMainScreenPartial()` and `drawControlScreenPartial()` functions
  - Background only cleared on full screen transitions
- Updated `output_manager_get_mode_name()` to return "TimeProp" and "OnOff" (no hyphens) for dropdown matching

### Files Modified
- `include/output_manager.h` - Added `CONTROL_MODE_TIME_PROP` enum, time-prop struct fields, `output_manager_set_time_prop_params()` declaration
- `src/control/output_manager.cpp` - Implemented `updateTimeProp()`, `resetTimePropState()`, load/save for time-prop params
- `src/network/web_server.cpp` - Time-prop mode in dropdowns, config section with cycle time inputs, API handling
- `src/hardware/display_manager.cpp` - Partial screen update functions, change tracking variables
- `src/main.cpp` - Added timeprop to display mode callback

---

## [2.2.1] - 2026-01-17

### Added
- **Hardware Watchdog Timer**: ESP32 task watchdog for system reliability
  - 30-second timeout with automatic reset on hang
  - Prevents frozen state with heater stuck ON
  - Fed at start of each main loop iteration
- **Boot Loop Detection & Safe Mode**: Protection against crash loops
  - Tracks boot count in NVS flash
  - Enters SAFE MODE after 3 rapid reboots
  - All outputs forced OFF in safe mode
  - Web UI shows safe mode banner with exit option
  - Boot marked stable after 60 seconds of normal operation
- **Safety Settings Page**: Dedicated UI for safety configuration (`/safety`)
  - System safety status display (watchdog, boot count, safe mode)
  - Emergency Stop button (all outputs OFF immediately)
  - Per-output safety parameter configuration
  - Real-time fault status with color coding
  - Fault analysis table with all safety-related data
  - Manual fault clear with validation
  - Auto-refresh every 5 seconds
- **Safety Manager Module**: New `safety_manager.cpp/.h` module
  - Centralized safety system management
  - Functions: init, feed_watchdog, mark_stable, emergency_stop
  - Safe mode entry/exit with NVS persistence
- **Safety API Endpoints**:
  - `GET /api/safety/state` - Get safety system state
  - `POST /api/output/{n}/safety` - Update output safety settings
  - `POST /api/safety/emergency-stop` - Emergency stop all outputs
  - `POST /api/safety/exit-safe-mode` - Exit safe mode
- **SAFETY_FEATURES.md**: Comprehensive safety documentation
  - Documents all implemented safety mechanisms
  - Future work section for planned features
  - API reference for safety endpoints

### Changed
- Navigation bar now includes Safety link (always visible)
- Safety Settings UI Page marked as implemented in documentation

### Files Added
- `include/safety_manager.h` - Safety manager header
- `src/utils/safety_manager.cpp` - Safety manager implementation
- `SAFETY_FEATURES.md` - Safety documentation

### Files Modified
- `src/main.cpp` - Watchdog integration, stable boot marking
- `src/network/web_server.cpp` - Safety page + API endpoints + nav link

### Memory Usage
- Flash: 93.1% (1,220,061 / 1,310,720 bytes) - ~90 KB remaining
- RAM: 20.2% (66,284 / 327,680 bytes) - ~261 KB free

---

## [2.2.0] - 2026-01-16

### Added
- **Sensor Fault Detection**: Per-output sensor health monitoring
  - Tracks sensor states: OK, STALE (no update within threshold), ERROR (invalid readings)
  - Configurable fault timeout per output
  - Fault mode options: OFF, HOLD_LAST, CAP_POWER
  - Automatic failsafe state when sensor issues detected
- **Hard Temperature Cutoffs**: Safety limits that override all control modes
  - Per-output max_temp_c and min_temp_c limits
  - OVER_TEMP fault forces output OFF regardless of mode
  - Works even when schedule or PID requests heating
- **Health API Endpoint**: `/api/v1/health` for system monitoring
  - Sensor status per output
  - Fault state reporting
  - Uptime and memory metrics

### Changed
- **Project Structure Cleanup**: Consolidated documentation for efficiency
  - Created `START_HERE.md` - concise quick reference (~80 lines)
  - Moved old docs to `Archive/docs/` (SESSION_SUMMARY, QUICK_START, etc.)
  - Moved release notes to `Archive/releases/`
  - Moved deprecated code to `Archive/deprecated/`
  - Deleted junk files (Windows artifacts, empty folders)
- **Version Sync**: Unified version numbers across all files
  - main.cpp, config.h, display_manager.cpp, mqtt_manager.cpp, wifi_manager.cpp

### Files Modified
- `src/main.cpp` - Version update, header comments
- `src/control/output_manager.cpp` - Sensor fault handling, safety cutoffs
- `include/output_manager.h` - Fault state enums, config structs
- `src/network/web_server.cpp` - Health endpoint
- `src/hardware/display_manager.cpp` - Version in splash screen
- `include/config.h` - Version constant
- `src/network/mqtt_manager.cpp` - MQTT device version
- `src/network/wifi_manager.cpp` - mDNS version

---

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
