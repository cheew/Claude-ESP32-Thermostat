# Temperature Scheduling Guide - v1.3.0

## 🎯 Overview

Your thermostat now has a powerful scheduling system that automatically adjusts temperatures throughout the day!

## ✨ Features

- **Up to 8 time slots** per day
- **Day-specific schedules** (different temps for weekdays vs weekends)
- **Next scheduled change indicator** - always know what's coming next
- **Enable/disable toggle** - easily turn scheduling on/off
- **Temperature history logging** - 24 hours of data at 1-minute intervals
- **NTP time sync** - accurate time from the internet

## 📅 How It Works

1. Set up time slots with target temperatures
2. Choose which days each slot applies to
3. Enable the schedule
4. Thermostat automatically changes temperature at scheduled times
5. Manual changes temporarily override schedule until next scheduled time

## 🚀 Quick Start

### Default Schedule (Pre-configured)
When you first access the schedule page, you'll see:
- **Slot 1:** 7:00 AM → 28°C (Every day)
- **Slot 2:** 10:00 PM → 24°C (Every day)

This gives your reptile warm days and cooler nights!

### Accessing the Schedule Page

**Web Browser:**
```
http://[your-device-ip]/schedule
```

Or click **📅 Schedule** in the navigation bar.

## 📝 Setting Up Your Schedule

### Each Time Slot Has:

1. **Active Checkbox** - Enable/disable this specific slot
2. **Hour** (0-23) - 24-hour format (e.g., 14 = 2 PM)
3. **Minute** (0-59)
4. **Temperature** (15-45°C) - Target temperature for this time
5. **Active Days** - Click day buttons to toggle (green = active)

### Example Schedules:

#### **Daytime Reptile (Nocturnal)**
```
Slot 1: 08:00 AM → 24°C (Cooler day) - Every day
Slot 2: 08:00 PM → 30°C (Warmer night) - Every day
```

#### **Diurnal Reptile with Weekend Variation**
```
Slot 1: 07:00 AM → 28°C - M T W T F (Weekdays)
Slot 2: 09:00 AM → 28°C - S S (Weekends - sleep in!)
Slot 3: 10:00 PM → 24°C - Every day
```

#### **Advanced Multi-Temperature**
```
Slot 1: 06:00 AM → 25°C (Morning warm-up)
Slot 2: 10:00 AM → 30°C (Peak basking)
Slot 3: 04:00 PM → 28°C (Afternoon)
Slot 4: 10:00 PM → 24°C (Night)
```

## 🎮 Using the Schedule Page

### Enable/Disable Schedule
- Toggle switch at top right of page
- When disabled: Manual control only
- When enabled: Automatic temperature changes

### Adding Slots
1. Click **➕ Add Slot** button
2. New slot appears at bottom
3. Configure time, temperature, and days
4. Check "Active" box to enable it
5. Click **Save Schedule**

### Removing Slots
1. Click **➖ Remove Last** button
2. Last slot is deleted
3. Click **Save Schedule**

### Editing Slots
1. Change any values (time, temp, days)
2. Click **Save Schedule** at bottom
3. Changes take effect immediately

### Day Selection
- **S M T W T F S** = Sun, Mon, Tue, Wed, Thu, Fri, Sat
- Click day buttons to toggle green (active) / gray (inactive)
- Multiple days can be selected per slot

## 📊 Schedule Indicators

### On Home Page
When schedule is active:
```
📅 Next: 24°C at 10:00 PM
```

### On Schedule Page
Shows next upcoming scheduled change with time and temperature.

## ⚠️ Important Notes

### Time Synchronization
- ESP32 syncs time from internet (NTP) on boot
- Requires WiFi connection (doesn't work in AP mode)
- Time zone is UTC by default (you can change this if needed)
- If time seems wrong, restart device while connected to WiFi

### Manual Overrides
- You can manually change temperature anytime
- Manual change stays until next scheduled time
- Schedule remains active in background
- Next scheduled change will still occur

### Schedule Priority
- Schedule changes target temperature only
- PID control still manages actual heating
- Mode setting (auto/on/off) is independent

## 🔧 Technical Details

### Time Format
- Uses 24-hour format (military time)
- 00:00 = Midnight
- 12:00 = Noon
- 23:59 = 11:59 PM

### Day Codes
- Days stored as string: "SMTWTFS"
- S = Sunday, M = Monday, etc.
- Example: "MTWTF" = Weekdays only

### Storage
- Schedule stored in ESP32 flash memory
- Survives power loss and reboots
- Up to 8 slots = ~500 bytes of storage

### Temperature History
- Logs temperature every 1 minute
- Stores last 1440 points (24 hours)
- Circular buffer (oldest replaced by newest)
- API available: `/api/temp-history`

## 🐛 Troubleshooting

### Schedule Not Working?

**Check these:**
1. Is schedule **enabled**? (Toggle at top of page)
2. Is the time slot **active**? (Checkbox checked)
3. Is **today** selected in days? (Day button green)
4. Is the **time correct**? Check Info page for current time
5. Is WiFi connected? (Time sync needs internet)

### Time Incorrect?
1. Verify WiFi connection (not in AP mode)
2. Restart device to re-sync with NTP
3. Check logs for "Syncing time with NTP" message
4. May need to wait a few minutes after boot

### Next Schedule Shows Wrong Time?
- Calculation only considers future times today
- Won't show tomorrow's schedule
- Refreshes automatically every minute

### Can't Add More Slots?
- Maximum is 8 slots
- Remove unused slots to make room
- Consider combining similar time slots

## 💡 Pro Tips

### Optimal Slot Count
- **2-4 slots** is usually plenty for most reptiles
- Too many slots can be confusing
- Focus on major temperature changes (day/night)

### Day-Specific Scheduling
- Use for different weekend routines
- Adjust for cleaning days
- Match your feeding schedule

### Gradual Temperature Changes
```
07:00 AM → 25°C (Gentle wake-up)
09:00 AM → 28°C (Full basking)
06:00 PM → 26°C (Evening)
10:00 PM → 24°C (Night)
```

### Testing Your Schedule
1. Set a slot for 5 minutes from now
2. Watch the log page for "Schedule: Temp set to X°C"
3. Verify temperature changes
4. Adjust as needed

## 📱 Mobile Use

Schedule page is fully responsive:
- Works great on phones and tablets
- Day buttons stack nicely
- Easy to edit on the go

## 🔮 Future Enhancements

Possible additions:
- [ ] Visual timeline graph
- [ ] Copy schedule to other days
- [ ] Import/export schedules
- [ ] Seasonal schedules
- [ ] Sunrise/sunset based timing
- [ ] Temperature ramping (gradual changes)

## 📖 Example Use Cases

### Bearded Dragon (Diurnal)
```
Slot 1: 07:00 AM → 28°C (Morning basking)
Slot 2: 10:00 PM → 22°C (Night drop)
Days: SMTWTFS (Every day)
```

### Leopard Gecko (Nocturnal)
```
Slot 1: 08:00 AM → 25°C (Cooler day)
Slot 2: 08:00 PM → 29°C (Warm night activity)
Days: SMTWTFS (Every day)
```

### Ball Python with Weekend Variation
```
Slot 1: 08:00 AM → 28°C - MTWTF (Weekday)
Slot 2: 10:00 AM → 28°C - SS (Weekend)
Slot 3: 10:00 PM → 26°C - SMTWTFS (Every night)
```

---

## 🎉 You're All Set!

Your thermostat now has professional-grade scheduling. Set it up once and forget about it - your reptile will have perfect temperatures around the clock!

**Need help?** Check the Logs page to see schedule changes in action.
