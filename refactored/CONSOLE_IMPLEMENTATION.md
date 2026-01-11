# Live Console Feature - Implementation Status

## ✅ Completed Components

### 1. Console Event Buffer Module
**Files Created:**
- `include/console.h` - Interface with event types
- `src/utils/console.cpp` - Ring buffer implementation (50 events)

**Features:**
- Circular buffer for system events
- Event types: SYSTEM, MQTT, WIFI, TEMP, PID, SCHEDULE, ERROR, DEBUG
- Timestamped entries
- Color-coded by type
- Automatic serial output

### 2. MQTT Activity Logging
**Modified:**
- `src/network/mqtt_manager.cpp` - Added console logging to mqtt_publish_temperature()

**Next MQTT Functions to Log:**
- mqtt_publish_state()
- mqtt_publish_mode()
- mqtt_publish_status_extended()
- mqtt_connect() / mqtt_disconnect()
- mqttCallback() (incoming messages)

### 3. Web Server Routes
**Modified:**
- `src/network/web_server.cpp` - Added route declarations
  - `/console` - Console page
  - `/api/console` - Get console events as JSON

## 🔧 TODO: Complete Implementation

### Step 1: Add Console Event Handlers to web_server.cpp

Add after `handleLogs()` function (around line 528):

```cpp
/**
 * Handle /api/console - Get console events
 */
static void handleConsoleEvents(void) {
    DynamicJsonDocument doc(4096);
    JsonArray events = doc.createNestedArray("events");

    int count = console_get_count();
    for (int i = 0; i < count; i++) {
        const char* event = console_get_event(i);
        ConsoleEventType_t type = console_get_event_type(i);

        if (event != nullptr) {
            JsonObject evt = events.createNestedObject();
            evt["message"] = event;
            evt["type"] = console_get_type_name(type);
        }
    }

    doc["count"] = count;

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

/**
 * Handle /console page
 */
static void handleConsole(void) {
    String html = webserver_get_html_header("Live Console", "console");

    html += "<h2>System Console</h2>";
    html += "<div class='info-box'>Real-time system events, MQTT activity, and debug messages</div>";

    html += "<div style='margin-bottom:15px'>";
    html += "<button onclick='refreshConsole()' class='btn-secondary' style='margin-right:10px'>Refresh</button>";
    html += "<button onclick='clearConsole()' class='btn-secondary' style='margin-right:10px'>Clear</button>";
    html += "<label style='margin-left:20px'><input type='checkbox' id='autoRefresh' checked> Auto-refresh (2s)</label>";
    html += "</div>";

    html += "<div id='console-output' style='background:#1e1e1e;color:#d4d4d4;font-family:\"Courier New\",monospace;font-size:13px;padding:15px;border-radius:5px;height:600px;overflow-y:auto;'>";
    html += "<div style='color:#888'>Loading console...</div>";
    html += "</div>";

    // JavaScript for auto-refresh and formatting
    html += "<script>";
    html += "let autoRefreshTimer=null;";
    html += "function refreshConsole(){";
    html += "fetch('/api/console').then(r=>r.json()).then(data=>{";
    html += "const out=document.getElementById('console-output');out.innerHTML='';";
    html += "if(data.events&&data.events.length>0){";
    html += "data.events.forEach(evt=>{";
    html += "const div=document.createElement('div');div.style.marginBottom='2px';";
    html += "let color='#d4d4d4';";
    html += "if(evt.type==='ERROR')color='#f48771';";
    html += "else if(evt.type==='MQTT')color='#4ec9b0';";
    html += "else if(evt.type==='WIFI')color='#dcdcaa';";
    html += "else if(evt.type==='SYSTEM')color='#569cd6';";
    html += "div.innerHTML='<span style=\"color:'+color+'\">['+evt.type+']</span> '+evt.message;";
    html += "out.appendChild(div);";
    html += "});";
    html += "out.scrollTop=out.scrollHeight;";
    html += "}else{out.innerHTML='<div style=\"color:#888\">No console events yet...</div>';}";
    html += "}).catch(err=>console.error('Error:',err));}";
    html += "function clearConsole(){";
    html += "if(confirm('Clear all console events?')){";
    html += "fetch('/api/console-clear',{method:'POST'}).then(()=>refreshConsole());";
    html += "}}";
    html += "function toggleAutoRefresh(){";
    html += "if(document.getElementById('autoRefresh').checked){";
    html += "autoRefreshTimer=setInterval(refreshConsole,2000);";
    html += "}else{if(autoRefreshTimer)clearInterval(autoRefreshTimer);}}";
    html += "document.getElementById('autoRefresh').addEventListener('change',toggleAutoRefresh);";
    html += "refreshConsole();toggleAutoRefresh();";
    html += "</script>";

    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}
```

### Step 2: Add Console Clear Endpoint

In `webserver_init()`, after other routes:

```cpp
server.on("/api/console-clear", HTTP_POST, []() {
    console_clear();
    server.send(200, "text/plain", "OK");
});
```

### Step 3: Update Navigation Bar

In `buildNavBar()` function (around line 1164):

```cpp
nav += "<a href='/console' class='" + String(strcmp(activePage, "console") == 0 ? "active" : "") + "'>🖥️ Console</a>";
```

Add after the logs link.

### Step 4: Initialize Console in main.cpp

In `setup()` after logger_init():

```cpp
console_init();
console_add_event(CONSOLE_EVENT_SYSTEM, "System boot - v" FIRMWARE_VERSION);
```

### Step 5: Add Console Events Throughout Code

**In main.cpp:**
```cpp
// In readTemperature()
console_add_event_f(CONSOLE_EVENT_TEMP, "Temperature: %.1f°C", temp);

// In loop() when mode changes
console_add_event_f(CONSOLE_EVENT_SYSTEM, "Mode changed to: %s", mode);

// In PID calculations
console_add_event_f(CONSOLE_EVENT_PID, "PID output: %d%%", power);
```

**In wifi_manager.cpp:**
```cpp
console_add_event(CONSOLE_EVENT_WIFI, "WiFi connected");
console_add_event_f(CONSOLE_EVENT_WIFI, "IP: %s", ip);
```

**In mqtt_manager.cpp:**
```cpp
// In mqtt_connect()
console_add_event(CONSOLE_EVENT_MQTT, "MQTT connecting...");
console_add_event(CONSOLE_EVENT_MQTT, "MQTT connected");

// In mqttCallback()
console_add_event_f(CONSOLE_EVENT_MQTT, "MQTT RCV: %s = %s", topic, payload);
```

## Expected Features

When complete, the `/console` page will show:

✅ Real-time system events
✅ MQTT publish/subscribe activity
✅ WiFi connection events
✅ Temperature readings
✅ PID calculations
✅ Scheduler activations
✅ Errors and warnings

✅ Auto-refresh every 2 seconds
✅ Color-coded by event type
✅ Clear button
✅ Pause/resume auto-refresh
✅ Last 50 events

## Testing

1. Build and upload
2. Navigate to `http://192.168.1.236/console`
3. Watch events stream in real-time
4. Test MQTT activity by changing temperature via Home Assistant
5. Verify color coding and auto-refresh
