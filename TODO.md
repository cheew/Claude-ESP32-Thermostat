# ESP32 Reptile Thermostat - TODO List

## ✅ COMPLETED (Updated January 7, 2026)
- [x] Basic PID temperature control
- [x] AC dimmer integration (RobotDyn)
- [x] Web interface (home page)
- [x] Settings page (WiFi, MQTT, PID)
- [x] AP mode for initial setup
- [x] Persistent storage (preferences)
- [x] MQTT integration with Home Assistant discovery
- [x] Real-time status updates
- [x] Power output visualization
- [x] GitHub repository created
- [x] Clean UI design
- [x] TFT display integration (JC2432S028)
- [x] Touch button controls (+ / -)
- [x] TFT settings screen with mode selection
- [x] Device name customization (web + TFT)
- [x] MQTT status indicator on display
- [x] Touch calibration working perfectly
- [x] mDNS/Bonjour service discovery
- [x] OTA firmware update capability
- [x] **Create `/info` page (device info, uptime, stats)**
- [x] **Add navigation bar to all pages**
- [x] **Create `/logs` page (show recent events/errors)**
- [x] **Improve mobile responsiveness**
- [x] **Add "About" section with version (in footer on all pages)**

---

## 🔧 READY TO BUILD (Android App Foundation)

### High Priority - Android App Development

#### 1. Improve JSON API Endpoints
**Why:** App needs consistent, well-structured data
**Status:** Partially done - `/api/status` exists, needs expansion
**Tasks:**
- [ ] Add `/api/info` endpoint (device name, version, MAC address, uptime)
- [ ] Standardize error responses
- [ ] Add `/api/control` POST endpoint (unified control)
- [ ] Document all API endpoints in README
- [ ] Test all endpoints with Postman or curl

**Proposed Endpoints:**
```
GET  /api/status  → {temp, target, heating, power, mode}
GET  /api/info    → {name, version, mac, ip, uptime, wifi_rssi, mqtt_connected}
GET  /api/logs    → {logs: [...]} (optional - for app display)
POST /api/control → {target: 28.0, mode: "auto"}
```

---

#### 2. Create Android App Project Structure
**Why:** Get basic app skeleton ready
**Tasks:**
- [ ] Install Android Studio
- [ ] Create new project (Kotlin, minimum SDK 24)
- [ ] Set up basic navigation (list screen, detail screen)
- [ ] Add required permissions (INTERNET, ACCESS_WIFI_STATE, CHANGE_WIFI_MULTICAST_STATE)
- [ ] Test "Hello World" app on phone

**Project Structure:**
```
ThermostatApp/
├── MainActivity (Device List)
├── DeviceDetailActivity (WebView or Native UI)
├── Device model class
└── Network/Discovery classes (TBD)
```

---

#### 3. Implement Basic Android App Features (Without Discovery)
**Why:** Get functional app before adding mDNS complexity
**Tasks:**
- [ ] Create device list UI (RecyclerView)
- [ ] Add manual "Add Device" by IP address
- [ ] Implement HTTP requests to `/api/status`
- [ ] Display device name and temperature in list
- [ ] Add refresh button
- [ ] Open WebView or native detail view on device tap
- [ ] Save devices to SharedPreferences
- [ ] Test with your physical thermostat

**Result:** Working app that can manually add and monitor devices

---

#### 4. Implement mDNS Device Discovery
**Why:** Auto-find thermostats on network (this is the complex part)
**Tasks:**
- [ ] Research Android NSD (Network Service Discovery) API
- [ ] Implement service discovery for "_http._tcp."
- [ ] Filter for "reptile-thermostat" service type
- [ ] Parse device name and IP from TXT records
- [ ] Add discovered devices to list automatically
- [ ] Test discovery reliability
- [ ] Add manual refresh discovery option

**Note:** This is technically challenging on Android, start simple with manual IP entry first!

---

### Medium Priority - Firmware API Improvements

#### 5. Add Comprehensive API Endpoint
**Why:** Better app integration
**Tasks:**
- [ ] Create `/api/info` endpoint with:
  - Device name
  - Firmware version
  - MAC address
  - IP address
  - WiFi SSID and RSSI
  - MQTT connection status
  - Uptime
  - Free memory
- [ ] Optional: Add `/api/logs` endpoint (return JSON array of logs)
- [ ] Test endpoints return valid JSON
- [ ] Update README with API documentation

---

#### 6. Improve Error Handling & Safety
**Why:** Production reliability
**Tasks:**
- [ ] Add safe mode if sensor fails (turn off heating)
- [ ] Improve sensor error recovery
- [ ] Add watchdog timer (optional)
- [ ] Better MQTT reconnection logic
- [ ] Log all errors to system logs

---

### Low Priority - Nice to Have

#### 7. Add Statistics/Data Logging
**Tasks:**
- [ ] Track total heating time
- [ ] Count heating cycles
- [ ] Track temperature min/max/average
- [ ] Display stats on Info page
- [ ] Optional: Export stats to CSV via web

---

#### 8. Add Scheduling Feature
**Tasks:**
- [ ] Simple day/night temperature schedule
- [ ] Web interface for schedule editing
- [ ] Store schedule in preferences
- [ ] Display next scheduled change

---

#### 9. Code Cleanup and Documentation
**Tasks:**
- [ ] Add detailed code comments
- [ ] Create wiring diagram (Fritzing or similar)
- [ ] Add photos of completed build to README
- [ ] Create quick start guide
- [ ] Document calibration procedures
- [ ] Create troubleshooting guide

---

## 📱 ANDROID APP DEVELOPMENT ROADMAP

### Phase 1: Basic Functionality (Start Here!)
**Goal:** Working app with manual device entry
1. Install Android Studio
2. Create basic app skeleton
3. Add device by IP address
4. Fetch and display `/api/status`
5. Show temperature and device name
6. Test with real hardware

**Estimated Time:** 4-8 hours for someone new to Android

---

### Phase 2: Enhanced Features
**Goal:** Better UX and controls
1. Add device detail page
2. Temperature control from app
3. Mode switching
4. Refresh button
5. Device persistence

**Estimated Time:** 4-6 hours

---

### Phase 3: Auto-Discovery (Advanced)
**Goal:** Auto-find devices on network
1. Research NSD API
2. Implement discovery
3. Add discovered devices automatically
4. Handle network changes

**Estimated Time:** 8-12 hours (this is tricky!)

---

### Phase 4: Polish
**Goal:** Production-ready app
1. Improve UI design
2. Add notifications (optional)
3. Widget support (optional)
4. Error handling
5. Settings page
6. About page

**Estimated Time:** 6-10 hours

---

## 🚀 FUTURE FEATURES (Post-MVP)

### Phase 2 Features
- [ ] Auto-update from GitHub releases
- [ ] Temperature history graphing
- [ ] Email/SMS alerts
- [ ] Multiple temperature zones
- [ ] Integration with other smart home platforms (beyond HA)
- [ ] Cloud synchronization (optional)

### Phase 3 Features  
- [ ] iOS app
- [ ] Voice control (Alexa/Google)
- [ ] Humidity control integration
- [ ] Custom enclosure design
- [ ] PCB design (integrated board)
- [ ] Battery backup with RTC

---

## 📝 NOTES

### Current Hardware Setup
- ESP32 WROOM DevKit
- DS18B20 temperature sensor (GPIO4)
- RobotDyn AC Dimmer (GPIO5 PWM, GPIO27 Z-C)
- JC2432S028 TFT Display (2.8" ILI9341)

### Development Environment
- PlatformIO in VS Code
- GitHub repo: https://github.com/cheew/Claude-ESP32-Thermostat
- Android Studio: To be installed

### Network Configuration
- WiFi SSID: mesh
- MQTT Broker: 192.168.1.123
- MQTT User: admin

### Current Testing Access
- Device IP: Check Info page or serial monitor
- mDNS: `[device-name].local` (lowercase, spaces replaced with hyphens)
- Web Interface: `http://[ip-address]/` or `http://[device-name].local/`

---

## 🎯 IMMEDIATE NEXT STEPS

1. **Upload and test v1.2.0 firmware** ✓ (Ready to upload!)
   - Use `/update` page for OTA update
   - Or upload via USB if needed
   
2. **Test new web pages**
   - Browse to `/info` page
   - Check `/logs` page
   - Verify navigation works
   - Test on mobile browser

3. **Add `/api/info` endpoint** (Quick win for app development)
   - Should take ~30 minutes
   - Returns all device info as JSON
   - Makes app development much easier

4. **Set up Android Studio** (When ready)
   - Download and install
   - Create first project
   - Get familiar with basics

5. **Start simple Android app**
   - Manual IP entry first
   - Don't worry about discovery yet
   - Get basic status display working

---

**Last Updated:** January 7, 2026  
**Current Version:** 1.2.0  
**Status:** Web interface complete! Ready for Android app development.

## ✨ Recent Wins
- ✅ Enhanced web interface with Info and Logs pages
- ✅ Professional navigation and responsive design
- ✅ System logging with 20-entry buffer
- ✅ Uptime and memory monitoring
- ✅ Mobile-friendly layout
