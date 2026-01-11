# Next Session Plan - Priorities 1, 2, 3

**Status:** Ready to implement
**Estimated Time:** 2-3 hours
**Priority Order:** MQTT → Schedule → Mobile optimization

---

## 🎯 Priority 1: MQTT Multi-Output Support (HIGH)

### Goal
Update MQTT manager to publish all 3 outputs as separate Home Assistant climate entities.

### Current State
- MQTT only publishes Output 1 status
- Home Assistant sees single climate entity
- Topics: `thermostat/temperature`, `thermostat/setpoint`, etc.

### Target State
- 3 independent Home Assistant climate entities
- Per-output topics for each output
- Auto-discovery for all 3 outputs

### Implementation Tasks

#### A. Update MQTT Topic Structure
**Current topics:**
```
thermostat/temperature
thermostat/setpoint
thermostat/mode
thermostat/heating
thermostat/power
```

**New topics:**
```
thermostat/output1/temperature
thermostat/output1/setpoint
thermostat/output1/mode
thermostat/output1/heating
thermostat/output1/power

thermostat/output2/temperature
thermostat/output2/setpoint
thermostat/output2/mode
thermostat/output2/heating
thermostat/output2/power

thermostat/output3/temperature
thermostat/output3/setpoint
thermostat/output3/mode
thermostat/output3/heating
thermostat/output3/power
```

#### B. Update Auto-Discovery
Create 3 separate Home Assistant auto-discovery configs:
```
homeassistant/climate/thermostat_output1/config
homeassistant/climate/thermostat_output2/config
homeassistant/climate/thermostat_output3/config
```

Each discovery message includes:
- Unique entity ID
- Friendly name (from output name)
- MQTT topics for that output
- Device info (groups all 3 under single device)

#### C. Update MQTT Manager Functions
**Files to modify:**
- `src/network/mqtt_manager.cpp`
- `include/mqtt_manager.h`

**Changes needed:**
1. Replace single output publishing with loop over 3 outputs
2. Update subscription handling for per-output commands
3. Update auto-discovery to send 3 configs
4. Add output index to topic paths

**Function updates:**
```cpp
// Old: mqtt_publish_status() - publishes output 1 only
// New: mqtt_publish_status() - publishes all 3 outputs

// Old: Subscribes to "thermostat/setpoint/set"
// New: Subscribes to "thermostat/output1/setpoint/set"
//                     "thermostat/output2/setpoint/set"
//                     "thermostat/output3/setpoint/set"
```

#### D. Testing Checklist
- [ ] All 3 climate entities appear in Home Assistant
- [ ] Each entity shows correct name from output config
- [ ] Temperature updates for each entity
- [ ] Target temperature changes work for each
- [ ] Mode changes work for each (heat/off)
- [ ] All 3 entities grouped under single device

**Estimated Time:** 1-1.5 hours

---

## 🎯 Priority 2: Schedule Page Multi-Output Update (MEDIUM)

### Goal
Update schedule page to support per-output scheduling with output selector.

### Current State
- Schedule page uses old single-output scheduler
- Shows 8 time slots globally
- No way to select which output to schedule

### Target State
- Dropdown to select Output 1, 2, or 3
- Shows that output's 8 schedule slots
- Save button updates selected output's schedule
- Optional: "Copy to other outputs" button

### Implementation Tasks

#### A. Add Output Selector
Add dropdown at top of schedule page:
```html
<select id="output-selector" onchange="loadSchedule()">
  <option value="1">Output 1 - Lights</option>
  <option value="2">Output 2 - Heat Mat</option>
  <option value="3">Output 3 - Ceramic Heater</option>
</select>
```

#### B. Update JavaScript
Modify schedule loading/saving to work with selected output:
```javascript
function loadSchedule() {
  let outputId = document.getElementById('output-selector').value;
  fetch('/api/output/' + outputId)
    .then(r => r.json())
    .then(d => {
      // Load schedule from d.schedule array
      // Populate form fields
    });
}

function saveSchedule() {
  let outputId = document.getElementById('output-selector').value;
  let scheduleData = [/* collect from form */];
  fetch('/api/output/' + outputId + '/config', {
    method: 'POST',
    body: JSON.stringify({schedule: scheduleData})
  });
}
```

#### C. Add Copy Feature (Optional)
Button to copy current schedule to other outputs:
```javascript
function copySchedule() {
  let sourceOutput = document.getElementById('output-selector').value;
  let targets = prompt('Copy to outputs (comma-separated, e.g., 2,3):');
  // Fetch source schedule
  // POST to each target output
}
```

#### D. Update Schedule Display
Show current output name in page title:
```html
<h2>Schedule - <span id="current-output-name">Output 1</span></h2>
```

**Files to modify:**
- `src/network/web_server.cpp` - `handleSchedule()` function

**Estimated Time:** 45 minutes - 1 hour

---

## 🎯 Priority 3: Mobile Responsive Improvements (LOW)

### Goal
Optimize web interface for mobile phones and tablets.

### Current State
- Responsive grid works but could be better
- Navigation bar can be cramped on small screens
- Forms may need better mobile styling
- Touch targets could be larger

### Implementation Tasks

#### A. CSS Media Queries
Add mobile-specific styles:
```css
@media (max-width: 768px) {
  /* Stack output cards vertically on mobile */
  .output-grid {
    grid-template-columns: 1fr !important;
  }

  /* Larger touch targets */
  button {
    min-height: 44px;
    font-size: 16px;
  }

  /* Responsive navigation */
  .nav {
    flex-wrap: wrap;
  }

  /* Full-width forms */
  input, select {
    width: 100% !important;
  }
}
```

#### B. Viewport Meta Tag
Ensure HTML header includes:
```html
<meta name="viewport" content="width=device-width, initial-scale=1">
```

#### C. Touch-Friendly Controls
- Increase button padding on mobile
- Larger slider controls
- Better spacing between form elements
- Ensure 44x44px minimum touch targets

#### D. Navigation Improvements
- Consider hamburger menu on very small screens
- Or horizontal scroll for nav items
- Sticky navigation on scroll

#### E. Testing
Test on:
- iPhone (Safari)
- Android phone (Chrome)
- iPad/tablet
- Various screen sizes in Chrome DevTools

**Files to modify:**
- `src/network/web_server.cpp` - `buildCSS()` function
- Add media queries to CSS

**Estimated Time:** 30-45 minutes

---

## 📋 Preparation Checklist

Before starting next session:

### For MQTT:
- [ ] Check current `mqtt_manager.cpp` implementation
- [ ] Verify Home Assistant version (for auto-discovery format)
- [ ] Note current MQTT topic structure
- [ ] Have Home Assistant credentials ready for testing

### For Schedule:
- [ ] Review current schedule page HTML
- [ ] Understand schedule slot structure (8 slots per output)
- [ ] Check if old scheduler.cpp is still being used

### For Mobile:
- [ ] Test current interface on phone/tablet
- [ ] Note specific issues or pain points
- [ ] Check viewport meta tag exists
- [ ] Review current CSS structure

---

## 🔄 Session Workflow

### Recommended Order:

1. **Start with MQTT** (45-60 min)
   - Read mqtt_manager.cpp
   - Update publishing logic
   - Update subscriptions
   - Update auto-discovery
   - Build and test

2. **Then Schedule** (30-45 min)
   - Update schedule page HTML
   - Add output selector
   - Update JS load/save functions
   - Build and test

3. **Finally Mobile** (30-45 min)
   - Add media queries to CSS
   - Test on devices
   - Refine as needed
   - Build and upload

4. **Final Testing** (30 min)
   - Test all 3 features together
   - Verify nothing broke
   - Test on multiple devices/browsers
   - Document any issues

**Total Estimated Time:** 2-3 hours

---

## 🎯 Success Criteria

### MQTT Complete When:
- [ ] Home Assistant shows 3 separate climate entities
- [ ] Each entity has unique name from output config
- [ ] All entities update temperatures independently
- [ ] Control from HA works for all 3 outputs
- [ ] All entities grouped under one device

### Schedule Complete When:
- [ ] Output selector dropdown works
- [ ] Can view each output's schedule
- [ ] Can edit each output's schedule
- [ ] Save persists to correct output
- [ ] Schedule actually activates at correct times

### Mobile Complete When:
- [ ] Interface usable on phone (320px+ width)
- [ ] All buttons easily tappable
- [ ] No horizontal scrolling needed
- [ ] Forms are full-width and easy to fill
- [ ] Navigation accessible on all screen sizes

---

## 💡 Quick Reference

### File Locations:
```
MQTT:     src/network/mqtt_manager.cpp
          include/mqtt_manager.h

Schedule: src/network/web_server.cpp (handleSchedule function)

Mobile:   src/network/web_server.cpp (buildCSS function)
```

### Key APIs Already Available:
```
GET  /api/output/{id}        - Get output with schedule
POST /api/output/{id}/config - Save schedule to output
GET  /api/outputs            - Get all outputs for MQTT
```

### Helper Functions Available:
```cpp
output_manager_get_output(index)           - Get output config
output_manager_set_schedule_slot(...)      - Set schedule slot
sensor_manager_get_sensor(index)           - Get sensor info
```

---

## 🚀 After Session Complete

### Documentation to Update:
1. Update `SESSION_SUMMARY.md` with new features
2. Update `QUICK_START.md` with MQTT setup instructions
3. Add schedule page usage to quick start
4. Note mobile optimization in docs

### Next Priorities (Future):
1. Remove old unused modules (cleanup)
2. Advanced features (grouping, calibration)
3. Performance optimizations
4. Additional polish and refinements

---

**Ready to Start:** ✅ All prerequisites met
**Next Session Focus:** MQTT → Schedule → Mobile
**Expected Outcome:** Complete multi-output system with full Home Assistant integration

---

**Notes:**
- Take breaks between priorities
- Build and test after each major change
- Don't rush - quality over speed
- Keep commits organized (one feature per commit if using git)
- Save work frequently
