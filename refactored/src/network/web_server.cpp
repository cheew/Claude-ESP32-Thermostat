/**
 * web_server.c
 * HTTP Web Server and API Endpoints Implementation
 */

#include "web_server.h"
#include "scheduler.h"
#include "logger.h"
#include "temp_history.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Web server instance
static WebServer server(80);

// Callbacks
static TempModeCallback_t controlCallback = NULL;
static ScheduleSaveCallback_t scheduleCallback = NULL;
static WebServerCallback_t restartCallback = NULL;

// State variables (updated by main code)
static float currentTemp = 0.0;
static float targetTemp = 28.0;
static bool heatingState = false;
static char currentMode[8] = "auto";
static int powerOutput = 0;

// Device info
static char deviceName[32] = "Thermostat";
static char firmwareVersion[16] = "1.3.3";

// Network status
static bool networkConnected = false;
static bool networkAPMode = false;
static char networkSSID[33] = "";
static char networkIP[16] = "";

// Logging (deprecated - using logger module now)
#define MAX_LOGS 20

// Schedule data (stored as void* to avoid circular dependency)
static bool scheduleEnabled = false;
static int scheduleSlotCount = 0;
static void* scheduleSlots = NULL;

// GitHub auto-update configuration
static const char* GITHUB_USER = "cheew";
static const char* GITHUB_REPO = "Claude-ESP32-Thermostat";
static const char* GITHUB_FIRMWARE = "firmware.bin";

// Forward declarations - Route handlers
static void handleRoot(void);
static void handleSchedule(void);
static void handleHistoryPage(void);
static void handleInfo(void);
static void handleLogs(void);
static void handleSettings(void);
static void handleStatus(void);
static void handleHistory(void);
static void handleSet(void);
static void handleSaveSettings(void);
static void handleSaveSchedule(void);
static void handleRestart(void);
static void handleUpdate(void);
static void handleUpload(void);
static void handleUploadDone(void);
static void handleCheckUpdate(void);
static void handleAutoUpdate(void);

// HTML generation helpers
static String buildCSS(void);
static String buildNavBar(const char* activePage);

/**
 * Initialize web server
 */
void webserver_init(void) {
    Serial.println("[WebServer] Initializing web server");
    
    // Setup routes
    server.on("/", handleRoot);
    server.on("/schedule", handleSchedule);
    server.on("/history", handleHistoryPage);
    server.on("/info", handleInfo);
    server.on("/logs", handleLogs);
    server.on("/settings", handleSettings);
    
    // API endpoints
    server.on("/api/status", handleStatus);
    server.on("/api/history", handleHistory);
    server.on("/api/set", HTTP_POST, handleSet);
    server.on("/api/save-settings", HTTP_POST, handleSaveSettings);
    server.on("/api/save-schedule", HTTP_POST, handleSaveSchedule);
    server.on("/api/restart", HTTP_POST, handleRestart);
    server.on("/api/check-update", handleCheckUpdate);
    server.on("/api/auto-update", HTTP_POST, handleAutoUpdate);
    
    // OTA update routes
    server.on("/update", handleUpdate);
    server.on("/api/upload", HTTP_POST, handleUploadDone, handleUpload);
    
    server.begin();
    Serial.println("[WebServer] Server started on port 80");
}

/**
 * Web server task
 */
void webserver_task(void) {
    server.handleClient();
}

/**
 * Set control callback
 */
void webserver_set_control_callback(TempModeCallback_t callback) {
    controlCallback = callback;
}

/**
 * Set schedule callback
 */
void webserver_set_schedule_callback(ScheduleSaveCallback_t callback) {
    scheduleCallback = callback;
}

/**
 * Set restart callback
 */
void webserver_set_restart_callback(WebServerCallback_t callback) {
    restartCallback = callback;
}

/**
 * Set current state
 */
void webserver_set_state(float temp, float target, bool heating, 
                         const char* mode, int power) {
    currentTemp = temp;
    targetTemp = target;
    heatingState = heating;
    strncpy(currentMode, mode, sizeof(currentMode) - 1);
    powerOutput = power;
}

/**
 * Set device info
 */
void webserver_set_device_info(const char* name, const char* version) {
    strncpy(deviceName, name, sizeof(deviceName) - 1);
    strncpy(firmwareVersion, version, sizeof(firmwareVersion) - 1);
}

/**
 * Set network status
 */
void webserver_set_network_status(bool connected, bool apMode, 
                                  const char* ssid, const char* ip) {
    networkConnected = connected;
    networkAPMode = apMode;
    strncpy(networkSSID, ssid, sizeof(networkSSID) - 1);
    strncpy(networkIP, ip, sizeof(networkIP) - 1);
}

/**
 * Add log entry (deprecated - use logger module)
 */
void webserver_add_log(const char* message) {
    // Forward to logger module
    logger_add(message);
}

/**
 * Set schedule data
 */
void webserver_set_schedule_data(bool enabled, int slotCount, void* slots) {
    scheduleEnabled = enabled;
    scheduleSlotCount = slotCount;
    scheduleSlots = slots;
}

// ===== ROUTE HANDLERS =====

/**
 * Handle root page (/)
 */
static void handleRoot(void) {
    String html = webserver_get_html_header("Home", "home");
    
    if (networkAPMode) {
        html += "<div class='warning-box'><strong>⚠️ AP Mode Active</strong><br>";
        html += "Configure WiFi in <a href='/settings'>Settings</a></div>";
    }
    
    // Auto-refresh script
    html += "<script>setInterval(()=>fetch('/api/status').then(r=>r.json()).then(d=>{";
    html += "document.getElementById('temp').innerText=d.temperature+'°C';";
    html += "document.getElementById('target').innerText=d.setpoint+'°C';";
    html += "document.getElementById('heating').innerText=d.heating?'ON':'OFF';";
    html += "document.getElementById('mode').innerText=d.mode.toUpperCase();";
    html += "document.getElementById('power-val').innerText=d.power+'%';";
    html += "document.getElementById('power-fill').style.width=d.power+'%';";
    html += "let statusDiv=document.querySelector('.status');";
    html += "statusDiv.style.background=d.heating?'#ffebee':'#e8f5e9';";
    html += "}),2000);</script>";
    
    // Status display
    html += "<h2>Current Status</h2>";
    html += "<div class='status' style='background:" + String(heatingState ? "#ffebee" : "#e8f5e9") + "'>";
    html += "<div><strong>Current:</strong> <span id='temp'>" + String(currentTemp, 1) + "°C</span></div>";
    html += "<div><strong>Target:</strong> <span id='target'>" + String(targetTemp, 1) + "°C</span></div>";
    html += "<div><strong>Heating:</strong> <span id='heating'>" + String(heatingState ? "ON" : "OFF") + "</span></div>";
    html += "<div><strong>Mode:</strong> <span id='mode'>" + String(currentMode) + "</span></div>";
    html += "</div>";
    
    // Power output visualization
    html += "<div style='margin:20px 0'><strong>Power Output: <span id='power-val'>" + String(powerOutput) + "%</span></strong>";
    html += "<div style='width:100%;height:30px;background:#ddd;border-radius:5px;overflow:hidden'>";
    html += "<div id='power-fill' style='height:100%;background:linear-gradient(90deg,#4CAF50,#ff9800);transition:width 0.3s;width:" + String(powerOutput) + "%'></div></div></div>";
    
    // Control form
    html += "<form action='/api/set' method='POST'>";
    html += "<h2>Temperature Control</h2>";
    html += "<div class='control'><label>Target Temperature (°C):</label>";
    html += "<input type='number' name='target' value='" + String(targetTemp, 1) + "' step='0.5' min='15' max='45'></div>";
    html += "<div class='control'><label>Mode:</label>";
    html += "<select name='mode'>";
    html += "<option value='auto'" + String(strcmp(currentMode, "auto") == 0 ? " selected" : "") + ">Auto (PID)</option>";
    html += "<option value='on'" + String(strcmp(currentMode, "on") == 0 ? " selected" : "") + ">Manual On (100%)</option>";
    html += "<option value='off'" + String(strcmp(currentMode, "off") == 0 ? " selected" : "") + ">Off</option>";
    html += "</select></div>";
    html += "<button type='submit'>Update Settings</button></form>";
    
    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}

/**
 * Handle /api/status
 */
static void handleStatus(void) {
    StaticJsonDocument<256> doc;
    doc["temperature"] = round(currentTemp * 10) / 10.0;
    doc["setpoint"] = targetTemp;
    doc["heating"] = heatingState;
    doc["mode"] = currentMode;
    doc["power"] = powerOutput;
    
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

/**
 * Handle /api/history
 */
static void handleHistory(void) {
    DynamicJsonDocument doc(8192);  // Large buffer for history data
    JsonArray data = doc.createNestedArray("data");

    int count = temp_history_get_count();
    for (int i = 0; i < count; i++) {
        const TempHistoryPoint_t* point = temp_history_get_point(i);
        if (point != nullptr) {
            JsonObject reading = data.createNestedObject();
            reading["timestamp"] = point->timestamp;
            reading["temperature"] = round(point->temperature * 10) / 10.0;
        }
    }

    doc["count"] = count;
    doc["interval"] = HISTORY_SAMPLE_INTERVAL / 1000;  // Convert to seconds

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

/**
 * Handle /api/set
 */
static void handleSet(void) {
    float newTarget = targetTemp;
    String newMode = String(currentMode);
    
    if (server.hasArg("target")) {
        newTarget = server.arg("target").toFloat();
        newTarget = constrain(newTarget, 15.0, 45.0);
    }
    
    if (server.hasArg("mode")) {
        String argMode = server.arg("mode");
        if (argMode == "auto" || argMode == "off" || argMode == "on") {
            newMode = argMode;
        }
    }
    
    // Call callback if registered
    if (controlCallback != NULL) {
        controlCallback(newTarget, newMode.c_str());
    }
    
    server.sendHeader("Location", "/");
    server.send(303);
}

/**
 * Handle info page
 */
static void handleInfo(void) {
    String html = webserver_get_html_header("Device Info", "info");
    
    if (networkAPMode) {
        html += "<div class='warning-box'><strong>⚠️ AP Mode Active</strong><br>";
        html += "Connect to WiFi network in <a href='/settings'>Settings</a></div>";
    }
    
    html += "<h2>Device Information</h2>";
    html += "<div class='stat-grid'>";
    html += "<div class='stat-card'><div class='stat-value'>" + String(deviceName) + "</div><div class='stat-label'>Device Name</div></div>";
    html += "<div class='stat-card'><div class='stat-value'>" + String(firmwareVersion) + "</div><div class='stat-label'>Firmware</div></div>";
    
    unsigned long uptime = millis() / 1000;
    unsigned long days = uptime / 86400;
    unsigned long hours = (uptime % 86400) / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    String uptimeStr = "";
    if (days > 0) uptimeStr += String(days) + "d ";
    uptimeStr += String(hours) + "h " + String(minutes) + "m";
    
    html += "<div class='stat-card'><div class='stat-value'>" + uptimeStr + "</div><div class='stat-label'>Uptime</div></div>";
    html += "<div class='stat-card'><div class='stat-value'>" + String(ESP.getFreeHeap() / 1024) + " KB</div><div class='stat-label'>Free Memory</div></div>";
    html += "</div>";
    
    html += "<h2>Network Status</h2>";
    html += "<div class='info-box'>";
    if (networkConnected) {
        html += "<strong>WiFi:</strong> Connected ✓<br>";
        html += "<strong>SSID:</strong> " + String(networkSSID) + "<br>";
        html += "<strong>IP Address:</strong> " + String(networkIP) + "<br>";
        html += "<strong>Signal Strength:</strong> " + String(WiFi.RSSI()) + " dBm<br>";
        html += "<strong>MAC Address:</strong> " + WiFi.macAddress();
    } else if (networkAPMode) {
        html += "<strong>Mode:</strong> Access Point<br>";
        html += "<strong>SSID:</strong> " + String(networkSSID) + "<br>";
        html += "<strong>IP Address:</strong> " + String(networkIP);
    } else {
        html += "<strong>WiFi:</strong> Not Connected ✗";
    }
    html += "</div>";
    
    html += "<h2>Sensor Information</h2>";
    html += "<div class='stat-grid'>";
    html += "<div class='stat-card'><div class='stat-value'>" + String(currentTemp, 1) + "°C</div><div class='stat-label'>Current Temp</div></div>";
    html += "<div class='stat-card'><div class='stat-value'>" + String(targetTemp, 1) + "°C</div><div class='stat-label'>Target Temp</div></div>";
    html += "<div class='stat-card'><div class='stat-value'>" + String(powerOutput) + "%</div><div class='stat-label'>Power Output</div></div>";
    html += "<div class='stat-card'><div class='stat-value'>" + String(currentMode) + "</div><div class='stat-label'>Mode</div></div>";
    html += "</div>";
    
    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}

/**
 * Handle logs page
 */
static void handleLogs(void) {
    String html = webserver_get_html_header("System Logs", "logs");
    
    html += "<h2>Recent Events</h2>";
    html += "<div class='info-box'>Showing last " + String(MAX_LOGS) + " log entries (newest first)</div>";
    
    html += "<div style='background:#f9f9f9;border-radius:5px;padding:10px;max-height:500px;overflow-y:auto'>";
    
    bool hasLogs = false;
    int logCount = logger_get_count();
    for (int i = 0; i < logCount; i++) {
        const char* logEntry = logger_get_entry(i);
        if (logEntry != nullptr) {
            html += "<div class='log-entry'>" + String(logEntry) + "</div>";
            hasLogs = true;
        }
    }
    
    if (!hasLogs) {
        html += "<div class='log-entry'>No logs yet...</div>";
    }
    
    html += "</div>";
    html += "<div style='margin-top:20px'><button onclick='location.reload()' class='btn-secondary'>Refresh Logs</button></div>";
    
    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}

/**
 * Handle history page
 */
static void handleHistoryPage(void) {
    String html = webserver_get_html_header("Temperature History", "history");

    html += "<h2>Temperature History</h2>";

    int count = temp_history_get_count();
    html += "<div class='info-box'>";
    html += "Recording every 5 minutes. Currently storing " + String(count) + " readings";
    if (count >= HISTORY_BUFFER_SIZE) {
        html += " (last 24 hours)";
    }
    html += "</div>";

    // Chart canvas
    html += "<div style='background:white;padding:15px;border-radius:5px;margin-top:15px'>";
    html += "<canvas id='tempChart' style='width:100%;max-height:400px'></canvas>";
    html += "</div>";

    // Chart.js from CDN
    html += "<script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js'></script>";
    html += "<script>";
    html += "fetch('/api/history').then(r=>r.json()).then(data=>{";
    html += "const ctx=document.getElementById('tempChart').getContext('2d');";
    html += "new Chart(ctx,{";
    html += "type:'line',";
    html += "data:{";
    html += "labels:data.data.map(d=>new Date(d.timestamp*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})),";
    html += "datasets:[{";
    html += "label:'Temperature (°C)',";
    html += "data:data.data.map(d=>d.temperature),";
    html += "borderColor:'rgb(255,99,71)',";
    html += "backgroundColor:'rgba(255,99,71,0.1)',";
    html += "tension:0.4,";
    html += "fill:true";
    html += "}]},";
    html += "options:{";
    html += "responsive:true,";
    html += "maintainAspectRatio:false,";
    html += "plugins:{legend:{display:true}},";
    html += "scales:{";
    html += "y:{beginAtZero:false,title:{display:true,text:'Temperature (°C)'}},";
    html += "x:{title:{display:true,text:'Time'}}";
    html += "}}";
    html += "});";
    html += "}).catch(e=>console.error('Error loading history:',e));";
    html += "</script>";

    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}

/**
 * Handle settings page
 */
static void handleSettings(void) {
    String html = webserver_get_html_header("Settings", "settings");
    
    // Load settings from preferences
    Preferences prefs;
    prefs.begin("thermostat", true);
    String savedSSID = prefs.getString("wifi_ssid", "");
    String savedMQTTBroker = prefs.getString("mqtt_broker", "192.168.1.123");
    String savedMQTTUser = prefs.getString("mqtt_user", "admin");
    float kp = prefs.getFloat("Kp", 10.0);
    float ki = prefs.getFloat("Ki", 0.5);
    float kd = prefs.getFloat("Kd", 5.0);
    prefs.end();
    
    if (networkAPMode) {
        html += "<div class='warning-box'><strong>⚠️ AP Mode Active</strong><br>";
        html += "Connect to WiFi to access all settings</div>";
    } else {
        html += "<div class='info-box'><strong>Status:</strong> Connected to WiFi ✓<br>";
        html += "<strong>IP:</strong> " + String(networkIP) + "</div>";
    }
    
    html += "<form action='/api/save-settings' method='POST'>";
    html += "<h2>Device Settings</h2>";
    html += "<div class='control'><label>Device Name:</label>";
    html += "<input type='text' name='device_name' value='" + String(deviceName) + "' maxlength='15'></div>";
    
    html += "<h2>WiFi Configuration</h2>";
    html += "<div class='control'><label>WiFi SSID:</label>";
    html += "<input type='text' name='wifi_ssid' value='" + savedSSID + "' required></div>";
    html += "<div class='control'><label>WiFi Password:</label>";
    html += "<input type='password' name='wifi_pass' placeholder='Enter new password or leave blank'></div>";
    
    html += "<h2>MQTT Configuration</h2>";
    html += "<div class='control'><label>MQTT Broker IP:</label>";
    html += "<input type='text' name='mqtt_broker' value='" + savedMQTTBroker + "' required></div>";
    html += "<div class='control'><label>MQTT Port:</label>";
    html += "<input type='number' name='mqtt_port' value='1883' required></div>";
    html += "<div class='control'><label>MQTT Username:</label>";
    html += "<input type='text' name='mqtt_user' value='" + savedMQTTUser + "'></div>";
    html += "<div class='control'><label>MQTT Password:</label>";
    html += "<input type='password' name='mqtt_pass' placeholder='Enter new password or leave blank'></div>";
    
    html += "<h2>PID Tuning</h2>";
    html += "<div class='control'><label>Kp (Proportional):</label>";
    html += "<input type='number' name='kp' value='" + String(kp, 2) + "' step='0.1' min='0'></div>";
    html += "<div class='control'><label>Ki (Integral):</label>";
    html += "<input type='number' name='ki' value='" + String(ki, 2) + "' step='0.01' min='0'></div>";
    html += "<div class='control'><label>Kd (Derivative):</label>";
    html += "<input type='number' name='kd' value='" + String(kd, 2) + "' step='0.1' min='0'></div>";
    
    html += "<button type='submit'>Save All Settings</button></form>";
    
    html += "<h2>Firmware</h2>";
    html += "<div class='info-box'><strong>Current Version:</strong> " + String(firmwareVersion) + "</div>";
    html += "<div id='update-status'></div>";
    html += "<button type='button' class='btn-secondary' onclick='checkUpdates()' id='check-btn'>Check for Updates</button>";
    html += "<a href='/update'><button type='button' class='btn-secondary'>Manual Upload</button></a>";
    
    // JavaScript for update checking
    html += "<script>";
    html += "function checkUpdates(){";
    html += "document.getElementById('check-btn').disabled=true;";
    html += "document.getElementById('check-btn').innerText='Checking...';";
    html += "fetch('/api/check-update').then(r=>r.json()).then(d=>{";
    html += "let s=document.getElementById('update-status');";
    html += "if(d.update_available){";
    html += "s.innerHTML='<div class=\"warning-box\"><strong>⚡ Update Available!</strong><br>Latest: v'+d.latest_version+'<br><button class=\"btn-secondary\" onclick=\"autoUpdate()\">Install Update</button></div>';";
    html += "}else{";
    html += "s.innerHTML='<div class=\"info-box\">✓ You are running the latest version</div>';";
    html += "}";
    html += "document.getElementById('check-btn').disabled=false;";
    html += "document.getElementById('check-btn').innerText='Check for Updates';";
    html += "}).catch(()=>{";
    html += "document.getElementById('update-status').innerHTML='<div class=\"warning-box\">✗ Could not check for updates</div>';";
    html += "document.getElementById('check-btn').disabled=false;";
    html += "document.getElementById('check-btn').innerText='Check for Updates';";
    html += "});}";
    html += "function autoUpdate(){";
    html += "if(!confirm('Download and install update?'))return;";
    html += "document.getElementById('update-status').innerHTML='<div class=\"info-box\">⏳ Downloading...</div>';";
    html += "fetch('/api/auto-update',{method:'POST'}).then(r=>r.json()).then(d=>{";
    html += "if(d.success){";
    html += "document.getElementById('update-status').innerHTML='<div class=\"info-box\">✓ Update successful! Restarting...</div>';";
    html += "}else{";
    html += "document.getElementById('update-status').innerHTML='<div class=\"warning-box\">✗ Update failed</div>';";
    html += "}});}";
    html += "</script>";
    
    html += "<h2>System Actions</h2>";
    html += "<form action='/api/restart' method='POST'>";
    html += "<button type='submit' class='btn-danger' onclick='return confirm(\"Restart device?\")'>Restart Device</button></form>";
    
    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}

/**
 * Handle schedule page
 */
static void handleSchedule(void) {
    String html = webserver_get_html_header("Schedule", "schedule");
    
    bool enabled = scheduler_is_enabled();
    int slotCount = scheduler_get_slot_count();
    
    // Schedule enable/disable toggle
    html += "<div style='margin:20px 0;display:flex;justify-content:space-between;align-items:center'>";
    html += "<h2 style='margin:0'>Temperature Schedule</h2>";
    html += "<label style='display:flex;align-items:center;gap:10px'>";
    html += "<span>Schedule " + String(enabled ? "Enabled" : "Disabled") + "</span>";
    html += "<input type='checkbox' id='schedule-enabled' " + String(enabled ? "checked" : "") + " ";
    html += "onchange='toggleSchedule()' style='width:auto;height:20px'>";
    html += "</label></div>";
    
    // Show next scheduled change
    char nextTime[10];
    float nextTemp;
    if (enabled && scheduler_get_next(nextTime, &nextTemp)) {
        html += "<div class='info-box'>📅 Next: " + String(nextTemp, 1) + "°C at " + String(nextTime) + "</div>";
    }
    
    html += "<form id='schedule-form'>";
    
    // Schedule slots
    for (int i = 0; i < 8; i++) {
        if (i >= slotCount && i > 3) continue;
        
        ScheduleSlot_t slot;
        scheduler_get_slot(i, &slot);
        
        bool isActive = (i < slotCount) && slot.enabled;
        
        html += "<div class='schedule-slot' style='border:2px solid " + String(isActive ? "#4CAF50" : "#ddd") + ";";
        html += "padding:15px;border-radius:10px;margin:15px 0;background:" + String(isActive ? "#f1f8f4" : "#f9f9f9") + "'>";
        
        html += "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:10px'>";
        html += "<strong>Slot " + String(i + 1) + "</strong>";
        html += "<label style='display:flex;align-items:center;gap:5px'>";
        html += "<input type='checkbox' name='enabled" + String(i) + "' " + String(isActive ? "checked" : "") + " style='width:auto'>";
        html += "<span>Active</span></label></div>";
        
        // Time and temperature
        html += "<div style='display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:10px 0'>";
        html += "<div><label>Hour</label><input type='number' name='hour" + String(i) + "' value='" + String(slot.hour) + "' min='0' max='23'></div>";
        html += "<div><label>Minute</label><input type='number' name='minute" + String(i) + "' value='" + String(slot.minute) + "' min='0' max='59'></div>";
        html += "<div><label>Temp (°C)</label><input type='number' name='temp" + String(i) + "' value='" + String(slot.targetTemp, 1) + "' step='0.5' min='15' max='45'></div>";
        html += "</div>";
        
        // Days
        html += "<div><label>Active Days:</label>";
        html += "<div style='display:flex;flex-wrap:wrap;gap:5px;margin-top:5px'>";
        String dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        for (int d = 0; d < 7; d++) {
            char dayChar = "SMTWTFS"[d];
            bool checked = (strchr(slot.days, dayChar) != NULL);
            html += "<label style='min-width:40px;flex:1;max-width:60px;text-align:center;padding:8px 4px;background:" + String(checked ? "#4CAF50" : "#ddd") + ";";
            html += "color:" + String(checked ? "white" : "#666") + ";border-radius:5px;cursor:pointer;font-size:12px'>";
            html += "<input type='checkbox' name='day" + String(i) + "_" + String(d) + "' " + String(checked ? "checked" : "") + " ";
            html += "style='display:none' onchange='this.parentElement.style.background=this.checked?\"#4CAF50\":\"#ddd\";";
            html += "this.parentElement.style.color=this.checked?\"white\":\"#666\"'>";
            html += dayNames[d] + "</label>";
        }
        html += "</div></div>";
        html += "</div>"; // End slot
    }
    
    // Add/Remove buttons
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:20px 0'>";
    if (slotCount < 8) {
        html += "<button type='button' onclick='addSlot()' class='btn-secondary'>➕ Add Slot</button>";
    } else {
        html += "<div></div>";
    }
    if (slotCount > 1) {
        html += "<button type='button' onclick='removeSlot()' class='btn-danger'>➖ Remove Last</button>";
    }
    html += "</div>";
    
    html += "<button type='button' onclick='saveSchedule()'>Save Schedule</button>";
    html += "</form>";
    
    // JavaScript
    html += "<script>";
    html += "let slotCount=" + String(slotCount) + ";";
    html += "function toggleSchedule(){";
    html += "fetch('/api/save-schedule',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},";
    html += "body:'schedule_enabled='+document.getElementById('schedule-enabled').checked}).then(()=>location.reload());}";
    html += "function addSlot(){if(slotCount<8){slotCount++;location.reload();}}";
    html += "function removeSlot(){if(slotCount>1){slotCount--;location.reload();}}";
    html += "function saveSchedule(){";
    html += "let form=document.getElementById('schedule-form');";
    html += "let data='slot_count='+slotCount;";
    html += "for(let i=0;i<8;i++){";
    html += "data+='&enabled'+i+'='+(form['enabled'+i]?.checked||false);";
    html += "data+='&hour'+i+'='+(form['hour'+i]?.value||0);";
    html += "data+='&minute'+i+'='+(form['minute'+i]?.value||0);";
    html += "data+='&temp'+i+'='+(form['temp'+i]?.value||28);";
    html += "let days='';";
    html += "for(let d=0;d<7;d++){if(form['day'+i+'_'+d]?.checked)days+='SMTWTFS'[d];}";
    html += "data+='&days'+i+'='+days;}";
    html += "fetch('/api/save-schedule',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data})";
    html += ".then(()=>{alert('Schedule saved!');location.reload();});}";
    html += "</script>";
    
    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}

/**
 * Handle save settings
 */
static void handleSaveSettings(void) {
    // Save settings via Preferences
    Preferences prefs;
    prefs.begin("thermostat", false);
    
    if (server.hasArg("device_name")) {
        prefs.putString("device_name", server.arg("device_name"));
    }
    
    if (server.hasArg("wifi_ssid")) {
        prefs.putString("wifi_ssid", server.arg("wifi_ssid"));
    }
    
    if (server.hasArg("wifi_pass") && server.arg("wifi_pass").length() > 0) {
        prefs.putString("wifi_pass", server.arg("wifi_pass"));
    }
    
    // Save MQTT settings
    if (server.hasArg("mqtt_broker")) {
        prefs.putString("mqtt_broker", server.arg("mqtt_broker"));
    }
    if (server.hasArg("mqtt_port")) {
        prefs.putFloat("mqtt_port", server.arg("mqtt_port").toFloat());
    }
    if (server.hasArg("mqtt_user")) {
        prefs.putString("mqtt_user", server.arg("mqtt_user"));
    }
    if (server.hasArg("mqtt_pass") && server.arg("mqtt_pass").length() > 0) {
        prefs.putString("mqtt_pass", server.arg("mqtt_pass"));
    }
    
    // Save PID settings
    if (server.hasArg("kp")) {
        prefs.putFloat("Kp", server.arg("kp").toFloat());
    }
    if (server.hasArg("ki")) {
        prefs.putFloat("Ki", server.arg("ki").toFloat());
    }
    if (server.hasArg("kd")) {
        prefs.putFloat("Kd", server.arg("kd").toFloat());
    }
    
    prefs.end();
    
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='5;url=/'>";
    html += "<title>Settings Saved</title></head><body style='text-align:center;padding-top:50px'>";
    html += "<h1>Settings Saved!</h1><p>Device will restart in 5 seconds...</p></body></html>";
    
    server.send(200, "text/html", html);
    
    if (restartCallback != NULL) {
        delay(5000);
        restartCallback();
    }
}

/**
 * Handle save schedule
 */
static void handleSaveSchedule(void) {
    // Toggle schedule enable/disable
    if (server.hasArg("schedule_enabled")) {
        bool enabled = (server.arg("schedule_enabled") == "true");
        scheduler_set_enabled(enabled);
        scheduler_save();
        server.send(200, "text/plain", "OK");
        return;
    }
    
    // Save full schedule
    if (server.hasArg("slot_count")) {
        int slotCount = server.arg("slot_count").toInt();
        scheduler_set_slot_count(slotCount);
        
        for (int i = 0; i < 8; i++) {
            ScheduleSlot_t slot;
            slot.enabled = server.hasArg("enabled" + String(i)) && (server.arg("enabled" + String(i)) == "true");
            slot.hour = server.arg("hour" + String(i)).toInt();
            slot.minute = server.arg("minute" + String(i)).toInt();
            slot.targetTemp = server.arg("temp" + String(i)).toFloat();
            
            String days = server.arg("days" + String(i));
            if (days.length() == 0) days = "SMTWTFS";
            strncpy(slot.days, days.c_str(), sizeof(slot.days) - 1);
            slot.days[sizeof(slot.days) - 1] = '\0';
            
            scheduler_set_slot(i, &slot);
        }
        
        scheduler_save();
    }
    
    server.send(200, "text/plain", "OK");
}

/**
 * Handle restart
 */
static void handleRestart(void) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='10;url=/'>";
    html += "<title>Restarting</title></head><body style='text-align:center;padding-top:50px'>";
    html += "<h1>Restarting...</h1><p>Page will reload in 10 seconds.</p></body></html>";
    
    server.send(200, "text/html", html);
    
    if (restartCallback != NULL) {
        delay(1000);
        restartCallback();
    }
}

/**
 * Handle update page
 */
static void handleUpdate(void) {
    String html = webserver_get_html_header("Firmware Update", "settings");
    
    html += "<div class='info-box'><strong>Current Version:</strong> " + String(firmwareVersion) + "</div>";
    html += "<div class='warning-box'><strong>⚠️ Warning:</strong><br>";
    html += "• Do not power off during update<br>";
    html += "• Update takes 30-60 seconds</div>";
    
    html += "<h2>Upload Firmware</h2>";
    html += "<form method='POST' action='/api/upload' enctype='multipart/form-data'>";
    html += "<input type='file' name='firmware' accept='.bin' required style='margin:20px 0'>";
    html += "<button type='submit'>Upload Firmware</button></form>";
    
    html += "<a href='/settings'><button type='button' class='btn-secondary'>Back to Settings</button></a>";
    
    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}

/**
 * Handle firmware upload
 */
static void handleUpload(void) {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[WebServer] Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("[WebServer] Update Success: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

/**
 * Handle upload done
 */
static void handleUploadDone(void) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='15;url=/'>";
    html += "<title>Update Complete</title></head><body style='text-align:center;padding-top:50px'>";
    
    if (Update.hasError()) {
        html += "<h1 style='color:#f44336'>✗ Update Failed</h1>";
        html += "<p><a href='/update'>Try Again</a></p>";
    } else {
        html += "<h1>✓ Update Successful!</h1>";
        html += "<p>Device is restarting...</p>";
    }
    
    html += "</body></html>";
    server.send(200, "text/html", html);
    
    if (!Update.hasError() && restartCallback != NULL) {
        delay(1000);
        restartCallback();
    }
}

/**
 * Handle check for GitHub updates
 */
static void handleCheckUpdate(void) {
    if (networkAPMode || !networkConnected) {
        server.send(500, "application/json", "{\"error\":\"No internet connection\"}");
        return;
    }
    
    HTTPClient http;
    String url = "https://api.github.com/repos/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest";
    
    http.begin(url);
    http.addHeader("Accept", "application/vnd.github.v3+json");
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        
        // Simple JSON parsing for tag_name
        int tagIndex = payload.indexOf("\"tag_name\"");
        if (tagIndex > 0) {
            int startQuote = payload.indexOf("\"", tagIndex + 12);
            int endQuote = payload.indexOf("\"", startQuote + 1);
            String latestVersion = payload.substring(startQuote + 1, endQuote);
            
            // Remove 'v' prefix
            if (latestVersion.startsWith("v")) {
                latestVersion = latestVersion.substring(1);
            }
            
            bool updateAvailable = (latestVersion != String(firmwareVersion));
            
            String response = "{\"update_available\":" + String(updateAvailable ? "true" : "false");
            response += ",\"current_version\":\"" + String(firmwareVersion) + "\"";
            response += ",\"latest_version\":\"" + latestVersion + "\"}";
            
            server.send(200, "application/json", response);
            http.end();
            return;
        }
    }
    
    http.end();
    server.send(500, "application/json", "{\"error\":\"Could not check for updates\"}");
}

/**
 * Handle GitHub auto-update
 */
static void handleAutoUpdate(void) {
    if (networkAPMode || !networkConnected) {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"No internet\"}");
        return;
    }
    
    Serial.println("[WebServer] Starting GitHub auto-update");
    
    HTTPClient http;
    String url = "https://github.com/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest/download/" + String(GITHUB_FIRMWARE);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200 || httpCode == 302) {
        if (httpCode == 302) {
            String redirectUrl = http.getLocation();
            http.end();
            http.begin(redirectUrl);
            httpCode = http.GET();
        }
        
        if (httpCode == 200) {
            int contentLength = http.getSize();
            
            if (contentLength > 0 && Update.begin(contentLength)) {
                WiFiClient* stream = http.getStreamPtr();
                size_t written = Update.writeStream(*stream);
                
                if (written == contentLength && Update.end()) {
                    if (Update.isFinished()) {
                        Serial.println("[WebServer] Update successful");
                        server.send(200, "application/json", "{\"success\":true}");
                        http.end();
                        
                        if (restartCallback != NULL) {
                            delay(1000);
                            restartCallback();
                        }
                        return;
                    }
                }
            }
        }
    }
    
    http.end();
    Serial.println("[WebServer] Update failed");
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Download failed\"}");
}

// ===== HTML GENERATION HELPERS =====

/**
 * Get HTML header with navigation
 */
String webserver_get_html_header(const char* title, const char* activePage) {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>" + String(title) + " - " + String(deviceName) + "</title>";
    html += buildCSS();
    html += "</head><body><div class='container'>";
    html += "<div class='header'><h1>" + String(deviceName) + "</h1>";
    html += "<div class='subtitle'>ESP32 Reptile Thermostat v" + String(firmwareVersion) + "</div></div>";
    html += buildNavBar(activePage);
    
    return html;
}

/**
 * Get HTML footer
 */
String webserver_get_html_footer(unsigned long uptimeSeconds) {
    unsigned long days = uptimeSeconds / 86400;
    unsigned long hours = (uptimeSeconds % 86400) / 3600;
    unsigned long minutes = (uptimeSeconds % 3600) / 60;
    
    String html = "<div class='footer'>";
    html += "ESP32 Reptile Thermostat v" + String(firmwareVersion);
    html += " | Uptime: ";
    if (days > 0) html += String(days) + "d ";
    html += String(hours) + "h " + String(minutes) + "m";
    html += "</div></div></body></html>";
    
    return html;
}

/**
 * Build CSS stylesheet
 */
static String buildCSS(void) {
    String css = "<style>";
    css += "body{font-family:Arial;margin:0;padding:20px;background:#f0f0f0}";
    css += "@media(max-width:640px){body{padding:10px}}";
    css += ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
    css += ".header{background:linear-gradient(135deg,#4CAF50,#45a049);color:white;padding:20px;border-radius:10px 10px 0 0;margin:-20px -20px 20px -20px;text-align:center}";
    css += ".header h1{margin:0;font-size:24px}";
    css += ".header .subtitle{margin:5px 0 0 0;font-size:12px;opacity:0.9}";
    css += ".nav{display:flex;flex-wrap:wrap;justify-content:center;gap:10px;margin-bottom:20px}";
    css += ".nav a{flex:1;min-width:100px;padding:12px 20px;background:#2196F3;color:white;text-decoration:none;border-radius:5px;text-align:center;transition:background 0.3s}";
    css += ".nav a:hover{background:#0b7dda}";
    css += ".nav a.active{background:#4CAF50}";
    css += "h2{color:#666;border-bottom:2px solid #4CAF50;padding-bottom:5px;margin-top:30px}";
    css += ".status{display:flex;justify-content:space-between;margin:20px 0;padding:15px;border-radius:5px}";
    css += "@media(max-width:640px){.status{flex-direction:column;gap:10px}}";
    css += ".control{margin:20px 0}";
    css += "label{display:block;margin:10px 0 5px;font-weight:bold}";
    css += "input,select{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box}";
    css += "button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin-top:10px}";
    css += "button:hover{background:#45a049}";
    css += ".btn-secondary{background:#2196F3}";
    css += ".btn-secondary:hover{background:#0b7dda}";
    css += ".btn-danger{background:#f44336}";
    css += ".btn-danger:hover{background:#da190b}";
    css += ".info-box{background:#e3f2fd;padding:15px;border-radius:5px;margin:10px 0;border-left:4px solid #2196F3}";
    css += ".warning-box{background:#fff3cd;padding:15px;border-radius:5px;margin:10px 0;border-left:4px solid #ffc107}";
    css += ".stat-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin:20px 0}";
    css += ".stat-card{background:#f5f5f5;padding:15px;border-radius:5px;text-align:center}";
    css += ".stat-value{font-size:24px;font-weight:bold;color:#4CAF50}";
    css += ".stat-label{font-size:12px;color:#666;margin-top:5px}";
    css += ".log-entry{padding:10px;border-bottom:1px solid #eee;font-family:monospace;font-size:14px}";
    css += ".footer{text-align:center;margin-top:30px;padding-top:20px;border-top:1px solid #ddd;color:#666;font-size:12px}";
    css += "</style>";
    
    return css;
}

/**
 * Build navigation bar
 */
static String buildNavBar(const char* activePage) {
    String nav = "<div class='nav'>";
    nav += "<a href='/' class='" + String(strcmp(activePage, "home") == 0 ? "active" : "") + "'>🏠 Home</a>";
    nav += "<a href='/schedule' class='" + String(strcmp(activePage, "schedule") == 0 ? "active" : "") + "'>📅 Schedule</a>";
    nav += "<a href='/history' class='" + String(strcmp(activePage, "history") == 0 ? "active" : "") + "'>📈 History</a>";
    nav += "<a href='/info' class='" + String(strcmp(activePage, "info") == 0 ? "active" : "") + "'>ℹ️ Info</a>";
    nav += "<a href='/logs' class='" + String(strcmp(activePage, "logs") == 0 ? "active" : "") + "'>📋 Logs</a>";
    nav += "<a href='/settings' class='" + String(strcmp(activePage, "settings") == 0 ? "active" : "") + "'>⚙️ Settings</a>";
    nav += "</div>";
    
    return nav;
}
