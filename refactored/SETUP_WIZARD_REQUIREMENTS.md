# Setup Wizard - Requirements Specification

**Document Version:** 1.0
**Created:** January 24, 2026
**Project Version:** v2.3.0
**Status:** Draft - Requirements Gathering

---

## 🎯 Overview

The Setup Wizard provides a guided first-run experience for configuring the ESP32 thermostat. It simplifies the initial setup process and ensures all critical components are properly configured before operation.

### Goals
- Reduce setup time from 30+ minutes to under 10 minutes
- Eliminate configuration errors through validation
- Guide users through sensor discovery and output assignment
- Ensure safety features are configured
- Provide a professional first-run experience
- Support both web interface and TFT display (future)

---

## 🚀 Entry Points / Triggers

### Automatic Triggers
1. **First Boot** - No WiFi credentials saved in preferences
2. **Factory Reset** - User initiates factory reset from settings
3. **Safe Mode** - After boot loop detection clears preferences

### Manual Triggers
4. **Settings Button** - "Run Setup Wizard" button on Settings page
5. **TFT Display** - "Setup" option from main menu (future)

### Skip Capability
- Users can skip the wizard (not recommended)
- Skipping shows warning about manual configuration required
- "Run wizard later" option available in Settings

---

## 📋 Wizard Flow & Steps

### Step 1: Welcome & Introduction
**Purpose:** Orient user and explain wizard process

**Display:**
- Welcome message
- Brief description of what will be configured
- Estimated time: 5-10 minutes
- Option to skip wizard (with warning)
- [Continue] button

**Data Collected:** None

**Validation:** None

---

### Step 2: Device Naming
**Purpose:** Set friendly device name for identification

**Display:**
- Input field: Device Name
- Placeholder: "ESP32 Thermostat"
- Examples: "Gecko Tank", "Snake Enclosure", "Havoc's Habitat"
- Character limit: 32 characters
- Preview of where name appears (web UI, MQTT topics, TFT display)

**Data Collected:**
- `deviceName` (string, 1-32 chars)

**Validation:**
- Must not be empty
- Must be alphanumeric + spaces/hyphens/underscores
- No special characters that break MQTT/mDNS

**Default:** "ESP32 Thermostat"

---

### Step 3: WiFi Configuration
**Purpose:** Connect device to WiFi network

**Display:**
- [Scan Networks] button
- List of discovered networks (SSID, signal strength, security)
- Manual entry option (for hidden networks)
- Password input field (show/hide toggle)
- [Test Connection] button
- Connection status indicator

**Data Collected:**
- `wifiSSID` (string)
- `wifiPassword` (string)

**Validation:**
- SSID must not be empty
- Password validation (WPA2 requires 8+ chars)
- Test connection before proceeding
- Show error message if connection fails
- Retry option with different credentials

**Advanced Options (collapsible):**
- Static IP configuration
- Custom DNS servers
- Hostname configuration

**Notes:**
- Device starts in AP mode if no WiFi configured
- After successful connection, wizard continues on new IP
- Auto-redirect to new IP address with countdown

---

### Step 4: Sensor Discovery & Naming
**Purpose:** Discover all DS18B20 sensors and assign friendly names

**Display:**
- [Scan for Sensors] button
- List of discovered sensors:
  - ROM address (64-bit hex)
  - Current temperature reading
  - Name input field
  - Visual indicator (working/error)
- Warning if no sensors found
- Option to rescan
- Sensor assignment preview (which output will use which sensor)

**Data Collected:**
For each sensor:
- `sensorAddress` (hex string, 16 chars)
- `sensorName` (string, 1-32 chars)
- `sensorEnabled` (boolean)

**Validation:**
- At least 1 sensor must be discovered
- Sensor names must be unique
- Each sensor must have a valid temperature reading
- Warn if sensor count doesn't match expected (3)

**Default Names:**
- "Sensor 1", "Sensor 2", "Sensor 3"
- Suggest names based on typical use: "Basking Spot", "Cool Side", "Ambient"

**Error Handling:**
- No sensors found → Show troubleshooting tips (wiring, GPIO pin)
- Sensor read error → Mark as faulty, suggest checking connection

---

### Step 5: Output Configuration
**Purpose:** Configure all 3 outputs (name, hardware type, sensor assignment, initial targets)

**Display:**
For each output (1, 2, 3):
- Enable/Disable toggle
- Output name input
- Hardware type dropdown:
  - AC Dimmer (PID, Pulse Width, On/Off, Schedule)
  - SSR Pulse Control (Pulse Width, On/Off, Schedule)
  - SSR/Relay On/Off (On/Off, Schedule)
- Sensor assignment dropdown (list of named sensors from Step 4)
- Initial target temperature (15-40°C range)
- Control mode selection:
  - Off
  - Manual (fixed power %)
  - PID (automatic control)
  - Time-Proportional
  - On/Off Thermostat
  - Schedule

**Data Collected:**
For each output (1-3):
- `enabled` (boolean)
- `name` (string, 1-32 chars)
- `hardwareType` (enum: DIMMER, SSR_PULSE, SSR_ONOFF)
- `controlPin` (GPIO - pre-set based on output number)
- `assignedSensor` (sensor address from Step 4)
- `targetTemp` (float, 15.0-40.0)
- `controlMode` (enum: OFF, MANUAL, PID, TIME_PROP, ONOFF, SCHEDULE)

**Validation:**
- At least 1 output must be enabled
- Each enabled output must have a sensor assigned
- Target temperature must be reasonable (15-40°C)
- Output names must be unique
- Warn if multiple outputs share same sensor (allowed but unusual)

**Default Configuration:**
```
Output 1 (GPIO 5):
- Name: "Output 1"
- Hardware: AC Dimmer
- Sensor: Sensor 1
- Target: 25.0°C
- Mode: PID

Output 2 (GPIO 14):
- Name: "Output 2"
- Hardware: SSR Pulse
- Sensor: Sensor 2
- Target: 25.0°C
- Mode: Time-Proportional

Output 3 (GPIO 32):
- Name: "Output 3"
- Hardware: SSR On/Off
- Sensor: Sensor 3
- Target: 25.0°C
- Mode: On/Off Thermostat
```

**Advanced Options (per output):**
- PID tuning parameters (if PID mode selected)
- Hysteresis (if On/Off mode)
- Pulse cycle duration (if Pulse or Time-Proportional mode)

---

### Step 6: Safety Configuration
**Purpose:** Configure essential safety parameters

**Display:**
For each enabled output:
- Maximum temperature cutoff (default: 45°C)
- Minimum temperature cutoff (default: 15°C)
- Sensor fault response:
  - Disable output
  - Set to safe power level (specify %)
  - Continue last setting (not recommended)

**Global Safety Settings:**
- Hardware watchdog: Enabled/Disabled (default: Enabled)
- Boot loop detection: Enabled/Disabled (default: Enabled)
- Emergency stop behavior (all outputs off)

**Data Collected:**
For each output:
- `maxTempCutoff` (float, 30-60°C)
- `minTempCutoff` (float, 5-25°C)
- `sensorFaultMode` (enum: DISABLE, SAFE_POWER, CONTINUE)
- `safePowerLevel` (int, 0-100%)

Global:
- `watchdogEnabled` (boolean)
- `bootLoopDetectionEnabled` (boolean)

**Validation:**
- Max temp must be > min temp
- Max temp must be > target temp
- Safe power level must be 0-100%
- Recommend max temp at least 5°C above target

**Defaults:**
- Max temp: target + 10°C (capped at 45°C)
- Min temp: 15°C
- Fault mode: Disable output
- Watchdog: Enabled
- Boot loop detection: Enabled

---

### Step 7: MQTT Configuration (Optional)
**Purpose:** Configure MQTT broker for Home Assistant integration

**Display:**
- "Skip MQTT setup" checkbox (checked by default)
- Broker address input (hostname or IP)
- Broker port (default: 1883)
- Username input (optional)
- Password input (optional)
- Use Home Assistant auto-discovery toggle
- [Test Connection] button
- Connection status indicator

**Data Collected:**
- `mqttEnabled` (boolean)
- `mqttBroker` (string, hostname or IP)
- `mqttPort` (int, 1-65535)
- `mqttUsername` (string, optional)
- `mqttPassword` (string, optional)
- `mqttHADiscovery` (boolean)

**Validation:**
- If enabled, broker address must not be empty
- Port must be valid (1-65535)
- Test connection before proceeding (optional but recommended)
- Show timeout if broker not reachable
- Allow skip if connection fails

**Defaults:**
- MQTT disabled
- Port: 1883
- HA Discovery: Enabled (if MQTT enabled)

**Advanced Options:**
- Custom base topic
- QoS settings
- TLS/SSL (future)

---

### Step 8: Testing & Verification
**Purpose:** Test configuration before finalizing

**Display:**
- Configuration summary (all settings in review)
- Real-time sensor readings
- Manual output test controls:
  - [Test Output 1] - Activate at 50% for 10 seconds
  - [Test Output 2] - Activate at 50% for 10 seconds
  - [Test Output 3] - Activate at 50% for 10 seconds
- Visual/audio indicators (LEDs, display, etc.)
- Warning: "Ensure heating elements are safe to test"
- [Edit Configuration] - Go back to any step
- [Restart Wizard] - Start over
- [Complete Setup] - Save and finish

**Actions:**
- Show all configured settings for review
- Allow testing outputs individually
- Provide safety warning before testing
- Auto-timeout on test activations (10s max)
- Emergency stop available during testing

**Validation:**
- All enabled outputs have valid sensor assignment
- All temperatures are reading correctly
- WiFi is connected
- MQTT connected (if enabled)

---

### Step 9: Completion
**Purpose:** Finalize setup and start normal operation

**Display:**
- Success message
- Summary of configuration
- Quick links:
  - Go to Dashboard
  - View Settings
  - Configure Schedules
  - Safety Settings
- "Setup complete!" confirmation
- Next steps:
  - Configure schedules (optional)
  - Install Android app (link/QR code)
  - Add to Home Assistant (if MQTT enabled)

**Actions:**
- Save all configuration to preferences
- Restart system to apply settings
- Clear "first boot" flag
- Redirect to main dashboard (auto-redirect after 5s)

**Post-Setup:**
- Show brief tutorial overlay on first dashboard load (dismissible)
- Enable all configured outputs in selected modes
- Start normal operation

---

## 🎨 User Interface Design

### Layout
- **Progress Indicator:** Step X of 9 (visual progress bar)
- **Navigation:**
  - [Previous] button (disabled on step 1)
  - [Next] button (disabled until validation passes)
  - [Skip Wizard] link (confirmation dialog)
- **Help Text:** Context-sensitive help for each step
- **Responsive:** Works on mobile, tablet, desktop browsers

### Visual Design
- Clean, minimal design
- Dark mode compatible
- Large touch-friendly buttons (TFT display)
- Color-coded sections (info, warning, error)
- Animations for transitions (smooth, not distracting)

### Accessibility
- Clear labels and instructions
- Error messages near relevant fields
- Keyboard navigation support
- Screen reader compatible (ARIA labels)

---

## 🔧 Technical Implementation

### Architecture

**Web-Based Wizard:**
- Primary interface via web UI
- New route: `/setup` (accessible in AP mode and normal mode)
- Multi-page form with client-side validation
- AJAX for network scans, sensor discovery, testing
- Progressive enhancement (works without JavaScript)

**TFT Display Wizard (Future):**
- Native TFT implementation using display_manager
- Touch-based navigation
- Synchronized with web wizard (same backend)

### Backend API Endpoints

```
GET  /api/wizard/status       - Check if wizard is required
GET  /api/wizard/state        - Get current wizard state/progress
POST /api/wizard/reset        - Reset wizard state

# Step-specific endpoints
POST /api/wizard/device-name  - Save device name
GET  /api/wizard/wifi/scan    - Scan for WiFi networks
POST /api/wizard/wifi/test    - Test WiFi credentials
POST /api/wizard/wifi/save    - Save WiFi config

GET  /api/wizard/sensors/scan - Scan for DS18B20 sensors
POST /api/wizard/sensors/save - Save sensor names

POST /api/wizard/outputs/save - Save output configuration
POST /api/wizard/safety/save  - Save safety settings
POST /api/wizard/mqtt/test    - Test MQTT connection
POST /api/wizard/mqtt/save    - Save MQTT config

POST /api/wizard/test-output  - Activate output for testing
POST /api/wizard/complete     - Finalize and save all config
```

### State Management

**Wizard State Storage:**
- Use preferences namespace: "wizard"
- Track current step
- Save progress (allow resuming if interrupted)
- Clear state on completion

**Configuration Storage:**
- Save to existing preferences namespaces:
  - "wifi" - WiFi credentials
  - "device" - Device name, settings
  - "sensors" - Sensor addresses and names
  - "output1", "output2", "output3" - Output configs
  - "safety" - Safety parameters
  - "mqtt" - MQTT settings

### Error Handling

**Network Errors:**
- WiFi scan timeout → Show "No networks found, try again"
- Connection test failure → Show specific error (wrong password, no DHCP, etc.)
- MQTT connection failure → Allow skip or retry

**Hardware Errors:**
- No sensors found → Troubleshooting guide (check wiring, GPIO pin)
- Sensor read error → Mark sensor as faulty, allow continuing
- Output test failure → Warning but allow proceeding

**Validation Errors:**
- Show inline error messages
- Highlight invalid fields in red
- Disable [Next] button until errors resolved
- Provide specific guidance for fixing errors

### Performance Considerations

- Lazy loading: Only scan WiFi/sensors when user reaches that step
- Caching: Cache sensor scan results for 60 seconds
- Timeouts: Reasonable timeouts for network operations (10s WiFi scan, 5s connection test)
- Non-blocking: Use async operations for scanning/testing

---

## 🧪 Testing Requirements

### Unit Tests
- Validate input sanitization (device name, SSID, etc.)
- Test configuration save/load
- Verify state machine transitions

### Integration Tests
- Full wizard flow (all steps)
- Wizard resumption after interruption
- Factory reset → wizard trigger
- Configuration persistence after wizard completion

### User Acceptance Testing
- First-time user completes wizard in under 10 minutes
- User can recover from errors (wrong password, etc.)
- Wizard accessible from both AP mode and normal mode
- All configurations applied correctly

### Edge Cases
- No WiFi networks available
- No sensors detected
- Power loss during wizard
- Browser refresh during wizard
- Multiple browsers accessing wizard simultaneously

---

## 📱 Platform Support

### Web Interface (Primary)
- Desktop browsers (Chrome, Firefox, Safari, Edge)
- Mobile browsers (iOS Safari, Android Chrome)
- Tablet browsers
- Minimum resolution: 320x480 (mobile)

### TFT Display (Future Phase)
- Native touch-based wizard
- Simplified flow for small screen
- Synchronized with web wizard progress

### API Access (Advanced Users)
- Programmatic setup via REST API
- Useful for mass deployment
- Configuration import/export (JSON)

---

## 🔒 Security Considerations

### WiFi Credentials
- Passwords never displayed in UI (use password input type)
- Stored encrypted in preferences (ESP32 flash encryption)
- Not transmitted in logs or MQTT

### MQTT Credentials
- Same security as WiFi credentials
- Optional authentication
- Future: TLS/SSL support

### Wizard Access Control
- Wizard always accessible in AP mode (no auth required)
- In normal mode:
  - If PIN security enabled, require PIN before accessing wizard
  - Warn that wizard will re-configure device

### Factory Reset Protection
- Require confirmation dialog
- Require PIN (if security enabled)
- Show warning about data loss

---

## ⚡ Performance Targets

| Operation | Target Time | Acceptable Max |
|-----------|-------------|----------------|
| WiFi scan | < 5s | 10s |
| WiFi connection test | < 5s | 10s |
| Sensor scan | < 3s | 5s |
| Sensor temperature read | < 1s | 2s |
| MQTT connection test | < 3s | 5s |
| Output test activation | Immediate | 500ms |
| Save configuration | < 1s | 2s |
| Complete wizard | < 2s | 5s |
| Page transitions | < 100ms | 300ms |

**Total Wizard Completion Time:**
- Target: 5-8 minutes (experienced user)
- Acceptable: 10-15 minutes (first-time user with reading)

---

## 📝 Documentation Requirements

### User Documentation
- Setup wizard guide with screenshots
- Troubleshooting section
- Video walkthrough (optional)
- FAQ for common issues

### Developer Documentation
- API endpoint documentation
- State machine diagram
- Database schema for wizard state
- Code comments and inline docs

---

## 🚀 Future Enhancements

### Phase 2 Features
- Import/export configuration (JSON/file)
- Multi-language support (i18n)
- Wizard templates (pre-configured for common setups)
- Cloud sync of configuration (backup/restore)

### Advanced Features
- Wizard API for external tools
- Configuration migration assistant
- Bulk device provisioning
- Remote setup assistance (guided by expert via chat)

---

## ✅ Acceptance Criteria

### Must Have (v1.0)
- [ ] All 9 steps implemented and functional
- [ ] WiFi configuration with network scanning
- [ ] Sensor discovery and naming
- [ ] Output configuration (all 3 outputs)
- [ ] Safety parameter configuration
- [ ] MQTT setup (optional step)
- [ ] Configuration testing
- [ ] Save configuration to preferences
- [ ] Responsive web UI (mobile + desktop)
- [ ] Input validation on all fields
- [ ] Error handling with helpful messages
- [ ] Progress indicator and navigation
- [ ] Accessible from AP mode
- [ ] Factory reset triggers wizard

### Should Have (v1.1)
- [ ] Configuration summary/review page
- [ ] Wizard progress saving (resume on interruption)
- [ ] Advanced options (collapsible sections)
- [ ] Help text and tooltips
- [ ] TFT display wizard implementation

### Could Have (v2.0)
- [ ] Import/export configuration
- [ ] Wizard templates
- [ ] Multi-language support
- [ ] Video/image guides embedded in wizard

---

## 📊 Success Metrics

### Usability Metrics
- Setup completion rate: >90%
- Average completion time: <10 minutes
- Error recovery rate: >95% (users recover from errors without giving up)
- Support ticket reduction: 50% fewer "how do I configure" questions

### Technical Metrics
- Wizard crash rate: <1%
- Configuration persistence: 100% (no lost settings)
- WiFi connection success rate: >95%
- Sensor discovery success rate: >98%

---

## 📌 Notes & Open Questions

### Questions for Tomorrow:
1. Should wizard be accessible after initial setup (for reconfiguration)?
2. How to handle wizard access when PIN security is enabled?
3. Should wizard support importing config from another device?
4. What happens if user exits wizard mid-way (save progress or discard)?
5. Should there be a "quick setup" mode (minimal config) vs "full setup"?
6. How to handle firmware updates that add new wizard steps?
7. Should wizard validate hardware connections (e.g., detect if AC dimmer is actually connected)?
8. Visual design mockups needed?
9. Should wizard support scheduling configuration, or leave that for post-setup?
10. How to handle multi-language support in future?

### Design Decisions Needed:
- [ ] Wizard theme (match existing web UI or separate branding?)
- [ ] Transition animations (smooth vs instant)
- [ ] TFT wizard priority (include in v1.0 or defer to v1.1?)
- [ ] Configuration backup before wizard runs (auto-create backup?)

---

**Document Status:** Draft - Ready for review and additions
**Next Steps:** Review requirements, add/modify as needed, create implementation plan
