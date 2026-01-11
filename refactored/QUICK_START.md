# Quick Start Guide - Multi-Output Thermostat v2.0.0

## 🚀 Getting Started

### 1. Access Your Device
Open your web browser and navigate to:
```
http://192.168.1.236/
```
(Replace with your device's actual IP address)

---

## 📱 Web Interface Guide

### Home Page (Main Dashboard)
- **What you'll see:** 3 output cards showing real-time status
- **Quick controls:** Off / Manual 50% / PID buttons on each card
- **Updates:** Auto-refreshes every 2 seconds

**Try this first:**
1. Click "Manual 50%" on Output 1
2. Watch the power bar update
3. Click "Off" to turn it off
4. Click "PID" to enable automatic control

---

### 💡 Outputs Page (Configuration)
**Purpose:** Configure each output's name, sensor, and PID settings

**Steps to configure an output:**
1. Click the "💡 Outputs" tab in navigation
2. Click "Output 1", "Output 2", or "Output 3" tab
3. Change settings:
   - **Name:** Give it a friendly name (e.g., "Basking Lamp")
   - **Sensor:** Select which temperature sensor to use
   - **Target Temp:** Set desired temperature (15-45°C)
   - **Mode:** Choose control mode (Off, Manual, PID, On/Off, Schedule)
   - **PID Tuning:** Adjust Kp, Ki, Kd values (start with 10, 0.5, 2)
4. Click "Save Configuration"
5. Click "Apply Control" to activate changes immediately

---

### 🌡️ Sensors Page (Sensor Management)
**Purpose:** View all temperature sensors and rename them

**Features:**
- Live temperature readings (updates every 3 seconds)
- See ROM address (unique identifier)
- Rename sensors for easier identification

**To rename a sensor:**
1. Click the "🌡️ Sensors" tab
2. Find the sensor you want to rename
3. Click "Rename" button
4. Enter new name (e.g., "Basking Spot")
5. Click OK
6. Name saves automatically

---

## 🎮 Control Modes Explained

### 1. Off
- Output completely disabled
- No power to device
- Use when not needed

### 2. Manual
- Set fixed power percentage (0-100%)
- No automatic temperature control
- Good for testing or constant power needs
- Example: "Manual 50%" = 50% power constantly

### 3. PID (Auto)
- Automatic temperature control
- Maintains target temperature precisely
- Uses PID algorithm for smooth control
- **Best for most situations**
- Requires sensor assignment

### 4. On/Off Thermostat
- Simple on/off switching
- Heats to target + 0.5°C, then turns off
- Turns back on at target - 0.5°C
- More temperature swing than PID
- Good for high-power heaters

### 5. Schedule
- Changes target temperature based on time of day
- Up to 8 time slots per output
- Each slot: time + target temperature
- Automatically switches between slots
- Good for day/night temperature changes

---

## 🔧 Common Tasks

### Change Target Temperature
**Method 1:** Quick change from Home page
1. Go to Home
2. Use quick control buttons

**Method 2:** Precise control from Outputs page
1. Go to Outputs page
2. Select output tab
3. Change "Target Temperature"
4. Click "Apply Control"

### Assign Sensor to Output
1. Go to Outputs page
2. Select output tab (1, 2, or 3)
3. Choose sensor from "Sensor" dropdown
4. Click "Save Configuration"

### Tune PID for Better Control
**Default values:** Kp=10, Ki=0.5, Kd=2

**If temperature oscillates too much:**
- Reduce Kp (try 8 or 5)
- Reduce Kd (try 1 or 0.5)

**If temperature responds too slowly:**
- Increase Kp (try 15 or 20)
- Increase Ki (try 1 or 2)

**If temperature settles with offset:**
- Increase Ki slightly (try 0.8 or 1.0)

### Enable/Disable an Output
1. Go to Outputs page
2. Select output tab
3. Check or uncheck "Enabled" checkbox
4. Click "Save Configuration"

---

## 🌐 API Usage (Advanced)

### Get All Outputs Status
```bash
curl http://192.168.1.236/api/outputs
```

### Get Single Output Details
```bash
curl http://192.168.1.236/api/output/1
```

### Control Output (Set Mode and Target)
```bash
# Set Output 1 to PID mode, 30°C target
curl -X POST http://192.168.1.236/api/output/1/control \
  -H "Content-Type: application/json" \
  -d '{"mode":"pid","target":30.0}'

# Set Output 2 to manual mode, 75% power
curl -X POST http://192.168.1.236/api/output/2/control \
  -H "Content-Type: application/json" \
  -d '{"mode":"manual","power":75}'

# Turn off Output 3
curl -X POST http://192.168.1.236/api/output/3/control \
  -H "Content-Type: application/json" \
  -d '{"mode":"off"}'
```

### Configure Output (Name, Sensor, PID)
```bash
curl -X POST http://192.168.1.236/api/output/1/config \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Basking Lamp",
    "sensor": "28FF1A2B3C4D5E6F",
    "pid": {"kp": 10.0, "ki": 0.5, "kd": 2.0}
  }'
```

### Get All Sensors
```bash
curl http://192.168.1.236/api/sensors
```

### Rename Sensor
```bash
curl -X POST http://192.168.1.236/api/sensor/name \
  -H "Content-Type: application/json" \
  -d '{
    "address": "28FF1A2B3C4D5E6F",
    "name": "Basking Spot"
  }'
```

---

## 💡 Tips & Tricks

### Sensor Assignment
- **Auto-assigned on boot:**
  - Sensor 1 → Output 1
  - Sensor 2 → Output 2
  - Sensor 3 → Output 3
- You can change assignments anytime in Outputs page
- Multiple outputs can use the same sensor

### Output Names
- Default names: "Lights", "Heat Mat", "Ceramic Heater"
- Change to anything you want: "Basking Lamp", "Cool Side", etc.
- Makes it easier to identify in Home page

### Sensor Names
- Default names: "Temperature Sensor 1", "Temperature Sensor 2", etc.
- Rename to locations: "Basking Spot", "Cool Hide", "Ambient"
- Helps when assigning sensors to outputs

### Quick Testing
1. Set Output to Manual mode
2. Set power to 25%, 50%, 75%, 100% to test
3. Verify hardware responds correctly
4. Then switch to PID mode for automatic control

### Monitoring
- Home page auto-refreshes every 2 seconds
- Keep it open in browser to monitor temperatures
- Color-coded: Red = heating, Green = idle

---

## ⚠️ Important Notes

### Hardware Restrictions
- **Output 1 (GPIO 5):** AC Dimmer only → Use for lights
- **Output 2 (GPIO 14):** SSR only → Use for heat devices
- **Output 3 (GPIO 32):** SSR only → Use for heat devices

### Safety
- Always test manually before leaving unattended
- Monitor for first few hours
- Verify target temperatures are safe for your setup
- Use proper hardware rated for your heating elements

### Configuration Persistence
- All settings save to ESP32 flash memory
- Persist after power cycle or reboot
- To reset to defaults: reflash firmware

### Temperature Range
- Minimum: 15°C
- Maximum: 45°C
- Adjust in code if you need different range

---

## 🐛 Troubleshooting

### "No sensors found" message
1. Check sensor wiring (red = 5V, black = GND, yellow = GPIO 4)
2. Verify sensor is working (test with multimeter)
3. Power cycle ESP32
4. Sensors should auto-discover on boot

### Output not responding
1. Check "Enabled" checkbox is checked
2. Verify mode is not "Off"
3. Check sensor is assigned
4. Verify hardware connections
5. Test with Manual mode at 50% power

### Temperature not updating
1. Check sensor is assigned to output
2. Go to Sensors page - verify sensor reading
3. Verify sensor address matches in Outputs config
4. Check sensor wiring

### PID mode not working well
1. Verify sensor is assigned and reading correctly
2. Adjust PID parameters (start with Kp=10, Ki=0.5, Kd=2)
3. Give it time to stabilize (15-30 minutes)
4. Monitor and tune parameters as needed

### Web page not loading
1. Verify ESP32 is powered on
2. Check WiFi connection (LED indicators)
3. Verify IP address (check router DHCP table)
4. Try accessing via mDNS: `http://thermostat.local/`

---

## 📞 Need Help?

Check these resources:
- **Full documentation:** `SESSION_SUMMARY.md`
- **API reference:** `PROGRESS_UPDATE.md`
- **Source code:** Browse `src/` directory
- **GitHub Issues:** Report bugs or request features

---

**Version:** 2.0.0
**Last Updated:** January 11, 2026
**Status:** Ready for testing
