/**
 * web_server.c
 * HTTP Web Server and API Endpoints Implementation
 */

#include "web_server.h"
#include "logger.h"
#include "temp_history.h"
#include "console.h"
#include "sensor_manager.h"
#include "output_manager.h"
#include "safety_manager.h"
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

// Schedule data (legacy - now handled per-output in output_manager)
// Keeping variables for backward compatibility with old API endpoints

// GitHub auto-update configuration
static const char* GITHUB_USER = "cheew";
static const char* GITHUB_REPO = "Claude-ESP32-Thermostat";
static const char* GITHUB_FIRMWARE = "firmware.bin";

// Security settings
static bool secureMode = false;
static char securePin[8] = "";
static char sessionToken[33] = "";

// UI Mode (false = simple, true = advanced)
static bool advancedMode = false;

// Forward declarations - Route handlers
static void handleLogin(void);
static void handleLoginAPI(void);
static void handleLogout(void);
static void handleUIMode(void);
static void handleRoot(void);
static void handleSchedule(void);
static void handleHistoryPage(void);
static void handleInfo(void);
static void handleLogs(void);
static void handleConsole(void);
static void handleSettings(void);
static void handleStatus(void);
static void handleInfo_API(void);
static void handleLogs_API(void);
static void handleHistory(void);
static void handleConsoleEvents(void);
static void handleSet(void);
static void handleControl(void);
static void handleSaveSettings(void);
static void handleRestart(void);
static void handleUpdate(void);
static void handleUpload(void);
static void handleUploadDone(void);
static void handleCheckUpdate(void);
static void handleAutoUpdate(void);

// Multi-output API handlers
static void handleOutputsAPI(void);
static void handleOutputAPI(void);
static void handleOutputControl(void);
static void handleOutputConfig(void);
static void handleOutputClearFault(void);
static void handleSensorsAPI(void);
static void handleSensorName(void);

// v1 API handlers
static void handleHealthAPI(void);

// Safety page and API handlers
static void handleSafetyPage(void);
static void handleSafetyAPI(void);
static void handleEmergencyStop(void);
static void handleExitSafeMode(void);

// HTML generation helpers
static String buildCSS(void);
static String buildNavBar(const char* activePage);

// Authentication helpers
static void generateSessionToken(void);
static bool isAuthenticated(void);
static void requireAuth(void);

/**
 * Generate a random session token
 */
static void generateSessionToken(void) {
    const char* hex = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        sessionToken[i] = hex[random(16)];
    }
    sessionToken[32] = '\0';
}

/**
 * Check if current request is authenticated
 * Returns true if secure mode is off or valid session cookie present
 */
static bool isAuthenticated(void) {
    // If secure mode disabled, always authenticated
    if (!secureMode) return true;

    // If no PIN set, always authenticated
    if (strlen(securePin) == 0) return true;

    // Check for valid session cookie
    if (server.hasHeader("Cookie")) {
        String cookie = server.header("Cookie");
        String expectedCookie = "session=" + String(sessionToken);
        if (cookie.indexOf(expectedCookie) >= 0 && strlen(sessionToken) > 0) {
            return true;
        }
    }
    return false;
}

/**
 * Redirect to login page if not authenticated
 */
static void requireAuth(void) {
    String redirect = server.uri();
    server.sendHeader("Location", "/login?redirect=" + redirect);
    server.send(302);
}

/**
 * Handle login page (GET shows form, POST validates)
 */
static void handleLogin(void) {
    String error = "";
    String redirect = server.arg("redirect");
    if (redirect.length() == 0) redirect = "/";

    // Handle POST (form submission)
    if (server.method() == HTTP_POST) {
        String pin = server.arg("pin");
        if (pin == String(securePin)) {
            // Successful login - generate new session and set cookie
            generateSessionToken();
            server.sendHeader("Set-Cookie", "session=" + String(sessionToken) + "; Path=/; HttpOnly");
            server.sendHeader("Location", redirect);
            server.send(302);
            Serial.println("[WebServer] Login successful");
            return;
        } else {
            error = "Invalid PIN";
            Serial.println("[WebServer] Login failed - invalid PIN");
        }
    }

    // Show login form
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Login - " + String(deviceName) + "</title>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;background:#f5f5f5;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}";
    html += ".login-box{background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;max-width:300px;width:90%}";
    html += "h1{color:#333;margin-bottom:30px;font-size:24px}";
    html += "input[type=password]{width:100%;padding:15px;font-size:24px;text-align:center;border:2px solid #ddd;border-radius:5px;margin-bottom:20px;letter-spacing:8px;box-sizing:border-box}";
    html += "input[type=password]:focus{border-color:#2196F3;outline:none}";
    html += "button{width:100%;padding:15px;font-size:18px;background:#2196F3;color:white;border:none;border-radius:5px;cursor:pointer}";
    html += "button:hover{background:#1976D2}";
    html += ".error{color:#f44336;margin-bottom:20px;padding:10px;background:#ffebee;border-radius:5px}";
    html += ".device-name{color:#666;font-size:14px;margin-bottom:10px}";
    html += "</style></head><body>";
    html += "<div class='login-box'>";
    html += "<div class='device-name'>" + String(deviceName) + "</div>";
    html += "<h1>Enter PIN</h1>";
    if (error.length() > 0) {
        html += "<div class='error'>" + error + "</div>";
    }
    html += "<form method='POST'>";
    html += "<input type='hidden' name='redirect' value='" + redirect + "'>";
    html += "<input type='password' name='pin' maxlength='6' pattern='[0-9]*' inputmode='numeric' placeholder='****' autofocus required>";
    html += "<button type='submit'>Login</button>";
    html += "</form>";
    html += "</div></body></html>";

    server.send(200, "text/html", html);
}

/**
 * Handle API login (JSON POST)
 */
static void handleLoginAPI(void) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }

    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    const char* pin = doc["pin"];
    if (pin && strcmp(pin, securePin) == 0) {
        generateSessionToken();
        server.sendHeader("Set-Cookie", "session=" + String(sessionToken) + "; Path=/; HttpOnly");
        server.send(200, "application/json", "{\"success\":true}");
        Serial.println("[WebServer] API login successful");
    } else {
        server.send(401, "application/json", "{\"success\":false,\"error\":\"Invalid PIN\"}");
        Serial.println("[WebServer] API login failed");
    }
}

/**
 * Handle logout
 */
static void handleLogout(void) {
    // Clear session by setting expired cookie
    server.sendHeader("Set-Cookie", "session=; Path=/; HttpOnly; Max-Age=0");
    server.sendHeader("Location", "/");
    server.send(302);
    Serial.println("[WebServer] Logout");
}

/**
 * Handle UI mode switch (Simple/Advanced)
 */
static void handleUIMode(void) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }

    StaticJsonDocument<64> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    const char* mode = doc["mode"];
    if (mode) {
        bool newAdvancedMode = (strcmp(mode, "advanced") == 0);
        advancedMode = newAdvancedMode;

        // Save to preferences
        Preferences prefs;
        prefs.begin("thermostat", false);
        prefs.putBool("ui_advanced", advancedMode);
        prefs.end();

        Serial.printf("[WebServer] UI mode changed to: %s\n", advancedMode ? "Advanced" : "Simple");
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing mode\"}");
    }
}

/**
 * Initialize web server
 */
void webserver_init(void) {
    Serial.println("[WebServer] Initializing web server");

    // Load security settings from preferences
    Preferences prefs;
    prefs.begin("thermostat", true);
    secureMode = prefs.getBool("secure_mode", false);
    String pin = prefs.getString("secure_pin", "");
    strncpy(securePin, pin.c_str(), sizeof(securePin) - 1);
    securePin[sizeof(securePin) - 1] = '\0';
    advancedMode = prefs.getBool("ui_advanced", false);
    prefs.end();

    // Generate initial session token
    generateSessionToken();

    Serial.printf("[WebServer] Secure mode: %s, UI mode: %s\n",
                  secureMode ? "ON" : "OFF",
                  advancedMode ? "Advanced" : "Simple");

    // Collect cookies for authentication
    const char* headerKeys[] = {"Cookie"};
    server.collectHeaders(headerKeys, 1);

    // Setup routes
    server.on("/", handleRoot);
    server.on("/login", HTTP_GET, handleLogin);
    server.on("/login", HTTP_POST, handleLogin);
    server.on("/api/login", HTTP_POST, handleLoginAPI);
    server.on("/logout", handleLogout);
    server.on("/api/ui-mode", HTTP_POST, handleUIMode);
    server.on("/outputs", []() {
        // Protected route
        if (!isAuthenticated()) { requireAuth(); return; }
        String html = webserver_get_html_header("Outputs", "outputs");

        html += "<h2>Output Configuration</h2>";
        html += "<p>Configure each output's settings, sensor assignment, and PID parameters.</p>";

        // Output selector tabs
        html += "<div style='display:flex;gap:10px;margin:20px 0'>";
        for (int i = 1; i <= 3; i++) {
            html += "<button onclick='showOutput(" + String(i) + ")' id='tab" + String(i) + "' style='padding:10px 20px;cursor:pointer'>";
            html += "Output " + String(i) + "</button>";
        }
        html += "</div>";

        html += "<script>";
        html += "let currentOutput=1;";
        html += "function showOutput(id){currentOutput=id;";
        html += "for(let i=1;i<=3;i++){document.getElementById('tab'+i).style.background=i===id?'#2196F3':'#f0f0f0';";
        html += "document.getElementById('tab'+i).style.color=i===id?'white':'black';}";
        html += "loadOutput(id);}";
        html += "function loadOutput(id){fetch('/api/output/'+id).then(r=>r.json()).then(d=>{";
        html += "document.getElementById('out-name').value=d.name;";
        html += "document.getElementById('out-enabled').checked=d.enabled;";
        html += "document.getElementById('out-sensor').value=d.sensor;";
        html += "handleSensorChange(d.sensor);";
        html += "document.getElementById('out-target').value=d.target;";
        html += "document.getElementById('out-target-slider').value=d.target;";
        html += "document.getElementById('temp-display').innerText=parseFloat(d.target).toFixed(1);";
        html += "document.getElementById('out-mode').value=d.mode.toLowerCase();";
        html += "document.getElementById('out-power').value=d.manualPower;";
        html += "document.getElementById('out-kp').value=d.pid.kp;";
        html += "document.getElementById('out-ki').value=d.pid.ki;";
        html += "document.getElementById('out-kd').value=d.pid.kd;";
        html += "document.getElementById('out-tp-cycle').value=d.timeProp.cycleSec;";
        html += "document.getElementById('out-tp-min-on').value=d.timeProp.minOnSec;";
        html += "document.getElementById('out-tp-min-off').value=d.timeProp.minOffSec;";
        html += "document.getElementById('device-info').innerHTML='<strong>Device:</strong> '+d.deviceType+' | <strong>Hardware:</strong> '+d.hardwareType;";
        html += "});}";
        html += "function saveConfig(){let data={";
        html += "name:document.getElementById('out-name').value,";
        html += "enabled:document.getElementById('out-enabled').checked,";
        html += "sensor:document.getElementById('out-sensor').value,";
        html += "pid:{kp:parseFloat(document.getElementById('out-kp').value),";
        html += "ki:parseFloat(document.getElementById('out-ki').value),";
        html += "kd:parseFloat(document.getElementById('out-kd').value)},";
        html += "timeProp:{cycleSec:parseInt(document.getElementById('out-tp-cycle').value),";
        html += "minOnSec:parseInt(document.getElementById('out-tp-min-on').value),";
        html += "minOffSec:parseInt(document.getElementById('out-tp-min-off').value)}};";
        html += "fetch('/api/output/'+currentOutput+'/config',{method:'POST',";
        html += "headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})";
        html += ".then(()=>alert('Saved!'));}";
        html += "function saveControl(){let data={";
        html += "target:parseFloat(document.getElementById('out-target').value),";
        html += "mode:document.getElementById('out-mode').value,";
        html += "power:parseInt(document.getElementById('out-power').value)};";
        html += "fetch('/api/output/'+currentOutput+'/control',{method:'POST',";
        html += "headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})";
        html += ".then(()=>alert('Control updated!'));}";

        // Slider functions
        html += "function updateTempDisplay(val){";
        html += "document.getElementById('temp-display').innerText=parseFloat(val).toFixed(1);";
        html += "document.getElementById('out-target').value=val;}";

        html += "function toggleSafeZone(override){";
        html += "let slider=document.getElementById('out-target-slider');";
        html += "let input=document.getElementById('out-target');";
        html += "if(override){";
        html += "slider.max=45;input.max=45;";
        html += "console.log('⚠️ Safe zone overridden - extended range to 45°C');}else{";
        html += "slider.max=35;input.max=35;";
        html += "if(parseFloat(slider.value)>35){slider.value=35;updateTempDisplay(35);}";
        html += "}}";

        // Sensor type change handler
        html += "function handleSensorChange(val){";
        html += "let tempControl=document.getElementById('temp-control-section');";
        html += "let infoBox=document.getElementById('sensor-type-info');";
        html += "let modeSelect=document.getElementById('out-mode');";
        html += "if(val==='none'){";
        html += "tempControl.style.display='none';";
        html += "infoBox.style.display='block';";
        html += "infoBox.innerHTML='ℹ️ <strong>No Sensor Mode:</strong> Only Schedule and Manual modes available. Use for lights, foggers, or misters.';";
        html += "modeSelect.querySelector('option[value=\"pid\"]').disabled=true;";
        html += "modeSelect.querySelector('option[value=\"onoff\"]').disabled=true;";
        html += "if(modeSelect.value==='pid'||modeSelect.value==='onoff'){modeSelect.value='manual';}";
        html += "}else if(val==='humidity'){";
        html += "tempControl.style.display='block';";
        html += "document.querySelector('label[for=\"temp-display\"]').innerHTML='<strong>Target Humidity: <span id=\"temp-display\">50.0</span>%</strong>';";
        html += "infoBox.style.display='block';";
        html += "infoBox.innerHTML='⚠️ <strong>Humidity Mode:</strong> Coming soon! Humidity sensors not yet supported.';";
        html += "modeSelect.querySelector('option[value=\"pid\"]').disabled=false;";
        html += "modeSelect.querySelector('option[value=\"onoff\"]').disabled=false;";
        html += "}else{";
        html += "tempControl.style.display='block';";
        html += "infoBox.style.display='none';";
        html += "document.querySelector('label[for=\"temp-display\"]').innerHTML='<strong>Target Temperature: <span id=\"temp-display\">28.0</span>°C</strong>';";
        html += "modeSelect.querySelector('option[value=\"pid\"]').disabled=false;";
        html += "modeSelect.querySelector('option[value=\"onoff\"]').disabled=false;";
        html += "}}";

        html += "showOutput(1)";
        html += "</script>";

        // Configuration form
        html += "<div style='background:#f9f9f9;padding:20px;border-radius:8px;margin:20px 0'>";

        html += "<h3>Basic Settings</h3>";
        html += "<div style='margin:10px 0'><label><input type='checkbox' id='out-enabled'> Enabled</label></div>";
        html += "<div style='margin:10px 0'><label>Name: <input type='text' id='out-name' style='width:300px'></label></div>";

        html += "<div style='margin:10px 0'><label>Sensor: <select id='out-sensor' style='width:300px' onchange='handleSensorChange(this.value)'>";
        html += "<option value='none'>No Sensor (Time/Manual Only)</option>";
        html += "<option value='humidity'>Humidity Sensor (Future)</option>";
        int sensorCount = sensor_manager_get_count();
        for (int i = 0; i < sensorCount; i++) {
            const SensorInfo_t* sensor = sensor_manager_get_sensor(i);
            if (sensor) {
                html += "<option value='" + String(sensor->addressString) + "'>" + String(sensor->name) + "</option>";
            }
        }
        html += "</select></label></div>";
        html += "<div id='sensor-type-info' style='margin:10px 0;padding:10px;background:#fff3cd;border-radius:5px;display:none'></div>";

        html += "<div id='device-info' style='margin:10px 0;padding:10px;background:#e3f2fd;border-radius:5px'></div>";

        html += "<button onclick='saveConfig()' style='margin:10px 5px 10px 0;padding:10px 20px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer'>Save Configuration</button>";

        html += "<h3>Control Settings</h3>";

        // Target temperature with slider and safe zone override (can be hidden for "No Sensor" mode)
        html += "<div id='temp-control-section'>";
        html += "<div style='margin:10px 0'>";
        html += "<label style='display:block;margin-bottom:5px' for='temp-display'><strong>Target Temperature: <span id='temp-display'>28.0</span>°C</strong></label>";
        html += "<input type='range' id='out-target-slider' min='15' max='35' step='0.5' value='28' ";
        html += "style='width:100%;max-width:400px' ";
        html += "oninput='updateTempDisplay(this.value)'>";
        html += "<div style='display:flex;justify-content:space-between;max-width:400px;font-size:12px;color:#666'>";
        html += "<span>15°C</span><span style='color:#4CAF50;font-weight:bold'>Safe Zone (15-35°C)</span><span>35°C</span></div>";
        html += "<label style='margin-top:10px;display:flex;align-items:center;gap:8px;color:#ff9800;font-weight:bold'>";
        html += "<input type='checkbox' id='override-safe' onchange='toggleSafeZone(this.checked)' style='width:auto'>";
        html += "<span>⚠️ Override Safe Zone (15-45°C)</span></label>";
        html += "<input type='number' id='out-target' step='0.5' min='15' max='35' value='28' style='display:none'>";
        html += "</div>";
        html += "</div>";

        html += "<div style='margin:10px 0'><label>Mode: <select id='out-mode' style='width:200px'>";
        html += "<option value='off'>Off</option>";
        html += "<option value='manual'>Manual</option>";
        html += "<option value='pid'>PID (Auto)</option>";
        html += "<option value='onoff'>On/Off Thermostat</option>";
        html += "<option value='timeprop'>Time-Proportional</option>";
        html += "<option value='schedule'>Schedule</option>";
        html += "</select></label></div>";
        html += "<div style='margin:10px 0'><label>Manual Power (%): <input type='number' id='out-power' min='0' max='100' style='width:100px'></label></div>";

        html += "<button onclick='saveControl()' style='margin:10px 5px 10px 0;padding:10px 20px;background:#2196F3;color:white;border:none;border-radius:5px;cursor:pointer'>Apply Control</button>";

        // PID Tuning - collapsible section
        html += "<h3 style='margin-top:20px'>Advanced Settings</h3>";
        html += "<button onclick='document.getElementById(\"pid-tuning\").style.display=document.getElementById(\"pid-tuning\").style.display===\"none\"?\"block\":\"none\";this.innerText=this.innerText.includes(\"Show\")?\"▼ Hide PID Tuning\":\"▶ Show PID Tuning\"' style='margin:10px 0;padding:10px 15px;background:#2196F3;color:white;border:none;border-radius:5px;cursor:pointer'>▶ Show PID Tuning</button>";
        html += "<div id='pid-tuning' style='display:none;margin-top:10px;padding:15px;background:#f0f0f0;border-radius:5px'>";
        html += "<div style='margin:10px 0'><label>Kp (Proportional): <input type='number' id='out-kp' step='0.1' style='width:100px'></label></div>";
        html += "<div style='margin:10px 0'><label>Ki (Integral): <input type='number' id='out-ki' step='0.01' style='width:100px'></label></div>";
        html += "<div style='margin:10px 0'><label>Kd (Derivative): <input type='number' id='out-kd' step='0.1' style='width:100px'></label></div>";
        html += "<p style='color:#666;font-size:14px'>PID tuning affects PID and Time-Proportional modes. Start with Kp=10, Ki=0.5, Kd=2.</p>";
        html += "</div>";

        // Time-Proportional Settings
        html += "<button onclick='document.getElementById(\"timeprop-settings\").style.display=document.getElementById(\"timeprop-settings\").style.display===\"none\"?\"block\":\"none\";this.innerText=this.innerText.includes(\"Show\")?\"Hide Time-Prop Settings\":\"Show Time-Prop Settings\"' style='margin:10px 0;padding:10px 15px;background:#ff9800;color:white;border:none;border-radius:5px;cursor:pointer'>Show Time-Prop Settings</button>";
        html += "<div id='timeprop-settings' style='display:none;margin-top:10px;padding:15px;background:#fff3e0;border-radius:5px'>";
        html += "<div style='margin:10px 0'><label>Cycle Time (sec): <input type='number' id='out-tp-cycle' min='5' max='120' value='30' style='width:100px'></label></div>";
        html += "<div style='margin:10px 0'><label>Min ON Time (sec): <input type='number' id='out-tp-min-on' min='1' max='30' value='1' style='width:100px'></label></div>";
        html += "<div style='margin:10px 0'><label>Min OFF Time (sec): <input type='number' id='out-tp-min-off' min='1' max='30' value='1' style='width:100px'></label></div>";
        html += "<p style='color:#666;font-size:14px'>Time-proportional converts PID output into ON/OFF cycles. 60% duty with 30s cycle = 18s ON, 12s OFF. Longer cycles (30-60s) reduce relay wear.</p>";
        html += "</div>";

        html += "</div>";

        html += webserver_get_html_footer(millis() / 1000);
        server.send(200, "text/html", html);
    });
    server.on("/sensors", []() {
        String html = webserver_get_html_header("Sensors", "sensors");

        html += "<h2>Temperature Sensors</h2>";
        html += "<p>Manage your DS18B20 temperature sensors. Rename sensors for easier identification.</p>";

        // Auto-refresh script
        html += "<script>function updateSensors(){fetch('/api/sensors').then(r=>r.json()).then(d=>{";
        html += "let tbody=document.getElementById('sensor-tbody');tbody.innerHTML='';";
        html += "d.sensors.forEach(s=>{let row=tbody.insertRow();";
        html += "row.innerHTML=`<td>${s.name}</td><td>${s.temp}°C</td><td><small>${s.address}</small></td>";
        html += "<td><button onclick='renameSensor(\"${s.address}\",\"${s.name}\")'>Rename</button></td>`;";
        html += "});});}updateSensors();setInterval(updateSensors,3000);";
        html += "function renameSensor(addr,oldName){let name=prompt('Rename sensor:',oldName);if(name&&name!==oldName){";
        html += "fetch('/api/sensor/name',{method:'POST',headers:{'Content-Type':'application/json'},";
        html += "body:JSON.stringify({address:addr,name:name})}).then(()=>updateSensors());}}";
        html += "</script>";

        // Sensors table
        html += "<table style='width:100%;border-collapse:collapse;margin:20px 0'>";
        html += "<thead><tr style='background:#f0f0f0'><th style='padding:10px;text-align:left'>Name</th>";
        html += "<th style='padding:10px;text-align:left'>Temperature</th>";
        html += "<th style='padding:10px;text-align:left'>Address</th>";
        html += "<th style='padding:10px;text-align:left'>Actions</th></tr></thead>";
        html += "<tbody id='sensor-tbody'>";

        // Generate initial rows
        int sensorCount = sensor_manager_get_count();
        if (sensorCount == 0) {
            html += "<tr><td colspan='4' style='padding:20px;text-align:center;color:#999'>No sensors found. Check wiring and restart.</td></tr>";
        } else {
            for (int i = 0; i < sensorCount; i++) {
                const SensorInfo_t* sensor = sensor_manager_get_sensor(i);
                if (!sensor) continue;

                html += "<tr style='border-bottom:1px solid #ddd'>";
                html += "<td style='padding:10px'>" + String(sensor->name) + "</td>";
                html += "<td style='padding:10px'>" + String(sensor->lastReading, 1) + "°C</td>";
                html += "<td style='padding:10px'><small>" + String(sensor->addressString) + "</small></td>";
                html += "<td style='padding:10px'><button onclick='renameSensor(\"" + String(sensor->addressString) + "\",\"" + String(sensor->name) + "\")'>Rename</button></td>";
                html += "</tr>";
            }
        }

        html += "</tbody></table>";

        html += "<div style='margin:20px 0;padding:15px;background:#e3f2fd;border-radius:8px'>";
        html += "<strong>ℹ️ Sensor Information:</strong><br>";
        html += "• Sensors are auto-discovered on boot<br>";
        html += "• Each sensor has a unique 64-bit ROM address<br>";
        html += "• Assign sensors to outputs in <a href='/outputs'>Outputs Configuration</a><br>";
        html += "• Temperature updates every 2 seconds<br>";
        html += "• To add new sensors: power off, connect sensor, power on";
        html += "</div>";

        html += webserver_get_html_footer(millis() / 1000);
        server.send(200, "text/html", html);
    });
    server.on("/schedule", handleSchedule);
    server.on("/history", handleHistoryPage);
    server.on("/info", handleInfo);
    server.on("/logs", handleLogs);
    server.on("/console", handleConsole);
    server.on("/settings", handleSettings);
    server.on("/safety", handleSafetyPage);

    // API endpoints
    server.on("/api/status", handleStatus);
    server.on("/api/info", handleInfo_API);
    server.on("/api/logs", handleLogs_API);
    server.on("/api/history", handleHistory);
    server.on("/api/console", handleConsoleEvents);
    server.on("/api/set", HTTP_POST, handleSet);
    server.on("/api/control", HTTP_POST, handleControl);
    server.on("/api/save-settings", HTTP_POST, handleSaveSettings);
    // Note: /api/save-schedule removed - use /api/output/{id}/config for per-output schedules
    server.on("/api/restart", HTTP_POST, handleRestart);
    server.on("/api/check-update", handleCheckUpdate);
    server.on("/api/auto-update", HTTP_POST, handleAutoUpdate);
    
    // Console routes
    server.on("/api/console-clear", HTTP_POST, []() {
        console_clear();
        server.send(200, "text/plain", "OK");
    });

    // Multi-output API routes
    server.on("/api/outputs", HTTP_GET, handleOutputsAPI);
    server.on("/api/output/1", HTTP_GET, handleOutputAPI);
    server.on("/api/output/2", HTTP_GET, handleOutputAPI);
    server.on("/api/output/3", HTTP_GET, handleOutputAPI);
    server.on("/api/output/1/control", HTTP_POST, handleOutputControl);
    server.on("/api/output/2/control", HTTP_POST, handleOutputControl);
    server.on("/api/output/3/control", HTTP_POST, handleOutputControl);
    server.on("/api/output/1/config", HTTP_POST, handleOutputConfig);
    server.on("/api/output/2/config", HTTP_POST, handleOutputConfig);
    server.on("/api/output/3/config", HTTP_POST, handleOutputConfig);
    server.on("/api/output/1/clear-fault", HTTP_POST, handleOutputClearFault);
    server.on("/api/output/2/clear-fault", HTTP_POST, handleOutputClearFault);
    server.on("/api/output/3/clear-fault", HTTP_POST, handleOutputClearFault);
    server.on("/api/output/1/safety", HTTP_POST, handleSafetyAPI);
    server.on("/api/output/2/safety", HTTP_POST, handleSafetyAPI);
    server.on("/api/output/3/safety", HTTP_POST, handleSafetyAPI);

    // Safety API routes
    server.on("/api/safety/state", HTTP_GET, []() {
        const SafetyState_t* state = safety_manager_get_state();
        StaticJsonDocument<256> doc;
        doc["safeMode"] = state->safeMode;
        doc["safeModeReason"] = safety_manager_get_reason_name(state->safeModeReason);
        doc["bootCount"] = state->bootCount;
        doc["watchdogEnabled"] = state->watchdogEnabled;
        doc["watchdogMarginMs"] = safety_manager_get_watchdog_margin();
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });
    server.on("/api/safety/emergency-stop", HTTP_POST, handleEmergencyStop);
    server.on("/api/safety/exit-safe-mode", HTTP_POST, handleExitSafeMode);

    // v1 API routes (versioned endpoints)
    server.on("/api/v1/health", HTTP_GET, handleHealthAPI);
    server.on("/api/v1/outputs", HTTP_GET, handleOutputsAPI);
    server.on("/api/v1/output/1", HTTP_GET, handleOutputAPI);
    server.on("/api/v1/output/2", HTTP_GET, handleOutputAPI);
    server.on("/api/v1/output/3", HTTP_GET, handleOutputAPI);

    // Sensor API routes
    server.on("/api/sensors", HTTP_GET, handleSensorsAPI);
    server.on("/api/sensor/name", HTTP_POST, handleSensorName);

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

// webserver_set_schedule_data() removed - legacy function no longer needed
// Schedule data now managed per-output via output_manager

// ===== ROUTE HANDLERS =====

/**
 * Handle root page (/) - Simple or Advanced mode
 */
static void handleRoot(void) {
    String html = webserver_get_html_header("Home", "home");

    if (networkAPMode) {
        html += "<div class='warning-box'><strong>AP Mode Active</strong><br>";
        html += "Configure WiFi in <a href='/settings'>Settings</a></div>";
    }

    if (!advancedMode) {
        // ========== SIMPLE MODE DASHBOARD ==========
        // Clean, card-based layout with sliders and dropdowns

        // CSS for Simple mode cards
        html += "<style>";
        html += ".simple-card{background:#fff;border-radius:12px;padding:20px;box-shadow:0 2px 8px rgba(0,0,0,0.1);margin-bottom:15px}";
        html += ".simple-card.heating{background:linear-gradient(135deg,#ffebee,#fff);border-left:4px solid #f44336}";
        html += ".simple-card.disabled{opacity:0.5}";
        html += ".simple-card h3{margin:0 0 15px 0;font-size:18px;color:#333;display:flex;justify-content:space-between;align-items:center}";
        html += ".temp-display{font-size:48px;font-weight:bold;color:#333;text-align:center;margin:10px 0}";
        html += ".temp-display small{font-size:20px;color:#666;font-weight:normal}";
        html += ".target-row{display:flex;align-items:center;gap:10px;margin:15px 0}";
        html += ".target-row label{min-width:60px;color:#666}";
        html += ".target-row input[type=range]{flex:1;height:8px}";
        html += ".target-row .target-val{min-width:60px;text-align:right;font-weight:bold;font-size:18px}";
        html += ".mode-row{display:flex;align-items:center;gap:10px;margin:15px 0}";
        html += ".mode-row label{min-width:60px;color:#666}";
        html += ".mode-row select{flex:1;padding:10px;font-size:16px;border-radius:5px;border:1px solid #ddd}";
        html += ".power-row{display:none;align-items:center;gap:10px;margin:15px 0}";
        html += ".power-row.show{display:flex}";
        html += ".power-row label{min-width:60px;color:#666}";
        html += ".power-row input[type=range]{flex:1}";
        html += ".power-row .power-val{min-width:50px;text-align:right;font-weight:bold}";
        html += ".status-indicator{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:8px}";
        html += ".status-indicator.on{background:#4CAF50;box-shadow:0 0 8px #4CAF50}";
        html += ".status-indicator.off{background:#ccc}";
        html += ".fault-chip{display:inline-block;padding:4px 10px;border-radius:12px;font-size:12px;font-weight:bold;margin-left:8px}";
        html += ".fault-chip.fault{background:#f44336;color:white}";
        html += ".fault-chip.stale{background:#ff9800;color:white}";
        html += ".fault-chip.ok{display:none}";
        html += ".simple-card.fault{border-left:4px solid #f44336;background:linear-gradient(135deg,#ffebee,#fff)}";
        html += ".clear-fault-btn{background:#f44336;color:white;border:none;padding:8px 16px;border-radius:5px;cursor:pointer;font-size:12px;margin-top:10px}";
        html += ".clear-fault-btn:hover{background:#d32f2f}";
        html += "</style>";

        // JavaScript for Simple mode
        html += "<script>";
        // Update function
        html += "function updateSimple(){fetch('/api/outputs').then(r=>r.json()).then(d=>{";
        html += "d.outputs.forEach((o,i)=>{let id=i+1;";
        html += "let card=document.getElementById('card'+id);";
        html += "if(!card)return;";
        html += "document.getElementById('currTemp'+id).innerText=o.enabled?(o.temp!==null?o.temp.toFixed(1):'--.-'):'--.-';";
        html += "card.className='simple-card'+(o.inFault?' fault':'')+(o.heating?' heating':'')+(o.enabled?'':' disabled');";
        html += "document.getElementById('status'+id).className='status-indicator '+(o.heating?'on':'off');";
        // Fault chip update
        html += "let faultChip=document.getElementById('faultChip'+id);";
        html += "let clearBtn=document.getElementById('clearFault'+id);";
        html += "if(o.inFault){faultChip.className='fault-chip fault';faultChip.innerText=o.faultState;clearBtn.style.display='block';}";
        html += "else if(o.sensorHealth!=='OK'){faultChip.className='fault-chip stale';faultChip.innerText=o.sensorHealth;clearBtn.style.display='none';}";
        html += "else{faultChip.className='fault-chip ok';clearBtn.style.display='none';}";
        html += "});}).catch(e=>console.error(e));}";

        // Control functions
        html += "function setTarget(id,val){";
        html += "document.getElementById('targetVal'+id).innerText=parseFloat(val).toFixed(1)+'°C';";
        html += "fetch('/api/output/'+id+'/control',{method:'POST',headers:{'Content-Type':'application/json'},";
        html += "body:JSON.stringify({target:parseFloat(val)})}).then(()=>updateSimple());}";

        html += "function setMode(id,mode){";
        html += "let powerRow=document.getElementById('powerRow'+id);";
        html += "powerRow.className='power-row'+(mode==='manual'?' show':'');";
        html += "let data={mode:mode};";
        html += "if(mode==='manual')data.power=parseInt(document.getElementById('powerSlider'+id).value);";
        html += "fetch('/api/output/'+id+'/control',{method:'POST',headers:{'Content-Type':'application/json'},";
        html += "body:JSON.stringify(data)}).then(()=>updateSimple());}";

        html += "function setPower(id,val){";
        html += "document.getElementById('powerVal'+id).innerText=val+'%';";
        html += "fetch('/api/output/'+id+'/control',{method:'POST',headers:{'Content-Type':'application/json'},";
        html += "body:JSON.stringify({power:parseInt(val)})}).then(()=>updateSimple());}";

        // Clear fault function
        html += "function clearFault(id){";
        html += "fetch('/api/output/'+id+'/clear-fault',{method:'POST'}).then(r=>r.json()).then(d=>{";
        html += "if(d.ok){updateSimple();}else{alert('Cannot clear fault: '+d.error.message);}";
        html += "}).catch(e=>alert('Error: '+e));}";

        // Start polling
        html += "updateSimple();setInterval(updateSimple,3000);";
        html += "</script>";

        // Output cards
        html += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin:20px 0'>";

        for (int i = 0; i < 3; i++) {
            OutputConfig_t* output = output_manager_get_output(i);
            if (!output) continue;

            int id = i + 1;
            String cardClass = "simple-card";
            if (output->faultState != FAULT_NONE) cardClass += " fault";
            else if (output->heating) cardClass += " heating";
            if (!output->enabled) cardClass += " disabled";

            html += "<div id='card" + String(id) + "' class='" + cardClass + "'>";

            // Header with name, status indicator, and fault chip
            html += "<h3><span><span id='status" + String(id) + "' class='status-indicator " + String(output->heating ? "on" : "off") + "'></span>";
            html += String(output->name);

            // Fault chip - shows fault state
            String faultChipClass = "fault-chip";
            String faultText = "";
            if (output->faultState != FAULT_NONE) {
                faultChipClass += " fault";
                faultText = output_manager_get_fault_name(output->faultState);
            } else if (output->sensorHealth != SENSOR_OK) {
                faultChipClass += " stale";
                faultText = output_manager_get_sensor_health_name(output->sensorHealth);
            } else {
                faultChipClass += " ok";
            }
            html += "<span id='faultChip" + String(id) + "' class='" + faultChipClass + "'>" + faultText + "</span>";

            html += "</span>";
            html += "<span style='font-size:12px;color:#999'>Output " + String(id) + "</span></h3>";

            // Current temperature (large)
            html += "<div class='temp-display'><span id='currTemp" + String(id) + "'>";
            if (output->enabled && output->currentTemp > -100) {
                html += String(output->currentTemp, 1);
            } else {
                html += "--.-";
            }
            html += "</span><small>°C</small></div>";

            // Target temperature slider
            html += "<div class='target-row'>";
            html += "<label>Target:</label>";
            html += "<input type='range' min='15' max='35' step='0.5' value='" + String(output->targetTemp, 1) + "' ";
            html += "oninput='document.getElementById(\"targetVal" + String(id) + "\").innerText=parseFloat(this.value).toFixed(1)+\"°C\"' ";
            html += "onchange='setTarget(" + String(id) + ",this.value)'>";
            html += "<span id='targetVal" + String(id) + "' class='target-val'>" + String(output->targetTemp, 1) + "°C</span>";
            html += "</div>";

            // Mode dropdown
            html += "<div class='mode-row'>";
            html += "<label>Mode:</label>";
            html += "<select onchange='setMode(" + String(id) + ",this.value)'>";
            html += "<option value='off'" + String(output->controlMode == CONTROL_MODE_OFF ? " selected" : "") + ">Off</option>";
            html += "<option value='manual'" + String(output->controlMode == CONTROL_MODE_MANUAL ? " selected" : "") + ">Manual</option>";
            html += "<option value='pid'" + String(output->controlMode == CONTROL_MODE_PID ? " selected" : "") + ">PID (Auto)</option>";
            html += "<option value='onoff'" + String(output->controlMode == CONTROL_MODE_ONOFF ? " selected" : "") + ">On/Off</option>";
            html += "<option value='timeprop'" + String(output->controlMode == CONTROL_MODE_TIME_PROP ? " selected" : "") + ">Time-Prop</option>";
            html += "</select>";
            html += "</div>";

            // Manual power slider (hidden unless manual mode)
            html += "<div id='powerRow" + String(id) + "' class='power-row" + String(output->controlMode == CONTROL_MODE_MANUAL ? " show" : "") + "'>";
            html += "<label>Power:</label>";
            html += "<input type='range' id='powerSlider" + String(id) + "' min='0' max='100' value='" + String(output->manualPower) + "' ";
            html += "oninput='document.getElementById(\"powerVal" + String(id) + "\").innerText=this.value+\"%\"' ";
            html += "onchange='setPower(" + String(id) + ",this.value)'>";
            html += "<span id='powerVal" + String(id) + "' class='power-val'>" + String(output->manualPower) + "%</span>";
            html += "</div>";

            // Clear fault button (shown only when in fault)
            String clearBtnStyle = output->faultState != FAULT_NONE ? "block" : "none";
            html += "<button id='clearFault" + String(id) + "' class='clear-fault-btn' style='display:" + clearBtnStyle + "' onclick='clearFault(" + String(id) + ")'>Clear Fault</button>";

            html += "</div>";
        }

        html += "</div>";

    } else {
        // ========== ADVANCED MODE (original) ==========
        // Multi-output auto-refresh script
        html += "<script>function updateOutputs(){fetch('/api/outputs').then(r=>r.json()).then(d=>{";
        html += "d.outputs.forEach((o,i)=>{let id=i+1;";
        html += "document.getElementById('temp'+id).innerText=o.temp+'°C';";
        html += "document.getElementById('target'+id).innerText=o.target+'°C';";
        html += "document.getElementById('heating'+id).innerText=o.heating?'ON':'OFF';";
        html += "document.getElementById('mode'+id).innerText=o.mode;";
        html += "document.getElementById('power-val'+id).innerText=o.power+'%';";
        html += "document.getElementById('power-fill'+id).style.width=o.power+'%';";
        html += "let card=document.getElementById('output'+id);";
        html += "card.style.background=o.heating?'#ffebee':'#e8f5e9';";
        html += "card.style.opacity=o.enabled?'1':'0.5';";
        html += "});});}updateOutputs();setInterval(updateOutputs,2000);</script>";

        html += "<h2>Outputs</h2>";
        html += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:15px;margin:20px 0'>";

        // Generate cards for all 3 outputs
        for (int i = 0; i < 3; i++) {
            OutputConfig_t* output = output_manager_get_output(i);
            if (!output) continue;

            int id = i + 1;
            String bgColor = output->heating ? "#ffebee" : "#e8f5e9";

            html += "<div id='output" + String(id) + "' style='background:" + bgColor + ";padding:15px;border-radius:8px;";
            html += "box-shadow:0 2px 5px rgba(0,0,0,0.1);opacity:" + String(output->enabled ? "1" : "0.5") + "'>";
            html += "<h3 style='margin:0 0 10px 0'>" + String(output->name) + " (Output " + String(id) + ")</h3>";

            // Status info
            html += "<div style='margin:8px 0'><strong>Current:</strong> <span id='temp" + String(id) + "'>" + String(output->currentTemp, 1) + "°C</span></div>";
            html += "<div style='margin:8px 0'><strong>Target:</strong> <span id='target" + String(id) + "'>" + String(output->targetTemp, 1) + "°C</span></div>";
            html += "<div style='margin:8px 0'><strong>Status:</strong> <span id='heating" + String(id) + "'>" + String(output->heating ? "ON" : "OFF") + "</span></div>";
            html += "<div style='margin:8px 0'><strong>Mode:</strong> <span id='mode" + String(id) + "'>" + String(output_manager_get_mode_name(output->controlMode)) + "</span></div>";

            // Power bar
            html += "<div style='margin:10px 0'><strong>Power: <span id='power-val" + String(id) + "'>" + String(output->currentPower) + "%</span></strong>";
            html += "<div style='width:100%;height:20px;background:#ddd;border-radius:5px;overflow:hidden;margin-top:5px'>";
            html += "<div id='power-fill" + String(id) + "' style='height:100%;background:linear-gradient(90deg,#4CAF50,#ff9800);transition:width 0.3s;width:" + String(output->currentPower) + "%'></div></div></div>";

            // Quick controls
            html += "<button onclick=\"fetch('/api/output/" + String(id) + "/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode:'off'})}).then(()=>updateOutputs())\" style='margin:5px 2px;padding:8px 12px;font-size:12px'>Off</button>";
            html += "<button onclick=\"fetch('/api/output/" + String(id) + "/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode:'manual',power:50})}).then(()=>updateOutputs())\" style='margin:5px 2px;padding:8px 12px;font-size:12px'>Manual 50%</button>";
            html += "<button onclick=\"fetch('/api/output/" + String(id) + "/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode:'pid'})}).then(()=>updateOutputs())\" style='margin:5px 2px;padding:8px 12px;font-size:12px'>PID</button>";

            html += "</div>";
        }

        html += "</div>";

        html += "<p style='margin-top:20px;text-align:center;color:#666'>";
        html += "Visit <a href='/outputs'>Outputs Configuration</a> for detailed control | ";
        html += "Visit <a href='/sensors'>Sensors</a> to manage sensors</p>";
    }

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
    doc["secureMode"] = secureMode;

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

/**
 * Handle /api/info
 */
static void handleInfo_API(void) {
    StaticJsonDocument<512> doc;

    // Device info
    doc["name"] = deviceName;
    doc["version"] = firmwareVersion;
    doc["mac"] = WiFi.macAddress();
    doc["ip"] = WiFi.localIP().toString();

    // Network info
    doc["wifi_connected"] = networkConnected;
    doc["wifi_ssid"] = networkSSID;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["ap_mode"] = networkAPMode;

    // System info
    unsigned long uptimeSeconds = millis() / 1000;
    doc["uptime_seconds"] = uptimeSeconds;
    doc["uptime_days"] = uptimeSeconds / 86400;
    doc["uptime_hours"] = (uptimeSeconds % 86400) / 3600;
    doc["uptime_minutes"] = (uptimeSeconds % 3600) / 60;

    doc["free_heap"] = ESP.getFreeHeap();
    doc["heap_size"] = ESP.getHeapSize();

    // MQTT status - would need to be passed from mqtt_manager
    // For now, just indicate if we have network
    doc["mqtt_configured"] = networkConnected && !networkAPMode;

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

/**
 * Handle /api/logs
 */
static void handleLogs_API(void) {
    DynamicJsonDocument doc(2048);
    JsonArray logs = doc.createNestedArray("logs");

    int logCount = logger_get_count();
    for (int i = 0; i < logCount; i++) {
        const char* logEntry = logger_get_entry(i);
        if (logEntry != nullptr) {
            logs.add(logEntry);
        }
    }

    doc["count"] = logCount;

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
 * Handle /api/control - Unified control endpoint with JSON
 */
static void handleControl(void) {
    // Parse JSON body
    if (!server.hasArg("plain")) {
        StaticJsonDocument<128> errorDoc;
        errorDoc["success"] = false;
        errorDoc["error"] = "No JSON body provided";
        String output;
        serializeJson(errorDoc, output);
        server.send(400, "application/json", output);
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        StaticJsonDocument<128> errorDoc;
        errorDoc["success"] = false;
        errorDoc["error"] = "Invalid JSON";
        String output;
        serializeJson(errorDoc, output);
        server.send(400, "application/json", output);
        return;
    }

    // Extract parameters
    float newTarget = targetTemp;
    String newMode = String(currentMode);
    bool changed = false;

    if (doc.containsKey("target")) {
        newTarget = doc["target"];
        newTarget = constrain(newTarget, 15.0, 45.0);
        changed = true;
    }

    if (doc.containsKey("mode")) {
        String argMode = doc["mode"].as<String>();
        if (argMode == "auto" || argMode == "off" || argMode == "on") {
            newMode = argMode;
            changed = true;
        }
    }

    // Call callback if registered and changes made
    if (changed && controlCallback != NULL) {
        controlCallback(newTarget, newMode.c_str());
    }

    // Return success response
    StaticJsonDocument<256> responseDoc;
    responseDoc["success"] = true;
    responseDoc["target"] = newTarget;
    responseDoc["mode"] = newMode;

    String output;
    serializeJson(responseDoc, output);
    server.send(200, "application/json", output);
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
 * Handle console page
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
    html += "else if(evt.type==='TEMP')color='#ce9178';";
    html += "else if(evt.type==='PID')color='#c586c0';";
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

/**
 * Handle console events API
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
    // Protected route
    if (!isAuthenticated()) { requireAuth(); return; }

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

    html += "<h2>Security</h2>";
    if (secureMode) {
        html += "<div class='info-box'>PIN protection is <strong>enabled</strong>. <a href='/logout'>Logout</a></div>";
    }
    html += "<div class='control'><label><input type='checkbox' name='secure_mode' value='1'";
    if (secureMode) html += " checked";
    html += "> Enable PIN Protection</label></div>";
    html += "<div class='control'><label>PIN (4-6 digits):</label>";
    html += "<input type='password' name='secure_pin' maxlength='6' pattern='[0-9]{4,6}' inputmode='numeric' placeholder='";
    if (strlen(securePin) > 0) {
        html += "****";
    } else {
        html += "Enter PIN";
    }
    html += "'></div>";
    html += "<p style='color:#666;font-size:14px'>Leave PIN blank to keep current PIN. When enabled, Settings, Outputs config, and control actions require login.</p>";

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

    html += "<div style='margin:20px 0;display:flex;justify-content:space-between;align-items:center'>";
    html += "<h2 style='margin:0'>Temperature Schedule</h2>";
    html += "</div>";

    // Output selector dropdown
    html += "<div style='margin:20px 0;padding:15px;background:#f0f0f0;border-radius:8px'>";
    html += "<label style='display:flex;align-items:center;gap:10px'>";
    html += "<strong>Select Output:</strong>";
    html += "<select id='output-selector' onchange='loadSchedule()' style='padding:8px;font-size:16px;border-radius:5px'>";
    for (int i = 0; i < 3; i++) {
        OutputConfig_t* output = output_manager_get_output(i);
        if (output) {
            html += "<option value='" + String(i) + "'>" + String(output->name) + " (Output " + String(i + 1) + ")</option>";
        }
    }
    html += "</select>";
    html += "</label>";
    html += "<p id='current-output-info' style='margin:10px 0;color:#666;font-size:14px'></p>";
    html += "<div id='next-schedule-info' style='margin-top:15px;padding:12px;background:#e3f2fd;border-radius:5px;border-left:4px solid #2196F3;display:none'>";
    html += "<strong>⏰ Next Scheduled Change:</strong> <span id='next-schedule-text'></span>";
    html += "</div>";
    html += "</div>";

    // Schedule slots container (will be filled by JavaScript)
    html += "<div id='schedule-slots' style='margin:20px 0'></div>";

    html += "<button type='button' onclick='saveSchedule()' style='margin:20px 0;padding:12px 30px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;font-size:16px'>Save Schedule</button>";

    html += "<div style='margin:20px 0;padding:15px;background:#e3f2fd;border-radius:8px'>";
    html += "<h4 style='margin:0 0 10px 0'>💡 Schedule Tips</h4>";
    html += "<ul style='margin:0;padding-left:20px;line-height:1.8'>";
    html += "<li>Each output has 8 independent schedule slots</li>";
    html += "<li>Enable/disable individual slots as needed</li>";
    html += "<li>Select active days for each slot (any combination)</li>";
    html += "<li>Schedule mode must be selected in Outputs page for this to activate</li>";
    html += "<li>Empty days = slot disabled</li>";
    html += "</ul>";
    html += "</div>";
    
    // JavaScript
    html += "<script>";
    html += "let currentOutputId=0;";
    html += "let currentSchedule=[];";

    // Load schedule from API
    html += "function loadSchedule(){";
    html += "currentOutputId=parseInt(document.getElementById('output-selector').value);";
    html += "fetch('/api/output/'+(currentOutputId+1)).then(r=>r.json()).then(d=>{";
    html += "currentSchedule=d.schedule||[];";
    html += "document.getElementById('current-output-info').innerHTML='Currently viewing schedule for <strong>'+d.name+'</strong>';";
    html += "renderSlots();";
    html += "updateNextSchedule();";
    html += "});}";

    // Calculate and display next scheduled change
    html += "function updateNextSchedule(){";
    html += "let now=new Date();";
    html += "let todayIdx=now.getDay();";
    html += "let dayChars='SMTWTFS';";
    html += "let activeSlots=currentSchedule.filter(s=>s.enabled&&s.days&&s.days.length>0);";
    html += "if(activeSlots.length===0){document.getElementById('next-schedule-info').style.display='none';return;}";
    html += "let nextSlot=null;let minDiff=999999;";
    html += "for(let s of activeSlots){";
    html += "for(let dayOffset=0;dayOffset<7;dayOffset++){";
    html += "let checkDay=(todayIdx+dayOffset)%7;";
    html += "if(s.days.indexOf(dayChars[checkDay])<0)continue;";
    html += "let slotTime=new Date(now);";
    html += "slotTime.setDate(now.getDate()+dayOffset);";
    html += "slotTime.setHours(s.hour,s.minute,0,0);";
    html += "let diff=(slotTime-now)/1000;";
    html += "if(diff>0&&diff<minDiff){minDiff=diff;nextSlot={time:slotTime,temp:s.targetTemp};}}}";
    html += "if(nextSlot){";
    html += "let h=nextSlot.time.getHours().toString().padStart(2,'0');";
    html += "let m=nextSlot.time.getMinutes().toString().padStart(2,'0');";
    html += "let date=nextSlot.time.toLocaleDateString('en-GB',{weekday:'short',day:'numeric',month:'short'});";
    html += "document.getElementById('next-schedule-text').innerHTML=date+' at '+h+':'+m+' → '+nextSlot.temp.toFixed(1)+'°C';";
    html += "document.getElementById('next-schedule-info').style.display='block';}else{";
    html += "document.getElementById('next-schedule-info').style.display='none';}}";

    // Render schedule slots HTML
    html += "function renderSlots(){";
    html += "let html='';";
    html += "let dayNames=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];";
    html += "for(let i=0;i<8;i++){";
    html += "let slot=currentSchedule[i]||{enabled:false,hour:0,minute:0,targetTemp:28.0,days:''};";
    html += "let isActive=slot.enabled&&slot.days.length>0;";
    html += "html+='<div class=\"schedule-slot\" style=\"border:2px solid '+(isActive?'#4CAF50':'#ddd')+';";
    html += "padding:15px;border-radius:10px;margin:15px 0;background:'+(isActive?'#f1f8f4':'#f9f9f9')+'\">';";
    html += "html+='<div style=\"display:flex;justify-content:space-between;align-items:center;margin-bottom:10px\">';";
    html += "html+='<strong>Slot '+(i+1)+'</strong>';";
    html += "html+='<label style=\"display:flex;align-items:center;gap:5px\">';";
    html += "html+='<input type=\"checkbox\" id=\"enabled'+i+'\" '+(isActive?'checked':'')+' style=\"width:auto\">';";
    html += "html+='<span>Active</span></label></div>';";

    // Time and temperature inputs
    html += "html+='<div style=\"display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:10px 0\">';";
    html += "html+='<div><label>Hour</label><input type=\"number\" id=\"hour'+i+'\" value=\"'+slot.hour+'\" min=\"0\" max=\"23\"></div>';";
    html += "html+='<div><label>Minute</label><input type=\"number\" id=\"minute'+i+'\" value=\"'+slot.minute+'\" min=\"0\" max=\"59\"></div>';";
    html += "html+='<div><label>Temp (°C)</label><input type=\"number\" id=\"temp'+i+'\" value=\"'+slot.targetTemp+'\" step=\"0.5\" min=\"15\" max=\"45\"></div>';";
    html += "html+='</div>';";

    // Day selector
    html += "html+='<div><label>Active Days:</label>';";
    html += "html+='<div style=\"display:flex;flex-wrap:wrap;gap:5px;margin-top:5px\">';";
    html += "for(let d=0;d<7;d++){";
    html += "let dayChar='SMTWTFS'[d];";
    html += "let checked=slot.days.indexOf(dayChar)>=0;";
    html += "html+='<label style=\"min-width:40px;flex:1;max-width:60px;text-align:center;padding:8px 4px;background:'+(checked?'#4CAF50':'#ddd')+';";
    html += "color:'+(checked?'white':'#666')+';border-radius:5px;cursor:pointer;font-size:12px\">';";
    html += "html+='<input type=\"checkbox\" id=\"day'+i+'_'+d+'\" '+(checked?'checked':'')+' ';";
    html += "html+='style=\"display:none\" onchange=\"this.parentElement.style.background=this.checked?\\'#4CAF50\\':\\'#ddd\\';";
    html += "this.parentElement.style.color=this.checked?\\'white\\':\\'#666\\'\">';";
    html += "html+=dayNames[d]+'</label>';}";
    html += "html+='</div></div>';";
    html += "html+='</div>';}";
    html += "document.getElementById('schedule-slots').innerHTML=html;}";

    // Save schedule to API
    html += "function saveSchedule(){";
    html += "let schedule=[];";
    html += "for(let i=0;i<8;i++){";
    html += "let enabled=document.getElementById('enabled'+i).checked;";
    html += "let hour=parseInt(document.getElementById('hour'+i).value)||0;";
    html += "let minute=parseInt(document.getElementById('minute'+i).value)||0;";
    html += "let targetTemp=parseFloat(document.getElementById('temp'+i).value)||28.0;";
    html += "let days='';";
    html += "for(let d=0;d<7;d++){";
    html += "if(document.getElementById('day'+i+'_'+d).checked){";
    html += "days+='SMTWTFS'[d];}}";
    html += "schedule.push({enabled:enabled&&days.length>0,hour:hour,minute:minute,targetTemp:targetTemp,days:days});}";
    html += "fetch('/api/output/'+(currentOutputId+1)+'/config',{";
    html += "method:'POST',headers:{'Content-Type':'application/json'},";
    html += "body:JSON.stringify({schedule:schedule})})";
    html += ".then(r=>r.ok?alert('Schedule saved!'):alert('Error saving schedule'));}";

    // Initialize on page load
    html += "loadSchedule();";
    html += "</script>";

    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}

/**
 * Handle save settings
 */
static void handleSaveSettings(void) {
    // Protected route
    if (!isAuthenticated()) { requireAuth(); return; }

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

    // Save security settings
    bool newSecureMode = server.hasArg("secure_mode");
    prefs.putBool("secure_mode", newSecureMode);
    secureMode = newSecureMode;

    // Only update PIN if a new one is provided
    if (server.hasArg("secure_pin") && server.arg("secure_pin").length() >= 4) {
        String newPin = server.arg("secure_pin");
        prefs.putString("secure_pin", newPin);
        strncpy(securePin, newPin.c_str(), sizeof(securePin) - 1);
        securePin[sizeof(securePin) - 1] = '\0';
        Serial.println("[WebServer] PIN updated");
    }

    Serial.printf("[WebServer] Secure mode: %s\n", secureMode ? "ON" : "OFF");

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

// handleSaveSchedule() removed - legacy code replaced by per-output schedule API
// Use /api/output/{id}/config to configure schedules per output

/**
 * Handle restart
 */
static void handleRestart(void) {
    // Protected route
    if (!isAuthenticated()) { requireAuth(); return; }

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
    // Protected route
    if (!isAuthenticated()) { requireAuth(); return; }

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
    // Protected route - check on first chunk only
    if (server.upload().status == UPLOAD_FILE_START && !isAuthenticated()) {
        return; // Will be handled by handleUploadDone
    }

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
    // Protected route
    if (!isAuthenticated()) { requireAuth(); return; }

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
    html += "</head><body";

    // Auto-apply dark mode from localStorage or system preference
    html += " onload=\"if(localStorage.getItem('darkMode')==='true'||((!localStorage.getItem('darkMode'))&&window.matchMedia('(prefers-color-scheme:dark)').matches)){document.body.classList.add('dark-mode');}\"";

    html += "><div class='container'>";
    html += "<div class='header'>";
    html += "<h1>" + String(deviceName) + "</h1>";
    html += "<div class='subtitle'>ESP32 Reptile Thermostat v" + String(firmwareVersion) + "</div>";
    html += "<div id='current-time' style='font-size:14px;margin-top:8px;opacity:0.95'></div>";
    html += "<script>";
    html += "function updateClock(){";
    html += "let now=new Date();";
    html += "let h=now.getHours().toString().padStart(2,'0');";
    html += "let m=now.getMinutes().toString().padStart(2,'0');";
    html += "let s=now.getSeconds().toString().padStart(2,'0');";
    html += "let date=now.toLocaleDateString('en-GB',{weekday:'short',day:'numeric',month:'short',year:'numeric'});";
    html += "document.getElementById('current-time').innerHTML='🕐 '+h+':'+m+':'+s+' | '+date;";
    html += "}updateClock();setInterval(updateClock,1000);";
    html += "</script>";
    html += "</div>";
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
// ===== MULTI-OUTPUT API HANDLERS =====

/**
 * GET /api/outputs - Get all outputs status
 */
static void handleOutputsAPI(void) {
    StaticJsonDocument<1536> doc;
    JsonArray outputs = doc.createNestedArray("outputs");

    for (int i = 0; i < 3; i++) {
        OutputConfig_t* output = output_manager_get_output(i);
        if (!output) continue;

        JsonObject obj = outputs.createNestedObject();
        obj["id"] = i + 1;
        obj["name"] = output->name;
        obj["enabled"] = output->enabled;
        obj["temp"] = serialized(String(output->currentTemp, 1));
        obj["target"] = serialized(String(output->targetTemp, 1));
        obj["mode"] = output_manager_get_mode_name(output->controlMode);
        obj["power"] = output->currentPower;
        obj["heating"] = output->heating;
        obj["sensor"] = output->sensorAddress;
        obj["deviceType"] = output_manager_get_device_type_name(output->deviceType);
        obj["hardwareType"] = output_manager_get_hardware_type_name(output->hardwareType);

        // Fault status
        obj["sensorHealth"] = output_manager_get_sensor_health_name(output->sensorHealth);
        obj["faultState"] = output_manager_get_fault_name(output->faultState);
        obj["inFault"] = (output->faultState != FAULT_NONE);
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

/**
 * GET /api/output/{id} - Get single output details
 */
static void handleOutputAPI(void) {
    String path = server.uri();
    int outputId = path.substring(path.lastIndexOf('/') + 1).toInt();
    int outputIndex = outputId - 1;

    if (outputIndex < 0 || outputIndex >= 3) {
        server.send(400, "text/plain", "Invalid output ID");
        return;
    }

    OutputConfig_t* output = output_manager_get_output(outputIndex);
    if (!output) {
        server.send(404, "text/plain", "Output not found");
        return;
    }

    StaticJsonDocument<1024> doc;
    doc["id"] = outputId;
    doc["name"] = output->name;
    doc["enabled"] = output->enabled;
    doc["temp"] = serialized(String(output->currentTemp, 1));
    doc["target"] = serialized(String(output->targetTemp, 1));
    doc["mode"] = output_manager_get_mode_name(output->controlMode);
    doc["power"] = output->currentPower;
    doc["heating"] = output->heating;
    doc["sensor"] = output->sensorAddress;
    doc["deviceType"] = output_manager_get_device_type_name(output->deviceType);
    doc["hardwareType"] = output_manager_get_hardware_type_name(output->hardwareType);
    doc["manualPower"] = output->manualPower;

    // PID parameters
    JsonObject pid = doc.createNestedObject("pid");
    pid["kp"] = serialized(String(output->pidKp, 2));
    pid["ki"] = serialized(String(output->pidKi, 2));
    pid["kd"] = serialized(String(output->pidKd, 2));

    // Time-proportional parameters
    JsonObject timeProp = doc.createNestedObject("timeProp");
    timeProp["cycleSec"] = output->timePropCycleSec;
    timeProp["minOnSec"] = output->timePropMinOnSec;
    timeProp["minOffSec"] = output->timePropMinOffSec;
    timeProp["dutyCycle"] = serialized(String(output->timePropDutyCycle, 1));
    timeProp["cycleState"] = output->timePropCurrentState;

    // Safety settings
    JsonObject safety = doc.createNestedObject("safety");
    safety["maxTempC"] = serialized(String(output->maxTempC, 1));
    safety["minTempC"] = serialized(String(output->minTempC, 1));
    safety["faultTimeoutSec"] = output->faultTimeoutSec;
    safety["faultMode"] = output->faultMode == FAULT_MODE_OFF ? "off" :
                          output->faultMode == FAULT_MODE_HOLD_LAST ? "hold" : "cap";
    safety["capPowerPct"] = output->capPowerPct;
    safety["autoResume"] = output->autoResumeOnSensorOk;

    // Current fault status
    JsonObject fault = doc.createNestedObject("fault");
    fault["sensorHealth"] = output_manager_get_sensor_health_name(output->sensorHealth);
    fault["state"] = output_manager_get_fault_name(output->faultState);
    fault["inFault"] = (output->faultState != FAULT_NONE);
    if (output->faultState != FAULT_NONE) {
        fault["durationSec"] = (millis() - output->faultStartTime) / 1000;
    }

    // Schedule
    JsonArray schedule = doc.createNestedArray("schedule");
    for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
        JsonObject slot = schedule.createNestedObject();
        slot["enabled"] = output->schedule[i].enabled;
        slot["hour"] = output->schedule[i].hour;
        slot["minute"] = output->schedule[i].minute;
        slot["targetTemp"] = serialized(String(output->schedule[i].targetTemp, 1));
        slot["days"] = output->schedule[i].days;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

/**
 * POST /api/output/{id}/control - Control output (mode, target, manual power)
 */
static void handleOutputControl(void) {
    // Protected route
    if (!isAuthenticated()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    String path = server.uri();
    int outputId = path.substring(path.lastIndexOf('/') - 1, path.lastIndexOf('/')).toInt();
    int outputIndex = outputId - 1;

    if (outputIndex < 0 || outputIndex >= 3) {
        server.send(400, "text/plain", "Invalid output ID");
        return;
    }

    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "No data received");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    // Update target temperature
    if (doc.containsKey("target")) {
        float target = doc["target"];
        output_manager_set_target(outputIndex, target);
    }

    // Update control mode
    if (doc.containsKey("mode")) {
        const char* modeStr = doc["mode"];
        ControlMode_t mode = CONTROL_MODE_OFF;

        if (strcmp(modeStr, "off") == 0) mode = CONTROL_MODE_OFF;
        else if (strcmp(modeStr, "manual") == 0) mode = CONTROL_MODE_MANUAL;
        else if (strcmp(modeStr, "pid") == 0 || strcmp(modeStr, "auto") == 0) mode = CONTROL_MODE_PID;
        else if (strcmp(modeStr, "onoff") == 0) mode = CONTROL_MODE_ONOFF;
        else if (strcmp(modeStr, "timeprop") == 0) mode = CONTROL_MODE_TIME_PROP;
        else if (strcmp(modeStr, "schedule") == 0) mode = CONTROL_MODE_SCHEDULE;

        output_manager_set_mode(outputIndex, mode);
    }

    // Update manual power
    if (doc.containsKey("power")) {
        int power = doc["power"];
        output_manager_set_manual_power(outputIndex, power);
    }

    // Save configuration
    output_manager_save_config();

    server.send(200, "text/plain", "OK");
}

/**
 * POST /api/output/{id}/config - Configure output (name, sensor, device type, PID, schedule)
 */
static void handleOutputConfig(void) {
    // Protected route
    if (!isAuthenticated()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    String path = server.uri();
    int outputId = path.substring(path.lastIndexOf('/') - 1, path.lastIndexOf('/')).toInt();
    int outputIndex = outputId - 1;

    if (outputIndex < 0 || outputIndex >= 3) {
        server.send(400, "text/plain", "Invalid output ID");
        return;
    }

    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "No data received");
        return;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    // Update name
    if (doc.containsKey("name")) {
        const char* name = doc["name"];
        output_manager_set_name(outputIndex, name);
    }

    // Update enabled state
    if (doc.containsKey("enabled")) {
        bool enabled = doc["enabled"];
        output_manager_set_enabled(outputIndex, enabled);
    }

    // Update sensor assignment
    if (doc.containsKey("sensor")) {
        const char* sensor = doc["sensor"];
        output_manager_set_sensor(outputIndex, sensor);
    }

    // Update PID parameters
    if (doc.containsKey("pid")) {
        JsonObject pid = doc["pid"];
        float kp = pid["kp"];
        float ki = pid["ki"];
        float kd = pid["kd"];
        output_manager_set_pid_params(outputIndex, kp, ki, kd);
    }

    // Update time-proportional parameters
    if (doc.containsKey("timeProp")) {
        JsonObject tp = doc["timeProp"];
        uint8_t cycleSec = tp["cycleSec"] | 30;
        uint8_t minOnSec = tp["minOnSec"] | 1;
        uint8_t minOffSec = tp["minOffSec"] | 1;
        output_manager_set_time_prop_params(outputIndex, cycleSec, minOnSec, minOffSec);
    }

    // Update schedule
    if (doc.containsKey("schedule")) {
        JsonArray schedule = doc["schedule"];
        for (int i = 0; i < schedule.size() && i < MAX_SCHEDULE_SLOTS; i++) {
            JsonObject slot = schedule[i];
            bool enabled = slot["enabled"];
            int hour = slot["hour"];
            int minute = slot["minute"];
            float target = slot["target"];
            output_manager_set_schedule_slot(outputIndex, i, enabled, hour, minute, target);
        }
    }

    // Save configuration
    output_manager_save_config();

    server.send(200, "text/plain", "OK");
}

/**
 * GET /api/sensors - Get all sensors
 */
static void handleSensorsAPI(void) {
    StaticJsonDocument<1024> doc;
    JsonArray sensors = doc.createNestedArray("sensors");

    int count = sensor_manager_get_count();
    for (int i = 0; i < count; i++) {
        const SensorInfo_t* sensor = sensor_manager_get_sensor(i);
        if (!sensor) continue;

        JsonObject obj = sensors.createNestedObject();
        obj["index"] = i;
        obj["address"] = sensor->addressString;
        obj["name"] = sensor->name;
        obj["temp"] = serialized(String(sensor->lastReading, 1));
        obj["lastRead"] = sensor->lastReadTime;
        obj["errors"] = sensor->errorCount;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

/**
 * POST /api/sensor/name - Rename a sensor
 * Body: {"address": "28FF...", "name": "New Name"}
 */
static void handleSensorName(void) {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "No data received");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    const char* address = doc["address"];
    const char* name = doc["name"];

    if (!address || !name) {
        server.send(400, "text/plain", "Missing address or name");
        return;
    }

    // Find sensor by address
    int count = sensor_manager_get_count();
    bool found = false;
    for (int i = 0; i < count; i++) {
        const SensorInfo_t* sensor = sensor_manager_get_sensor(i);
        if (sensor && strcmp(sensor->addressString, address) == 0) {
            sensor_manager_set_name(i, name);
            found = true;
            break;
        }
    }

    if (!found) {
        server.send(404, "text/plain", "Sensor not found");
        return;
    }

    // Save names
    sensor_manager_save_names();

    server.send(200, "text/plain", "OK");
}

// ===== HTML GENERATION HELPERS =====

static String buildCSS(void) {
    String css = "<style>";
    css += "*{box-sizing:border-box}";
    css += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0}";
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
    css += ".control{margin:20px 0}";
    css += "label{display:block;margin:10px 0 5px;font-weight:bold}";
    css += "input,select{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}";
    css += "button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin-top:10px;min-height:44px}";
    css += "button:hover{background:#45a049}";
    css += "button:active{background:#3d8b40}";
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

    // Mobile responsive media queries
    css += "@media(max-width:768px){";
    css += "body{padding:10px;font-size:16px}";
    css += ".container{padding:15px;border-radius:5px}";
    css += ".header{padding:15px;margin:-15px -15px 15px -15px}";
    css += ".header h1{font-size:20px}";
    css += ".header .subtitle{font-size:11px}";
    css += ".nav{gap:8px}";
    css += ".nav a{min-width:80px;padding:10px 12px;font-size:14px}";
    css += ".theme-toggle{min-width:44px;padding:10px}";
    css += ".status{flex-direction:column;gap:10px}";
    css += "h2{font-size:18px;margin-top:20px}";
    css += "input,select,button{font-size:16px;min-height:44px;padding:12px}";
    css += "button{padding:14px 20px}";
    css += ".stat-grid{grid-template-columns:1fr}";
    css += ".output-grid{grid-template-columns:1fr!important}";
    css += ".log-entry{font-size:13px;padding:8px}";
    css += "}";

    // Extra small screens (phones in portrait)
    css += "@media(max-width:480px){";
    css += "body{padding:8px}";
    css += ".container{padding:12px}";
    css += ".header{padding:12px;margin:-12px -12px 12px -12px}";
    css += ".header h1{font-size:18px}";
    css += ".nav{gap:6px}";
    css += ".nav a{min-width:70px;padding:8px 10px;font-size:12px}";
    css += ".theme-toggle{min-width:40px;padding:8px;font-size:16px}";
    css += "h2{font-size:16px}";
    css += ".stat-value{font-size:20px}";
    css += ".stat-label{font-size:11px}";
    css += "}";

    // Tablet landscape optimizations
    css += "@media(min-width:769px) and (max-width:1024px){";
    css += ".output-grid{grid-template-columns:repeat(2,1fr)!important}";
    css += "}";

    // Output card styling
    css += "[id^='output']{position:relative}";
    css += "[id^='output'] h3{color:#333}";
    css += "[id^='output'] div{color:#333}";
    css += "[id^='output'] strong{color:#333}";

    // Theme toggle button (must be before dark mode to avoid being overridden)
    css += ".theme-toggle{flex:0;min-width:50px;padding:12px 20px;background:#2196F3;color:white;border:none;border-radius:5px;cursor:pointer;font-size:18px;text-align:center;text-decoration:none;transition:background 0.3s;line-height:normal;box-sizing:border-box}";
    css += ".theme-toggle:hover{background:#0b7dda}";

    // Mode toggle dropdown
    css += ".mode-toggle{padding:10px 15px;background:#ff9800;color:white;border:none;border-radius:5px;cursor:pointer;font-size:14px;font-weight:bold}";

    // Dark mode overrides - improved contrast
    css += "body.dark-mode{background:#121212;color:#f0f0f0}";
    css += "body.dark-mode .container{background:#1e1e1e;box-shadow:0 2px 10px rgba(0,0,0,0.8)}";
    css += "body.dark-mode .header{background:linear-gradient(135deg,#2d5f2e,#1e3d1f)}";
    css += "body.dark-mode h2{color:#f0f0f0;border-bottom-color:#4d4d4d}";
    css += "body.dark-mode h3{color:#f0f0f0}";
    css += "body.dark-mode p{color:#d0d0d0}";
    css += "body.dark-mode label{color:#f0f0f0}";
    css += "body.dark-mode input,body.dark-mode select,body.dark-mode textarea{background:#2d2d2d;color:#f0f0f0;border:1px solid #4d4d4d}";
    css += "body.dark-mode input::placeholder{color:#808080}";
    css += "body.dark-mode button{background:#2d5f2e;color:#f0f0f0}";
    css += "body.dark-mode button:hover{background:#3d7f3e}";
    css += "body.dark-mode .btn-secondary{background:#1e4d7a}";
    css += "body.dark-mode .btn-secondary:hover{background:#163c5f}";
    css += "body.dark-mode .stat-card{background:#2d2d2d;border:1px solid #3d3d3d}";
    css += "body.dark-mode .stat-value{color:#4CAF50}";
    css += "body.dark-mode .stat-label{color:#d0d0d0}";
    css += "body.dark-mode .log-entry{border-bottom-color:#3d3d3d;color:#d0d0d0}";
    css += "body.dark-mode .footer{border-top-color:#3d3d3d;color:#d0d0d0}";
    css += "body.dark-mode .info-box{background:#1a2a3a;color:#d0f0ff;border-left-color:#2196F3}";
    css += "body.dark-mode .warning-box{background:#3a3020;color:#ffe0a0;border-left-color:#ffc107}";
    css += "body.dark-mode .nav a{background:#1e4d7a;color:#f0f0f0}";
    css += "body.dark-mode .nav a:hover{background:#2d6fa0}";
    css += "body.dark-mode .nav a.active{background:#2d5f2e}";
    css += "body.dark-mode .theme-toggle{background:#1e4d7a}";
    css += "body.dark-mode .theme-toggle:hover{background:#2d6fa0}";
    css += "body.dark-mode #next-schedule-info{background:#1a2a3a;color:#d0f0ff;border-left-color:#2196F3}";
    css += "body.dark-mode #pid-tuning{background:#2d2d2d;color:#f0f0f0}";
    css += "body.dark-mode #pid-tuning p{color:#d0d0d0}";
    css += "*{transition:background-color 0.3s,color 0.3s,border-color 0.3s}";

    css += "</style>";

    return css;
}

/**
 * Build navigation bar
 */
static String buildNavBar(const char* activePage) {
    String nav = "<div class='nav'>";

    // Home always visible
    nav += "<a href='/' class='" + String(strcmp(activePage, "home") == 0 ? "active" : "") + "'>🏠 Home</a>";

    // Advanced mode pages
    if (advancedMode) {
        nav += "<a href='/outputs' class='" + String(strcmp(activePage, "outputs") == 0 ? "active" : "") + "'>💡 Outputs</a>";
        nav += "<a href='/sensors' class='" + String(strcmp(activePage, "sensors") == 0 ? "active" : "") + "'>🌡️ Sensors</a>";
        nav += "<a href='/schedule' class='" + String(strcmp(activePage, "schedule") == 0 ? "active" : "") + "'>📅 Schedule</a>";
        nav += "<a href='/history' class='" + String(strcmp(activePage, "history") == 0 ? "active" : "") + "'>📈 History</a>";
        nav += "<a href='/info' class='" + String(strcmp(activePage, "info") == 0 ? "active" : "") + "'>ℹ️ Info</a>";
        nav += "<a href='/logs' class='" + String(strcmp(activePage, "logs") == 0 ? "active" : "") + "'>📋 Logs</a>";
        nav += "<a href='/console' class='" + String(strcmp(activePage, "console") == 0 ? "active" : "") + "'>🖥️ Console</a>";
    }

    // Settings and Safety always visible
    nav += "<a href='/settings' class='" + String(strcmp(activePage, "settings") == 0 ? "active" : "") + "'>⚙️ Settings</a>";
    nav += "<a href='/safety' class='" + String(strcmp(activePage, "safety") == 0 ? "active" : "") + "'>🛡️ Safety</a>";

    // Mode toggle dropdown
    nav += "<select class='mode-toggle' onchange='switchUIMode(this.value)'>";
    nav += "<option value='simple'" + String(!advancedMode ? " selected" : "") + ">Simple</option>";
    nav += "<option value='advanced'" + String(advancedMode ? " selected" : "") + ">Advanced</option>";
    nav += "</select>";

    nav += "<button class='theme-toggle' onclick='toggleDarkMode()' title='Toggle Dark Mode'>🌓</button>";
    nav += "</div>";

    // JavaScript for dark mode and UI mode switching
    nav += "<script>";
    nav += "function toggleDarkMode(){document.body.classList.toggle('dark-mode');localStorage.setItem('darkMode',document.body.classList.contains('dark-mode'));}";
    nav += "function switchUIMode(mode){fetch('/api/ui-mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode:mode})}).then(()=>location.reload());}";
    nav += "</script>";

    return nav;
}

// ===== NEW v1 API HANDLERS =====

/**
 * POST /api/output/{id}/clear-fault - Clear fault state for an output
 */
static void handleOutputClearFault(void) {
    // Protected route
    if (!isAuthenticated()) {
        server.send(401, "application/json", "{\"ok\":false,\"error\":{\"code\":\"UNAUTHORIZED\",\"message\":\"Authentication required\"}}");
        return;
    }

    String path = server.uri();
    // Extract output ID from path like /api/output/1/clear-fault
    int start = path.indexOf("/output/") + 8;
    int end = path.indexOf("/clear-fault");
    int outputId = path.substring(start, end).toInt();
    int outputIndex = outputId - 1;

    if (outputIndex < 0 || outputIndex >= 3) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":{\"code\":\"INVALID_OUTPUT\",\"message\":\"Invalid output ID\"}}");
        return;
    }

    OutputConfig_t* output = output_manager_get_output(outputIndex);
    if (!output) {
        server.send(404, "application/json", "{\"ok\":false,\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"Output not found\"}}");
        return;
    }

    // Try to clear the fault
    bool cleared = output_manager_clear_fault(outputIndex);

    StaticJsonDocument<256> doc;
    doc["ok"] = cleared;
    if (cleared) {
        doc["data"]["message"] = "Fault cleared";
    } else {
        doc["error"]["code"] = "FAULT_ACTIVE";
        doc["error"]["message"] = "Cannot clear fault - condition still active";
        doc["error"]["currentFault"] = output_manager_get_fault_name(output->faultState);
    }

    String response;
    serializeJson(doc, response);
    server.send(cleared ? 200 : 400, "application/json", response);
}

/**
 * GET /api/v1/health - System health and diagnostics
 */
static void handleHealthAPI(void) {
    StaticJsonDocument<1024> doc;
    doc["ok"] = true;

    JsonObject data = doc.createNestedObject("data");

    // System info
    data["uptime"] = millis() / 1000;
    data["freeHeap"] = ESP.getFreeHeap();
    data["minFreeHeap"] = ESP.getMinFreeHeap();
    data["heapSize"] = ESP.getHeapSize();
    data["chipModel"] = ESP.getChipModel();
    data["cpuFreqMHz"] = ESP.getCpuFreqMHz();

    // Flash info
    JsonObject flash = data.createNestedObject("flash");
    flash["size"] = ESP.getFlashChipSize();
    flash["speed"] = ESP.getFlashChipSpeed();

    // Network status
    JsonObject network = data.createNestedObject("network");
    network["wifiConnected"] = WiFi.isConnected();
    network["ssid"] = WiFi.SSID();
    network["rssi"] = WiFi.RSSI();
    network["ip"] = WiFi.localIP().toString();

    // Sensor status summary
    JsonObject sensors = data.createNestedObject("sensors");
    int sensorCount = sensor_manager_get_count();
    int healthySensors = 0;
    for (int i = 0; i < sensorCount; i++) {
        const SensorInfo_t* sensor = sensor_manager_get_sensor(i);
        if (sensor && sensor->discovered && sensor_manager_is_valid_temp(sensor->lastReading)) {
            healthySensors++;
        }
    }
    sensors["total"] = sensorCount;
    sensors["healthy"] = healthySensors;

    // Output status summary
    JsonObject outputsHealth = data.createNestedObject("outputs");
    int faultCount = 0;
    int activeCount = 0;
    for (int i = 0; i < 3; i++) {
        OutputConfig_t* output = output_manager_get_output(i);
        if (output) {
            if (output->faultState != FAULT_NONE) faultCount++;
            if (output->enabled && output->heating) activeCount++;
        }
    }
    outputsHealth["total"] = 3;
    outputsHealth["inFault"] = faultCount;
    outputsHealth["heating"] = activeCount;

    // Detailed output fault status
    JsonArray faults = data.createNestedArray("faults");
    for (int i = 0; i < 3; i++) {
        OutputConfig_t* output = output_manager_get_output(i);
        if (output && output->faultState != FAULT_NONE) {
            JsonObject fault = faults.createNestedObject();
            fault["outputId"] = i + 1;
            fault["outputName"] = output->name;
            fault["fault"] = output_manager_get_fault_name(output->faultState);
            fault["sensorHealth"] = output_manager_get_sensor_health_name(output->sensorHealth);
            fault["durationSec"] = (millis() - output->faultStartTime) / 1000;
        }
    }

    // Build info
    JsonObject build = data.createNestedObject("build");
    build["version"] = firmwareVersion;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ===== SAFETY PAGE AND API HANDLERS =====

/**
 * Safety Settings Page
 * Displays safety parameters, fault status, and emergency controls
 */
static void handleSafetyPage(void) {
    // Protected route
    if (!isAuthenticated()) { requireAuth(); return; }

    String html = webserver_get_html_header("Safety Settings", "safety");

    // Safe mode banner (if active)
    const SafetyState_t* safetyState = safety_manager_get_state();
    if (safetyState->safeMode) {
        html += "<div style='background:#f44336;color:white;padding:20px;border-radius:8px;margin:20px 0;text-align:center'>";
        html += "<h2 style='margin:0'>SAFE MODE ACTIVE</h2>";
        html += "<p style='margin:10px 0'>Reason: " + String(safety_manager_get_reason_name(safetyState->safeModeReason)) + "</p>";
        html += "<p style='margin:10px 0'>All outputs are disabled for safety.</p>";
        html += "<button onclick='exitSafeMode()' style='padding:10px 20px;font-size:16px;cursor:pointer'>Exit Safe Mode</button>";
        html += "</div>";
    }

    // System Safety Status
    html += "<h2>System Safety Status</h2>";
    html += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px;margin:20px 0'>";

    // Watchdog status
    html += "<div style='background:#e3f2fd;padding:15px;border-radius:8px'>";
    html += "<strong>Watchdog Timer</strong><br>";
    html += safetyState->watchdogEnabled ? "<span style='color:green'>Active</span>" : "<span style='color:orange'>Disabled</span>";
    html += "</div>";

    // Boot count
    html += "<div style='background:#e3f2fd;padding:15px;border-radius:8px'>";
    html += "<strong>Boot Count</strong><br>";
    html += String(safetyState->bootCount) + " / " + String(BOOT_LOOP_THRESHOLD);
    html += "</div>";

    // System status
    html += "<div style='background:#e3f2fd;padding:15px;border-radius:8px'>";
    html += "<strong>System Status</strong><br>";
    html += safetyState->safeMode ? "<span style='color:red'>SAFE MODE</span>" : "<span style='color:green'>Normal</span>";
    html += "</div>";

    html += "</div>";

    // Emergency Stop Button
    html += "<div style='background:#ffebee;padding:20px;border-radius:8px;margin:20px 0;text-align:center'>";
    html += "<button onclick='emergencyStop()' style='background:#f44336;color:white;padding:15px 40px;font-size:18px;border:none;border-radius:8px;cursor:pointer'>";
    html += "EMERGENCY STOP - All Outputs OFF</button>";
    html += "<p style='margin:10px 0 0 0;color:#666;font-size:14px'>Immediately disables all heating outputs</p>";
    html += "</div>";

    // Output selector
    html += "<h2>Per-Output Safety Settings</h2>";
    html += "<div style='margin:20px 0;padding:15px;background:#f0f0f0;border-radius:8px'>";
    html += "<label style='display:flex;align-items:center;gap:10px'>";
    html += "<strong>Select Output:</strong>";
    html += "<select id='output-selector' onchange='loadSafetySettings()' style='padding:8px;font-size:16px;border-radius:5px'>";
    for (int i = 0; i < 3; i++) {
        OutputConfig_t* output = output_manager_get_output(i);
        if (output) {
            html += "<option value='" + String(i) + "'>" + String(output->name) + " (Output " + String(i + 1) + ")</option>";
        }
    }
    html += "</select></label></div>";

    // Fault Status Panel
    html += "<div id='fault-status' style='margin:20px 0;padding:20px;background:#fff3e0;border-radius:8px;border-left:4px solid #ff9800'>";
    html += "<h3 style='margin:0 0 10px 0'>Current Fault Status</h3>";
    html += "<div id='fault-details'>Loading...</div>";
    html += "</div>";

    // Safety Parameters Form
    html += "<div style='background:#f9f9f9;padding:20px;border-radius:8px;margin:20px 0'>";
    html += "<h3>Safety Limits</h3>";

    html += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:15px'>";

    // Max Temperature
    html += "<div>";
    html += "<label><strong>Max Temperature (C)</strong><br>";
    html += "<input type='number' id='maxTempC' min='20' max='80' step='0.5' style='width:100%;padding:8px;margin-top:5px'>";
    html += "</label>";
    html += "<p style='color:#666;font-size:12px;margin:5px 0'>Hard cutoff - output forced OFF above this</p>";
    html += "</div>";

    // Min Temperature
    html += "<div>";
    html += "<label><strong>Min Temperature (C)</strong><br>";
    html += "<input type='number' id='minTempC' min='0' max='30' step='0.5' style='width:100%;padding:8px;margin-top:5px'>";
    html += "</label>";
    html += "<p style='color:#666;font-size:12px;margin:5px 0'>Warning threshold - triggers under-temp fault</p>";
    html += "</div>";

    // Fault Timeout
    html += "<div>";
    html += "<label><strong>Sensor Timeout (seconds)</strong><br>";
    html += "<input type='number' id='faultTimeoutSec' min='10' max='300' step='5' style='width:100%;padding:8px;margin-top:5px'>";
    html += "</label>";
    html += "<p style='color:#666;font-size:12px;margin:5px 0'>Time without reading before sensor stale fault</p>";
    html += "</div>";

    html += "</div>";

    html += "<h3 style='margin-top:20px'>Fault Response</h3>";

    html += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:15px'>";

    // Fault Mode
    html += "<div>";
    html += "<label><strong>Fault Mode</strong><br>";
    html += "<select id='faultMode' style='width:100%;padding:8px;margin-top:5px'>";
    html += "<option value='off'>OFF - Turn output off (safest)</option>";
    html += "<option value='hold'>HOLD - Maintain last power level</option>";
    html += "<option value='cap'>CAP - Limit to max power %</option>";
    html += "</select></label>";
    html += "</div>";

    // Cap Power
    html += "<div>";
    html += "<label><strong>Power Cap (%)</strong><br>";
    html += "<input type='number' id='capPowerPct' min='0' max='50' step='5' style='width:100%;padding:8px;margin-top:5px'>";
    html += "</label>";
    html += "<p style='color:#666;font-size:12px;margin:5px 0'>Max power when in CAP fault mode</p>";
    html += "</div>";

    // Auto Resume
    html += "<div>";
    html += "<label style='display:flex;align-items:center;gap:10px;margin-top:20px'>";
    html += "<input type='checkbox' id='autoResumeOnSensorOk'>";
    html += "<span><strong>Auto-resume on sensor recovery</strong><br>";
    html += "<span style='color:#666;font-size:12px'>Automatically clear sensor faults when sensor returns</span></span>";
    html += "</label></div>";

    html += "</div>";

    // Save and Reset Buttons
    html += "<div style='margin-top:20px;display:flex;gap:10px;flex-wrap:wrap'>";
    html += "<button onclick='saveSafetySettings()' style='background:#4CAF50;color:white;padding:12px 30px;border:none;border-radius:5px;cursor:pointer;font-size:16px'>Save Settings</button>";
    html += "<button onclick='clearFault()' id='clear-fault-btn' style='background:#ff9800;color:white;padding:12px 30px;border:none;border-radius:5px;cursor:pointer;font-size:16px' disabled>Clear Fault</button>";
    html += "</div>";

    html += "</div>";

    // Fault Analysis Section
    html += "<h2>Fault Analysis</h2>";
    html += "<div id='fault-analysis' style='background:#f9f9f9;padding:20px;border-radius:8px'>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#e0e0e0'>";
    html += "<th style='padding:10px;text-align:left'>Field</th>";
    html += "<th style='padding:10px;text-align:left'>Value</th>";
    html += "</tr></thead>";
    html += "<tbody id='fault-analysis-body'></tbody>";
    html += "</table></div>";

    // JavaScript
    html += "<script>";
    html += "let currentOutput=0;";

    // Load safety settings for selected output
    html += "function loadSafetySettings(){";
    html += "currentOutput=parseInt(document.getElementById('output-selector').value);";
    html += "fetch('/api/output/'+(currentOutput+1)).then(r=>r.json()).then(d=>{";
    // Populate form fields
    html += "document.getElementById('maxTempC').value=parseFloat(d.safety.maxTempC);";
    html += "document.getElementById('minTempC').value=parseFloat(d.safety.minTempC);";
    html += "document.getElementById('faultTimeoutSec').value=d.safety.faultTimeoutSec;";
    html += "document.getElementById('faultMode').value=d.safety.faultMode;";
    html += "document.getElementById('capPowerPct').value=d.safety.capPowerPct;";
    html += "document.getElementById('autoResumeOnSensorOk').checked=d.safety.autoResume;";
    // Update fault status
    html += "let faultDiv=document.getElementById('fault-details');";
    html += "let clearBtn=document.getElementById('clear-fault-btn');";
    html += "if(d.fault.inFault){";
    html += "faultDiv.innerHTML='<span style=\"color:red;font-weight:bold\">'+d.fault.state+'</span><br>'+";
    html += "'Duration: '+(d.fault.durationSec||0)+' seconds<br>'+";
    html += "'Sensor Health: '+d.fault.sensorHealth;";
    html += "clearBtn.disabled=false;";
    html += "document.getElementById('fault-status').style.borderLeftColor='#f44336';";
    html += "document.getElementById('fault-status').style.background='#ffebee';";
    html += "}else{";
    html += "faultDiv.innerHTML='<span style=\"color:green\">No active faults</span>';";
    html += "clearBtn.disabled=true;";
    html += "document.getElementById('fault-status').style.borderLeftColor='#4CAF50';";
    html += "document.getElementById('fault-status').style.background='#e8f5e9';";
    html += "}";
    // Update fault analysis table
    html += "let tbody=document.getElementById('fault-analysis-body');";
    html += "tbody.innerHTML='';";
    html += "let rows=[";
    html += "['Sensor Health',d.fault.sensorHealth],";
    html += "['Fault State',d.fault.state],";
    html += "['In Fault',d.fault.inFault?'Yes':'No'],";
    html += "['Fault Duration (sec)',d.fault.durationSec||'N/A'],";
    html += "['Current Temperature',d.temp+'C'],";
    html += "['Current Power',d.power+'%'],";
    html += "['Control Mode',d.mode],";
    html += "['Max Temp Limit',d.safety.maxTempC+'C'],";
    html += "['Min Temp Limit',d.safety.minTempC+'C'],";
    html += "['Fault Mode',d.safety.faultMode.toUpperCase()],";
    html += "];";
    html += "rows.forEach(r=>{";
    html += "let tr=document.createElement('tr');";
    html += "tr.innerHTML='<td style=\"padding:8px;border-bottom:1px solid #ddd\">'+r[0]+'</td>'+";
    html += "'<td style=\"padding:8px;border-bottom:1px solid #ddd\">'+r[1]+'</td>';";
    html += "tbody.appendChild(tr);});";
    html += "});}";

    // Save safety settings
    html += "function saveSafetySettings(){";
    html += "let data={";
    html += "maxTempC:parseFloat(document.getElementById('maxTempC').value),";
    html += "minTempC:parseFloat(document.getElementById('minTempC').value),";
    html += "faultTimeoutSec:parseInt(document.getElementById('faultTimeoutSec').value),";
    html += "faultMode:document.getElementById('faultMode').value,";
    html += "capPowerPct:parseInt(document.getElementById('capPowerPct').value),";
    html += "autoResumeOnSensorOk:document.getElementById('autoResumeOnSensorOk').checked";
    html += "};";
    html += "fetch('/api/output/'+(currentOutput+1)+'/safety',{method:'POST',";
    html += "headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})";
    html += ".then(r=>r.json()).then(d=>{";
    html += "if(d.ok){alert('Safety settings saved!');loadSafetySettings();}";
    html += "else{alert('Error: '+(d.error?.message||'Unknown error'));}";
    html += "});}";

    // Clear fault
    html += "function clearFault(){";
    html += "if(!confirm('Clear fault for this output?'))return;";
    html += "fetch('/api/output/'+(currentOutput+1)+'/clear-fault',{method:'POST'})";
    html += ".then(r=>r.json()).then(d=>{";
    html += "if(d.ok){alert('Fault cleared!');loadSafetySettings();}";
    html += "else{alert('Cannot clear: '+(d.error?.message||'Conditions still exist'));}";
    html += "});}";

    // Emergency stop
    html += "function emergencyStop(){";
    html += "if(!confirm('EMERGENCY STOP\\n\\nThis will immediately turn OFF all heating outputs.\\n\\nContinue?'))return;";
    html += "fetch('/api/safety/emergency-stop',{method:'POST'})";
    html += ".then(r=>r.json()).then(d=>{";
    html += "if(d.ok){alert('All outputs disabled!');location.reload();}";
    html += "else{alert('Error: '+d.error);}";
    html += "});}";

    // Exit safe mode
    html += "function exitSafeMode(){";
    html += "if(!confirm('Exit safe mode?\\n\\nOutputs will return to their configured modes.'))return;";
    html += "fetch('/api/safety/exit-safe-mode',{method:'POST'})";
    html += ".then(r=>r.json()).then(d=>{";
    html += "if(d.ok){alert('Exited safe mode');location.reload();}";
    html += "else{alert('Error: '+d.error);}";
    html += "});}";

    // Auto-refresh fault status
    html += "setInterval(loadSafetySettings,5000);";
    html += "loadSafetySettings();";
    html += "</script>";

    html += webserver_get_html_footer(millis() / 1000);
    server.send(200, "text/html", html);
}

/**
 * POST /api/output/{id}/safety - Update safety settings for an output
 */
static void handleSafetyAPI(void) {
    // Protected route
    if (!isAuthenticated()) {
        server.send(401, "application/json", "{\"ok\":false,\"error\":{\"code\":\"UNAUTHORIZED\",\"message\":\"Authentication required\"}}");
        return;
    }

    String path = server.uri();
    // Extract output ID from path like /api/output/1/safety
    int slashPos = path.indexOf("/output/") + 8;
    int outputId = path.substring(slashPos, slashPos + 1).toInt();
    int outputIndex = outputId - 1;

    if (outputIndex < 0 || outputIndex >= 3) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":{\"code\":\"INVALID_OUTPUT\",\"message\":\"Invalid output ID\"}}");
        return;
    }

    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":{\"code\":\"NO_DATA\",\"message\":\"No data received\"}}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":{\"code\":\"INVALID_JSON\",\"message\":\"Invalid JSON\"}}");
        return;
    }

    OutputConfig_t* output = output_manager_get_output(outputIndex);
    if (!output) {
        server.send(404, "application/json", "{\"ok\":false,\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"Output not found\"}}");
        return;
    }

    // Update safety limits
    float maxTempC = doc["maxTempC"] | output->maxTempC;
    float minTempC = doc["minTempC"] | output->minTempC;
    uint16_t faultTimeoutSec = doc["faultTimeoutSec"] | output->faultTimeoutSec;

    // Validate ranges
    if (maxTempC < 20 || maxTempC > 80) maxTempC = output->maxTempC;
    if (minTempC < 0 || minTempC > 30) minTempC = output->minTempC;
    if (faultTimeoutSec < 10 || faultTimeoutSec > 300) faultTimeoutSec = output->faultTimeoutSec;
    if (maxTempC <= minTempC) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":{\"code\":\"INVALID_RANGE\",\"message\":\"maxTempC must be greater than minTempC\"}}");
        return;
    }

    output_manager_set_safety_limits(outputIndex, maxTempC, minTempC, faultTimeoutSec);

    // Update fault mode
    if (doc.containsKey("faultMode")) {
        const char* modeStr = doc["faultMode"];
        FaultMode_t faultMode = FAULT_MODE_OFF;
        if (strcmp(modeStr, "hold") == 0) faultMode = FAULT_MODE_HOLD_LAST;
        else if (strcmp(modeStr, "cap") == 0) faultMode = FAULT_MODE_CAP_POWER;

        uint8_t capPowerPct = doc["capPowerPct"] | output->capPowerPct;
        if (capPowerPct > 50) capPowerPct = 50;  // Cap at 50% for safety

        output_manager_set_fault_mode(outputIndex, faultMode, capPowerPct);
    }

    // Update auto-resume setting
    if (doc.containsKey("autoResumeOnSensorOk")) {
        output->autoResumeOnSensorOk = doc["autoResumeOnSensorOk"];
    }

    // Save to NVS
    output_manager_save_config();

    console_add_event_f(CONSOLE_EVENT_SYSTEM, "Output %d safety settings updated via web", outputIndex + 1);

    server.send(200, "application/json", "{\"ok\":true}");
}

/**
 * POST /api/safety/emergency-stop - Emergency stop all outputs
 */
static void handleEmergencyStop(void) {
    // Protected route
    if (!isAuthenticated()) {
        server.send(401, "application/json", "{\"ok\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    safety_manager_emergency_stop();
    server.send(200, "application/json", "{\"ok\":true,\"message\":\"All outputs disabled\"}");
}

/**
 * POST /api/safety/exit-safe-mode - Exit safe mode
 */
static void handleExitSafeMode(void) {
    // Protected route
    if (!isAuthenticated()) {
        server.send(401, "application/json", "{\"ok\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    if (safety_manager_exit_safe_mode()) {
        server.send(200, "application/json", "{\"ok\":true,\"message\":\"Exited safe mode\"}");
    } else {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"Cannot exit safe mode\"}");
    }
}
