# Dark Mode Color Issues and Suggestions

Analysis performed: 2026-01-29

## Executive Summary

The dark mode has significant readability issues because many elements use **inline styles** that override the CSS dark mode rules. The main problems are:
- Light backgrounds that don't change in dark mode
- Dark text (`#333`, `#666`) that becomes invisible on dark backgrounds
- Light text (`#f0f0f0`) on elements that retain light backgrounds

---

## Color Palette Recommendation

### Dark Mode Base Colors
| Purpose | Current | Suggested | Notes |
|---------|---------|-----------|-------|
| Page background | `#121212` | `#121212` | Good |
| Container background | `#1e1e1e` | `#1e1e1e` | Good |
| Card/Panel background | `#f9f9f9` (light!) | `#2d2d2d` | Needs fix |
| Active card background | `#e8f5e9` (light green) | `#1e3d1f` | Dark green |
| Heating card background | `#ffebee` (light pink) | `#3d1f1f` | Dark red |
| Info box background | `#e3f2fd` (light blue) | `#1a2a3a` | Has CSS but inline overrides |
| Primary text | `#f0f0f0` | `#f0f0f0` | Good |
| Secondary text | `#666666` (dark!) | `#b0b0b0` | Needs fix |
| Muted text | `#333333` (dark!) | `#d0d0d0` | Needs fix |
| Border color | `#ddd` | `#3d3d3d` | Needs fix for inline styles |

---

## Page-by-Page Issues and Fixes

### Home Page (`/`)

**Critical Issues:**

1. **Output Cards Background**
   - Location: `web_server.cpp` lines 905, 918
   - Current: `#e8f5e9` (not heating) / `#ffebee` (heating)
   - Problem: Light backgrounds with light text = invisible
   - **Fix:** Add dark mode CSS or modify JavaScript:
   ```css
   body.dark-mode [id^='output'] { background: #2d2d2d !important; }
   body.dark-mode [id^='output'].heating { background: #3d1f1f !important; }
   body.dark-mode [id^='output'].not-heating { background: #1e3d1f !important; }
   ```

2. **Output Card Text**
   - Location: `web_server.cpp` lines 2410-2412
   - Current: `color:#333` for h3, div, strong inside output cards
   - Problem: Dark text invisible on dark backgrounds
   - **Fix:** Add dark mode override:
   ```css
   body.dark-mode [id^='output'] h3 { color: #f0f0f0; }
   body.dark-mode [id^='output'] div { color: #d0d0d0; }
   body.dark-mode [id^='output'] strong { color: #f0f0f0; }
   ```

3. **Temperature Display**
   - Location: `web_server.cpp` lines 736-737
   - Current: `.temp-display{color:#333}`, `small{color:#666}`
   - **Fix:**
   ```css
   body.dark-mode .temp-display { color: #f0f0f0; }
   body.dark-mode .temp-display small { color: #b0b0b0; }
   ```

4. **Target/Mode/Power Row Labels**
   - Location: `web_server.cpp` lines 739, 743, 747
   - Current: `color:#666`
   - **Fix:**
   ```css
   body.dark-mode .target-row label,
   body.dark-mode .mode-row label,
   body.dark-mode .power-row label { color: #b0b0b0; }
   ```

5. **Card Headings**
   - Location: `web_server.cpp` line 735
   - Current: `.simple-card h3{color:#333}`
   - **Fix:**
   ```css
   body.dark-mode .simple-card h3 { color: #f0f0f0; }
   ```

6. **Footer Help Text**
   - Location: `web_server.cpp` line 945
   - Current: Inline `style='color:#666'`
   - **Fix:** Remove inline style or add:
   ```css
   body.dark-mode .footer p, body.dark-mode p[style*='color:#666'] { color: #b0b0b0 !important; }
   ```

---

### Settings Page (`/settings`)

**Issues:**

1. **PIN Help Text**
   - Location: `web_server.cpp` line 1419
   - Current: Inline `style='color:#666'`
   - Shows as `rgb(102, 102, 102)` - too dark for dark mode
   - **Fix:** Same as above, use CSS class instead of inline style

**Working Well:**
- Info boxes (Status, Firmware) - CSS rule exists at line 2440
- Inputs and labels - properly styled
- Buttons - properly styled

---

### Safety Page (`/safety`)

**Issues:**

1. **Settings Panel Background**
   - Location: `web_server.cpp` line 2702
   - Current: Inline `background:#f9f9f9`
   - **Fix:**
   ```css
   body.dark-mode div[style*='background:#f9f9f9'] { background: #2d2d2d !important; }
   ```

2. **Help Text Throughout**
   - Locations: lines 2678, 2712, 2720, 2728, 2752, 2760
   - Current: Inline `color:#666`
   - **Fix:** Use a CSS class `.help-text` and add dark mode rule

3. **Fault Analysis Panel**
   - Location: `web_server.cpp` line 2775
   - Current: Inline `background:#f9f9f9`
   - Same fix as settings panel

4. **Info Boxes**
   - Locations: lines 2655, 2661, 2667
   - Current: Inline `background:#e3f2fd`
   - CSS rule exists but inline style overrides it
   - **Fix:** Remove inline styles or use `!important`

---

### Schedule Page (`/schedule`)

**Critical Issues:**

1. **Schedule Slot Backgrounds**
   - Location: `web_server.cpp` line 1585 (JavaScript-generated)
   - Current: `#f1f8f4` (active) / `#f9f9f9` (inactive)
   - Problem: Light backgrounds make dark-mode text invisible
   - **Fix:** Update JavaScript to check dark mode:
   ```javascript
   let isDark = document.body.classList.contains('dark-mode');
   let activeBg = isDark ? '#1e3d1f' : '#f1f8f4';
   let inactiveBg = isDark ? '#2d2d2d' : '#f9f9f9';
   let borderColor = isDark ? '#3d3d3d' : '#ddd';
   let activeBorder = isDark ? '#4CAF50' : '#4CAF50';
   ```

2. **Output Info Text**
   - Location: `web_server.cpp` line 1514
   - Current: Inline `color:#666`
   - **Fix:** Use CSS class

3. **Schedule Tips Box**
   - Location: `web_server.cpp` line 1525
   - Current: Inline `background:#e3f2fd`
   - Has CSS rule but inline overrides
   - **Fix:** Remove inline style or use class

4. **Day Selector Buttons**
   - Generated in JavaScript with inline styles
   - Need to add dark mode aware colors:
   ```javascript
   let selectedBg = isDark ? '#2d5f2e' : '#4CAF50';
   let unselectedBg = isDark ? '#3d3d3d' : '#ddd';
   let selectedText = 'white';
   let unselectedText = isDark ? '#b0b0b0' : '#666';
   ```

---

### Outputs Page (`/outputs`)

**Issues:**

1. **Config Panel Background**
   - Location: `web_server.cpp` line 427
   - Current: Inline `background:#f9f9f9`
   - **Fix:** Add dark mode CSS rule

2. **Device Info Box**
   - Location: `web_server.cpp` line 446
   - Current: Inline `background:#e3f2fd`
   - **Fix:** Add dark mode override

3. **PID/Time-Prop Help Text**
   - Locations: lines 487, 496, 459
   - Current: Inline `color:#666`
   - **Fix:** Use CSS class

4. **Help Box**
   - Location: `web_server.cpp` line 550
   - Current: Inline `background:#e3f2fd`
   - **Fix:** Add dark mode override

---

### Sensors Page (`/sensors`)

**Working:** Mostly fine, uses table headers with proper styling.

---

### Logs Page (`/logs`)

**Issues:**

1. **Log Container Background**
   - Location: `web_server.cpp` line 1212
   - Current: Inline `background:#f9f9f9`
   - **Fix:** Add dark mode CSS rule

**Working:** Log entries have CSS rule at line 2438.

---

### History Page (`/history`)

**Working:** Uses canvas for chart, no major text issues.

---

### Info Page (`/info`)

**Working:** Stat cards properly styled with dark mode CSS.

---

## Recommended Implementation Strategy

### Option A: CSS-Only Fix (Simpler, uses `!important`)

Add these rules to the dark mode CSS section (around line 2447):

```cpp
// Output cards
css += "body.dark-mode [id^='output']{background:#2d2d2d !important}";
css += "body.dark-mode [id^='output'] h3{color:#f0f0f0}";
css += "body.dark-mode [id^='output'] div{color:#d0d0d0}";
css += "body.dark-mode [id^='output'] strong{color:#f0f0f0}";

// Temperature display
css += "body.dark-mode .temp-display{color:#f0f0f0}";
css += "body.dark-mode .temp-display small{color:#b0b0b0}";

// Simple card styling
css += "body.dark-mode .simple-card h3{color:#f0f0f0}";
css += "body.dark-mode .target-row label,body.dark-mode .mode-row label,body.dark-mode .power-row label{color:#b0b0b0}";

// Panel backgrounds (override inline styles)
css += "body.dark-mode div[style*='background:#f9f9f9']{background:#2d2d2d !important}";
css += "body.dark-mode div[style*='background:#e3f2fd']{background:#1a2a3a !important;color:#d0f0ff !important}";
css += "body.dark-mode div[style*='background:#f1f8f4']{background:#1e3d1f !important}";

// Help text (override inline color:#666)
css += "body.dark-mode p[style*='color:#666'],body.dark-mode span[style*='color:#666']{color:#b0b0b0 !important}";

// Schedule slots
css += "body.dark-mode .schedule-slot,body.dark-mode #schedule-slots>div{background:#2d2d2d !important;border-color:#3d3d3d !important}";
```

### Option B: Remove Inline Styles (Better, more maintainable)

1. Replace inline `style='color:#666'` with `class='help-text'`
2. Replace inline `style='background:#f9f9f9'` with `class='panel'`
3. Replace inline `style='background:#e3f2fd'` with `class='info-panel'`
4. Add corresponding CSS rules for both light and dark modes

### Option C: JavaScript Dark Mode Awareness (For dynamic elements)

For JavaScript-generated elements (output cards, schedule slots), check dark mode:

```javascript
let isDark = document.body.classList.contains('dark-mode');
// Then use appropriate colors based on isDark
```

---

## Testing Checklist

After implementing fixes:

- [ ] Home page: Output cards readable in dark mode
- [ ] Home page: Temperature display visible
- [ ] Home page: Help text readable
- [ ] Settings page: PIN help text readable
- [ ] Safety page: All panels have dark backgrounds
- [ ] Safety page: Help text readable
- [ ] Schedule page: Schedule slots have dark backgrounds
- [ ] Schedule page: Day selector buttons visible
- [ ] Schedule page: Tips box has dark background
- [ ] Outputs page: Config panel dark
- [ ] Outputs page: Help text readable
- [ ] Logs page: Log container dark
- [ ] All pages: No white/light backgrounds remaining
- [ ] All pages: No dark text on dark backgrounds
- [ ] Toggle dark mode on/off - all elements transition smoothly
