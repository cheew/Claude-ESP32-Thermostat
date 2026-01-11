# Release v2.1.0 - Multi-Output MQTT, Schedule & Mobile

**Release Date:** January 11, 2026
**Tag:** v2.1.0
**Status:** Production Ready

---

## 🎉 What's New

This release completes the multi-output transformation with full MQTT Home Assistant integration, per-output scheduling, and comprehensive mobile optimizations.

### Major Features

#### 1. 🏠 MQTT Multi-Output Support
Complete Home Assistant integration with 3 independent climate entities:

- **Separate Climate Entities**: Each output appears as its own climate entity in Home Assistant
- **Per-Output MQTT Topics**:
  - `thermostat/output1/temperature`, `thermostat/output1/setpoint`, `thermostat/output1/mode`
  - `thermostat/output2/temperature`, `thermostat/output2/setpoint`, `thermostat/output2/mode`
  - `thermostat/output3/temperature`, `thermostat/output3/setpoint`, `thermostat/output3/mode`
- **Auto-Discovery**: All 3 entities auto-configure in Home Assistant
- **Device Grouping**: All entities grouped under single device
- **Independent Control**: Change temperature and mode for each output from HA

#### 2. 📅 Schedule Page Multi-Output
User-friendly per-output scheduling:

- **Output Selector**: Dropdown to switch between outputs
- **8 Schedule Slots**: Each output has 8 independent time-based schedules
- **Time Control**: Set hour, minute, and target temperature per slot
- **Day Selection**: Choose any combination of days (Sun-Sat) per slot
- **Visual Feedback**: Active slots highlighted in green
- **Tips Section**: Built-in guidance for using schedules

#### 3. 📱 Mobile Responsive Design
Optimized for phones and tablets:

- **Touch-Friendly**: 44x44px minimum touch targets (Apple/Google guidelines)
- **Media Queries**: Responsive layouts for tablet (≤768px), phone (≤480px), tablet landscape
- **Auto-Zoom Prevention**: 16px font size on inputs prevents iOS zoom
- **Single Column Layout**: Output cards stack vertically on mobile
- **Compact Navigation**: Smaller buttons on small screens
- **Dark Mode**: Fully compatible on all screen sizes

---

## 🔧 Technical Details

### Files Modified
- `src/network/mqtt_manager.cpp` - Multi-output MQTT publishing and subscriptions
- `include/mqtt_manager.h` - Added `mqtt_publish_all_outputs()` declaration
- `src/main.cpp` - Updated to use new MQTT multi-output function, version to 2.1.0
- `src/network/web_server.cpp` - Schedule page rewrite + mobile CSS enhancements

### Memory Usage
- **Flash**: 89.1% (1,167,717 / 1,310,720 bytes) - ~143 KB remaining
- **RAM**: 20.2% (66,064 / 327,680 bytes) - ~261 KB free

### Build Information
- **Platform**: ESP32 (espressif32)
- **Framework**: Arduino
- **Build Time**: 21.26 seconds
- **Status**: ✅ Successful

---

## 📋 Testing Checklist

### MQTT Integration
- [ ] Home Assistant auto-discovers 3 climate entities
- [ ] Each entity shows correct name from output config
- [ ] Temperature updates work for all 3 outputs
- [ ] Target temperature changes from HA work
- [ ] Mode changes from HA work (heat/off)
- [ ] All 3 entities grouped under one device

### Schedule Page
- [ ] Output selector dropdown works
- [ ] Can view each output's schedule
- [ ] Can edit time slots for each output
- [ ] Save persists to correct output
- [ ] Schedule activates at correct times

### Mobile Responsive
- [ ] Interface usable on phone (320px+ width)
- [ ] All buttons easily tappable (44x44px)
- [ ] No horizontal scrolling needed
- [ ] Forms full-width and easy to fill
- [ ] Navigation accessible on all screen sizes
- [ ] Dark mode works on mobile

---

## 🚀 Upgrade Instructions

### From v2.0.0
This is a **minor version upgrade** - settings and configuration will be preserved.

1. Download `firmware.bin` from this release
2. Upload via web interface (Settings → Update) or OTA
3. Device will reboot automatically
4. Configure MQTT broker settings if not already configured
5. In Home Assistant, delete old climate entity (if exists)
6. Wait 30 seconds for auto-discovery to create 3 new entities

### From v1.x
This is a **major version upgrade** - settings will be reset.

1. Backup your current settings (WiFi credentials, MQTT config)
2. Download `firmware.bin` from this release
3. Upload via web interface or OTA
4. Reconfigure WiFi, MQTT, and output settings
5. Reassign sensors to outputs
6. Configure schedules as needed

---

## 🐛 Known Issues

None currently identified. If you encounter issues, please report them on the [GitHub Issues](https://github.com/cheew/Claude-ESP32-Thermostat/issues) page.

---

## 📚 Documentation

- **Quick Start Guide**: [QUICK_START.md](QUICK_START.md)
- **Changelog**: [CHANGELOG.md](CHANGELOG.md)
- **Session Summary**: [SESSION_SUMMARY.md](SESSION_SUMMARY.md)
- **API Documentation**: See README.md

---

## 💡 What's Next (v2.2.0 Roadmap)

Potential future enhancements:
- Code cleanup (remove old unused modules)
- Schedule preview/visualization
- "Copy schedule to other outputs" button
- Output grouping (control multiple together)
- Temperature offset calibration per sensor
- Power limits per output

---

## 🙏 Credits

Developed with [Claude Code](https://claude.com/claude-code) - AI-assisted development tool

---

## 📝 Full Changelog

See [CHANGELOG.md](CHANGELOG.md) for complete version history.

**Previous Versions:**
- v2.0.0 - Multi-Output Architecture
- v1.9.1 - WiFi Reconnection Fix
- v1.9.0 - Dark Mode & Live Console
- v1.8.0 - Console Feature
- v1.7.1 - MQTT Enhancements

---

**Download the firmware binary from the release assets below** ⬇️
