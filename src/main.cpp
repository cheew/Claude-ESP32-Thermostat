/*
 * ESP32 Reptile Thermostat with PID Control
 * Version: 1.3.2
 * Last Updated: January 7, 2026
 * 
 * Features:
 * - PID temperature control with AC dimmer
 * - 2.8" TFT touchscreen display (ILI9341)
 * - Web interface with Info, Logs, Schedule, and Settings pages
 * - Temperature scheduling (up to 8 time slots, day-specific)
 * - MQTT integration + Home Assistant auto-discovery
 * - AP mode for easy setup
 * - Three display screens: Main, Settings, Simple View
 * - mDNS device discovery
 * - OTA firmware updates (manual + GitHub auto-update)
 * - System logging with 20-entry circular buffer
 * - Mobile-responsive web interface
 * 
 * Changelog v1.3.2:
 * - Fixed mDNS hostname bug (removed extra hyphen)
 * - Added hostname sanitization (removes invalid characters)
 * - Added trim() to remove trailing spaces from device name
 * 
 * Changelog v1.3.1:
 * - Added temperature scheduling system (up to 8 time slots)
 * - NTP time synchronization for accurate scheduling
 * - Schedule page with visual editor and day selection
 * - Next scheduled change indicator
 * - Memory optimized (no temp history for now)
 * - Fixed WiFi connection issues from v1.3.0
 * 
 * Changelog v1.2.0:
 * - Enhanced web interface with Info and Logs pages
 * - Improved navigation bar across all pages
 * - Better mobile responsiveness
 * - System logging with 20-entry circular buffer
 * - Uptime tracking and memory stats
 * - Network status details with signal strength
 * - GitHub auto-update capability
 * 
 * Changelog v1.1.0:
 * - Added mDNS/Bonjour service discovery
 * - Added OTA firmware update capability
 * - Added firmware version display
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <RBDdimmer.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <ESPmDNS.h>
#include <Update.h>

// Pin definitions
#define ONE_WIRE_BUS 4    // GPIO4 (D4)
#define DIMMER_PIN 5      // GPIO5 - PWM control pin for AC dimmer
#define ZEROCROSS_PIN 27  // GPIO27 - Zero-cross detection pin (was 18, moved for TFT)

// Firmware version
#define FIRMWARE_VERSION "1.3.2"

// GitHub repository for auto-updates
const char* github_user = "cheew";
const char* github_repo = "Claude-ESP32-Thermostat";
const char* github_firmware_asset = "firmware.bin";

// TFT Display is connected via SPI (defined in TFT_eSPI User_Setup.h):
// MOSI = GPIO 23, SCK = GPIO 18, CS = GPIO 15, DC = GPIO 2, RST = GPIO 33
// Touch: T_CS = GPIO 22, shares MOSI/MISO/SCK with display

// WiFi credentials
const char* ssid = "mesh";
const char* password = "Oasis0asis";

// MQTT settings
const char* mqtt_server = "192.168.1.123";
const int mqtt_port = 1883;
const char* mqtt_user = "admin";
const char* mqtt_password = "Oasis0asis!!";
const char* mqtt_client_id = "esp32_thermostat";

// Home Assistant discovery topics
const char* ha_discovery_prefix = "homeassistant";
const char* device_name = "Reptile Thermostat";
const char* device_id = "reptile_thermostat_01";

// MQTT topics
String base_topic = String("reptile/") + device_id;
String temp_topic = base_topic + "/temperature";
String state_topic = base_topic + "/state";
String set_temp_topic = base_topic + "/setpoint/set";
String mode_topic = base_topic + "/mode";
String mode_set_topic = base_topic + "/mode/set";

// Function declarations
void connectWiFi();
void startAPMode();
void setupMDNS();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void readTemperature();
void updatePID();
void publishStatus();
void sendHADiscovery();
void setupWebServer();
void handleRoot();
void handleStatus();
void handleSet();
void handlePID();
void handleSettings();
void handleSaveSettings();
void handleRestart();
void handleUpdate();
void handleUpload();
void handleUploadDone();
void handleCheckUpdate();
void handleAutoUpdate();
void handleInfo();
void handleLogs();
void handleSchedule();
void handleSaveSchedule();
void addLog(String message);
void checkSchedule();
void loadSchedule();
void saveSchedule();
String getHTMLHeader(String title, String activePage);
String getHTMLFooter();
void initDisplay();
void updateDisplay();
void updateSimpleDisplay();
void drawMainScreen();
void drawSettingsScreen();
void drawSimpleScreen();
void checkTouch();

// Temperature sensor setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// AC Dimmer setup
dimmerLamp dimmer(DIMMER_PIN, ZEROCROSS_PIN);

// TFT Display setup
TFT_eSPI tft = TFT_eSPI();

// Display state
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 500;  // Update display every 500ms
bool displayNeedsUpdate = true;
int currentScreen = 0;  // 0 = main, 1 = settings, 2 = simple view
String newDeviceName = "";  // For name editing

// Web server
WebServer server(80);

// MQTT client
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Preferences for storing settings
Preferences preferences;

// AP Mode settings
bool apMode = false;
const char* ap_ssid = "ReptileThermostat";
const char* ap_password = "thermostat123";
IPAddress apIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// Thermostat variables
float currentTemp = 0.0;
float targetTemp = 28.0;  // Default 28°C
bool heatingState = false;
String mode = "auto";     // auto, off, on
int powerOutput = 0;      // Dimmer power 0-100%

// Logging system
#define MAX_LOGS 20
String logMessages[MAX_LOGS];
int logIndex = 0;
unsigned long bootTime = 0;

// Schedule system
#define MAX_SCHEDULE_SLOTS 8
struct ScheduleSlot {
  bool enabled;
  int hour;        // 0-23
  int minute;      // 0-59
  float targetTemp; // Target temperature
  String days;     // "SMTWTFS" - 7 chars for days (S=Sunday, M=Monday, etc)
};

ScheduleSlot schedule[MAX_SCHEDULE_SLOTS];
bool scheduleEnabled = false;
int scheduleSlotCount = 0;
String nextScheduleTime = "";
float nextScheduleTemp = 0;

// PID variables
float Kp = 10.0;          // Proportional gain
float Ki = 0.5;           // Integral gain
float Kd = 5.0;           // Derivative gain
float integral = 0.0;
float previousError = 0.0;
unsigned long lastPidTime = 0;

unsigned long lastTempRead = 0;
unsigned long lastMqttPublish = 0;
const unsigned long tempReadInterval = 2000;
const unsigned long mqttPublishInterval = 30000;
const unsigned long pidInterval = 1000;  // PID update every 1 second

void setup() {
  Serial.begin(115200);
  
  // Record boot time
  bootTime = millis();
  
  // Initialize TFT Display first
  initDisplay();
  
  addLog("System boot - v" + String(FIRMWARE_VERSION));
  
  // Initialize AC dimmer
  dimmer.begin(NORMAL_MODE, ON);
  dimmer.setPower(0);
  
  // Load saved settings
  preferences.begin("thermostat", false);
  targetTemp = preferences.getFloat("target", 28.0);
  mode = preferences.getString("mode", "auto");
  Kp = preferences.getFloat("Kp", 10.0);
  Ki = preferences.getFloat("Ki", 0.5);
  Kd = preferences.getFloat("Kd", 5.0);
  preferences.end();
  
  // Load schedule (after preferences closed)
  loadSchedule();
  
  // Check if we have WiFi credentials saved
  preferences.begin("thermostat", true);
  String savedSSID = preferences.getString("wifi_ssid", "");
  if (savedSSID.length() == 0) {
    // No saved WiFi, start in AP mode
    preferences.end();
    startAPMode();
  } else {
    preferences.end();
    // Try to connect to saved WiFi
    connectWiFi();
  }
  
  // Initialize temperature sensor
  sensors.begin();
  
  // Setup time (NTP) if connected to WiFi
  if (WiFi.status() == WL_CONNECTED && !apMode) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("Time sync started");
    addLog("Time sync started");
  }
  
  // Setup MQTT (only if connected to WiFi)
  if (WiFi.status() == WL_CONNECTED && !apMode) {
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(512);
  }
  
  // Setup web server routes
  setupWebServer();
  
  Serial.println("ESP32 Reptile Thermostat with PID Started");
  if (apMode) {
    Serial.println("Running in AP Mode");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }
  
  // Initial display update
  drawMainScreen();
}

void loop() {
  // Handle WiFi connection (only if not in AP mode)
  if (!apMode && WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  
  // Handle MQTT connection (only if not in AP mode)
  if (!apMode && !mqtt.connected()) {
    reconnectMQTT();
  }
  if (!apMode) {
    mqtt.loop();
  }
  
  // Handle web server
  server.handleClient();
  
  // Read temperature
  if (millis() - lastTempRead >= tempReadInterval) {
    readTemperature();
    lastTempRead = millis();
  }
  
  // Update PID control
  if (millis() - lastPidTime >= pidInterval) {
    updatePID();
    lastPidTime = millis();
  }
  
  // Update TFT display
  if (millis() - lastDisplayUpdate >= displayUpdateInterval) {
    if (currentScreen == 0) {
      updateDisplay();  // Update main screen
    } else if (currentScreen == 2) {
      updateSimpleDisplay();  // Update simple screen
    }
    // Settings screen is static, no updates needed
    lastDisplayUpdate = millis();
  }
  
  // Check for touch input
  checkTouch();
  
  // Check schedule (every minute)
  static unsigned long lastScheduleCheck = 0;
  if (scheduleEnabled && millis() - lastScheduleCheck >= 60000) {  // Check every minute
    checkSchedule();
    lastScheduleCheck = millis();
  }
  
  // Publish to MQTT periodically (only if not in AP mode)
  if (!apMode && millis() - lastMqttPublish >= mqttPublishInterval) {
    publishStatus();
    lastMqttPublish = millis();
  }
}

void connectWiFi() {
  preferences.begin("thermostat", true);
  String savedSSID = preferences.getString("wifi_ssid", ssid);
  String savedPassword = preferences.getString("wifi_pass", password);
  preferences.end();
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(savedSSID);
  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    apMode = false;
    addLog("WiFi connected: " + WiFi.localIP().toString());
    setupMDNS();
  } else {
    Serial.println("\nWiFi connection failed, starting AP mode");
    addLog("WiFi failed, starting AP mode");
    startAPMode();
  }
}

void setupMDNS() {
  preferences.begin("thermostat", true);
  String deviceName = preferences.getString("device_name", "Thermostat");
  preferences.end();
  
  // Create hostname from device name (replace spaces with hyphens, lowercase)
  String hostname = deviceName;
  hostname.trim();  // Remove any leading/trailing whitespace
  hostname.replace(" ", "-");
  hostname.toLowerCase();
  
  // Remove any non-alphanumeric characters except hyphens
  String cleanHostname = "";
  for (int i = 0; i < hostname.length(); i++) {
    char c = hostname.charAt(i);
    if (isalnum(c) || c == '-') {
      cleanHostname += c;
    }
  }
  hostname = cleanHostname;
  
  // Start mDNS
  if (MDNS.begin(hostname.c_str())) {
    Serial.print("mDNS responder started: ");
    Serial.print(hostname);
    Serial.println(".local");
    
    // Add service
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "type", "reptile-thermostat");
    MDNS.addServiceTxt("http", "tcp", "version", FIRMWARE_VERSION);
    MDNS.addServiceTxt("http", "tcp", "name", deviceName.c_str());
  } else {
    Serial.println("Error setting up mDNS responder!");
  }
}

void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.println("AP Mode Started");
  Serial.print("AP SSID: ");
  Serial.println(ap_ssid);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void reconnectMQTT() {
  if (millis() % 5000 < 100) {  // Try every 5 seconds
    Serial.print("Attempting MQTT connection...");
    
    preferences.begin("thermostat", true);
    String savedBroker = preferences.getString("mqtt_broker", mqtt_server);
    String savedUser = preferences.getString("mqtt_user", mqtt_user);
    String savedPass = preferences.getString("mqtt_pass", mqtt_password);
    preferences.end();
    
    mqtt.setServer(savedBroker.c_str(), mqtt_port);
    
    if (mqtt.connect(mqtt_client_id, savedUser.c_str(), savedPass.c_str())) {
      Serial.println("connected");
      addLog("MQTT connected");
      
      // Subscribe to command topics
      mqtt.subscribe(set_temp_topic.c_str());
      mqtt.subscribe(mode_set_topic.c_str());
      
      // Send Home Assistant discovery
      sendHADiscovery();
      
      // Publish initial status
      publishStatus();
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqtt.state());
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  if (String(topic) == set_temp_topic) {
    float newTarget = message.toFloat();
    if (newTarget >= 15.0 && newTarget <= 45.0) {
      targetTemp = newTarget;
      integral = 0;  // Reset integral on setpoint change
      preferences.begin("thermostat", false);
      preferences.putFloat("target", targetTemp);
      preferences.end();
      addLog("Target temp set to " + String(targetTemp, 1) + "°C (MQTT)");
      publishStatus();
    }
  }
  else if (String(topic) == mode_set_topic) {
    if (message == "auto" || message == "off" || message == "heat") {
      mode = (message == "heat") ? "on" : message;
      integral = 0;  // Reset integral on mode change
      preferences.begin("thermostat", false);
      preferences.putString("mode", mode);
      preferences.end();
      addLog("Mode changed to " + mode + " (MQTT)");
      updatePID();
      publishStatus();
    }
  }
}

void readTemperature() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  
  if (temp != DEVICE_DISCONNECTED_C && temp > -50 && temp < 100) {
    if (abs(temp - currentTemp) > 0.1) {  // Only update if changed significantly
      currentTemp = temp;
      displayNeedsUpdate = true;
    }
  } else {
    Serial.println("Error reading temperature!");
    static unsigned long lastTempError = 0;
    if (millis() - lastTempError > 60000) {  // Log error once per minute
      addLog("Temperature sensor error!");
      lastTempError = millis();
    }
  }
}

void updatePID() {
  if (mode == "auto") {
    float error = targetTemp - currentTemp;
    float dt = pidInterval / 1000.0;  // Convert to seconds
    
    // Proportional term
    float P = Kp * error;
    
    // Integral term with anti-windup
    integral += error * dt;
    integral = constrain(integral, -10, 10);  // Anti-windup
    float I = Ki * integral;
    
    // Derivative term
    float derivative = (error - previousError) / dt;
    float D = Kd * derivative;
    
    // Calculate output
    float output = P + I + D;
    
    // Constrain to 0-100%
    powerOutput = constrain((int)output, 0, 100);
    
    previousError = error;
    
    // Set dimmer power
    dimmer.setPower(powerOutput);
    heatingState = (powerOutput > 0);
    
    Serial.print("PID: Err=");
    Serial.print(error, 2);
    Serial.print(" P=");
    Serial.print(P, 2);
    Serial.print(" I=");
    Serial.print(I, 2);
    Serial.print(" D=");
    Serial.print(D, 2);
    Serial.print(" Out=");
    Serial.println(powerOutput);
    
  } else if (mode == "on") {
    powerOutput = 100;
    dimmer.setPower(100);
    heatingState = true;
  } else {  // mode == "off"
    powerOutput = 0;
    dimmer.setPower(0);
    heatingState = false;
    integral = 0;
    previousError = 0;
  }
}

void publishStatus() {
  if (mqtt.connected()) {
    mqtt.publish(temp_topic.c_str(), String(currentTemp, 1).c_str(), true);
    mqtt.publish(state_topic.c_str(), heatingState ? "heating" : "idle", true);
    mqtt.publish(mode_topic.c_str(), mode.c_str(), true);
    
    // Publish combined JSON status
    StaticJsonDocument<256> doc;
    doc["temperature"] = round(currentTemp * 10) / 10.0;
    doc["setpoint"] = targetTemp;
    doc["heating"] = heatingState;
    doc["mode"] = mode;
    doc["power"] = powerOutput;
    
    String output;
    serializeJson(doc, output);
    mqtt.publish((base_topic + "/status").c_str(), output.c_str(), true);
  }
}

void sendHADiscovery() {
  // Temperature sensor
  StaticJsonDocument<512> tempDoc;
  tempDoc["name"] = String(device_name) + " Temperature";
  tempDoc["state_topic"] = temp_topic;
  tempDoc["unit_of_measurement"] = "°C";
  tempDoc["device_class"] = "temperature";
  tempDoc["unique_id"] = String(device_id) + "_temp";
  
  JsonObject device = tempDoc.createNestedObject("device");
  device["identifiers"][0] = device_id;
  device["name"] = device_name;
  device["model"] = "ESP32 Thermostat";
  device["manufacturer"] = "DIY";
  
  String tempPayload;
  serializeJson(tempDoc, tempPayload);
  String tempDiscoveryTopic = String(ha_discovery_prefix) + "/sensor/" + device_id + "_temp/config";
  mqtt.publish(tempDiscoveryTopic.c_str(), tempPayload.c_str(), true);
  
  // Climate entity
  StaticJsonDocument<768> climateDoc;
  climateDoc["name"] = device_name;
  climateDoc["mode_state_topic"] = mode_topic;
  climateDoc["mode_command_topic"] = mode_set_topic;
  climateDoc["temperature_state_topic"] = temp_topic;
  climateDoc["temperature_command_topic"] = set_temp_topic;
  climateDoc["current_temperature_topic"] = temp_topic;
  climateDoc["temp_step"] = 0.5;
  climateDoc["min_temp"] = 15;
  climateDoc["max_temp"] = 45;
  climateDoc["unique_id"] = String(device_id) + "_climate";
  
  JsonArray modes = climateDoc.createNestedArray("modes");
  modes.add("off");
  modes.add("heat");
  
  JsonObject climateDevice = climateDoc.createNestedObject("device");
  climateDevice["identifiers"][0] = device_id;
  climateDevice["name"] = device_name;
  climateDevice["model"] = "ESP32 Thermostat";
  climateDevice["manufacturer"] = "DIY";
  
  String climatePayload;
  serializeJson(climateDoc, climatePayload);
  String climateDiscoveryTopic = String(ha_discovery_prefix) + "/climate/" + device_id + "/config";
  mqtt.publish(climateDiscoveryTopic.c_str(), climatePayload.c_str(), true);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/set", HTTP_POST, handleSet);
  server.on("/api/pid", HTTP_POST, handlePID);
  server.on("/settings", handleSettings);
  server.on("/api/save-settings", HTTP_POST, handleSaveSettings);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/update", handleUpdate);
  server.on("/api/upload", HTTP_POST, handleUploadDone, handleUpload);
  server.on("/api/check-update", handleCheckUpdate);
  server.on("/api/auto-update", HTTP_POST, handleAutoUpdate);
  server.on("/info", handleInfo);
  server.on("/logs", handleLogs);
  server.on("/schedule", handleSchedule);
  server.on("/api/save-schedule", HTTP_POST, handleSaveSchedule);
  server.begin();
  Serial.println("Web server started");
  addLog("Web server started");
}

void handleRoot() {
  String html = getHTMLHeader("Home", "home");
  
  if (apMode) {
    html += "<div class='warning-box'><strong>⚠️ AP Mode Active</strong><br>";
    html += "Configure WiFi in <a href='/settings'>Settings</a></div>";
  }
  
  // Add auto-refresh script
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
  
  html += "<h2>Current Status</h2>";
  html += "<div class='status' style='background:" + String(heatingState ? "#ffebee" : "#e8f5e9") + "'>";
  html += "<div><strong>Current:</strong> <span id='temp'>" + String(currentTemp, 1) + "°C</span></div>";
  html += "<div><strong>Target:</strong> <span id='target'>" + String(targetTemp, 1) + "°C</span></div>";
  html += "<div><strong>Heating:</strong> <span id='heating'>" + String(heatingState ? "ON" : "OFF") + "</span></div>";
  html += "<div><strong>Mode:</strong> <span id='mode'>" + mode + "</span></div>";
  html += "</div>";
  
  html += "<div style='margin:20px 0'><strong>Power Output: <span id='power-val'>" + String(powerOutput) + "%</span></strong>";
  html += "<div style='width:100%;height:30px;background:#ddd;border-radius:5px;overflow:hidden'>";
  html += "<div id='power-fill' style='height:100%;background:linear-gradient(90deg,#4CAF50,#ff9800);transition:width 0.3s;width:" + String(powerOutput) + "%'></div></div></div>";
  
  html += "<form action='/api/set' method='POST'>";
  html += "<h2>Temperature Control</h2>";
  html += "<div class='control'><label>Target Temperature (°C):</label>";
  html += "<input type='number' name='target' value='" + String(targetTemp, 1) + "' step='0.5' min='15' max='45'></div>";
  html += "<div class='control'><label>Mode:</label>";
  html += "<select name='mode'>";
  html += "<option value='auto'" + String(mode == "auto" ? " selected" : "") + ">Auto (PID)</option>";
  html += "<option value='on'" + String(mode == "on" ? " selected" : "") + ">Manual On (100%)</option>";
  html += "<option value='off'" + String(mode == "off" ? " selected" : "") + ">Off</option>";
  html += "</select></div>";
  html += "<button type='submit'>Update Settings</button></form>";
  
  html += getHTMLFooter();
  server.send(200, "text/html", html);
}

void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["temperature"] = round(currentTemp * 10) / 10.0;
  doc["setpoint"] = targetTemp;
  doc["heating"] = heatingState;
  doc["mode"] = mode;
  doc["power"] = powerOutput;
  
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleSet() {
  if (server.hasArg("target")) {
    float newTarget = server.arg("target").toFloat();
    if (newTarget >= 15.0 && newTarget <= 45.0) {
      targetTemp = newTarget;
      integral = 0;  // Reset integral
      preferences.begin("thermostat", false);
      preferences.putFloat("target", targetTemp);
      preferences.end();
      addLog("Target temp set to " + String(targetTemp, 1) + "°C (Web)");
    }
  }
  
  if (server.hasArg("mode")) {
    String newMode = server.arg("mode");
    if (newMode == "auto" || newMode == "off" || newMode == "on") {
      mode = newMode;
      integral = 0;  // Reset integral
      preferences.begin("thermostat", false);
      preferences.putString("mode", mode);
      preferences.end();
      addLog("Mode changed to " + mode + " (Web)");
    }
  }
  
  updatePID();
  publishStatus();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePID() {
  bool updated = false;
  
  if (server.hasArg("kp")) {
    Kp = server.arg("kp").toFloat();
    updated = true;
  }
  
  if (server.hasArg("ki")) {
    Ki = server.arg("ki").toFloat();
    updated = true;
  }
  
  if (server.hasArg("kd")) {
    Kd = server.arg("kd").toFloat();
    updated = true;
  }
  
  if (updated) {
    preferences.begin("thermostat", false);
    preferences.putFloat("Kp", Kp);
    preferences.putFloat("Ki", Ki);
    preferences.putFloat("Kd", Kd);
    preferences.end();
    
    // Reset PID state
    integral = 0;
    previousError = 0;
  }
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSettings() {
  preferences.begin("thermostat", true);
  String savedSSID = preferences.getString("wifi_ssid", ssid);
  String savedMQTTBroker = preferences.getString("mqtt_broker", mqtt_server);
  String savedMQTTUser = preferences.getString("mqtt_user", mqtt_user);
  String savedDeviceName = preferences.getString("device_name", "Thermostat");
  preferences.end();
  
  String html = getHTMLHeader("Settings", "settings");
  
  if (apMode) {
    html += "<div class='warning-box'><strong>⚠️ AP Mode Active</strong><br>";
    html += "Connect to WiFi network: <strong>" + String(ap_ssid) + "</strong><br>";
    html += "Password: <strong>" + String(ap_password) + "</strong></div>";
  } else {
    html += "<div class='info-box'><strong>Status:</strong> Connected to WiFi ✓<br>";
    html += "<strong>IP:</strong> " + WiFi.localIP().toString() + "</div>";
  }
  
  html += "<form action='/api/save-settings' method='POST'>";
  
  html += "<h2>Device Settings</h2>";
  html += "<div class='control'><label>Device Name:</label>";
  html += "<input type='text' name='device_name' value='" + savedDeviceName + "' maxlength='15'></div>";
  
  html += "<h2>WiFi Configuration</h2>";
  html += "<div class='control'><label>WiFi SSID:</label>";
  html += "<input type='text' name='wifi_ssid' value='" + savedSSID + "' required></div>";
  html += "<div class='control'><label>WiFi Password:</label>";
  html += "<input type='password' name='wifi_pass' placeholder='Enter new password or leave blank'></div>";
  
  html += "<h2>MQTT Configuration</h2>";
  html += "<div class='control'><label>MQTT Broker IP:</label>";
  html += "<input type='text' name='mqtt_broker' value='" + savedMQTTBroker + "' required></div>";
  html += "<div class='control'><label>MQTT Port:</label>";
  html += "<input type='number' name='mqtt_port' value='" + String(mqtt_port) + "' required></div>";
  html += "<div class='control'><label>MQTT Username:</label>";
  html += "<input type='text' name='mqtt_user' value='" + savedMQTTUser + "'></div>";
  html += "<div class='control'><label>MQTT Password:</label>";
  html += "<input type='password' name='mqtt_pass' placeholder='Enter new password or leave blank'></div>";
  
  html += "<h2>PID Tuning</h2>";
  html += "<div class='control'><label>Kp (Proportional):</label>";
  html += "<input type='number' name='kp' value='" + String(Kp, 2) + "' step='0.1' min='0'></div>";
  html += "<div class='control'><label>Ki (Integral):</label>";
  html += "<input type='number' name='ki' value='" + String(Ki, 2) + "' step='0.01' min='0'></div>";
  html += "<div class='control'><label>Kd (Derivative):</label>";
  html += "<input type='number' name='kd' value='" + String(Kd, 2) + "' step='0.1' min='0'></div>";
  
  html += "<button type='submit'>Save All Settings</button></form>";
  
  html += "<h2>Firmware</h2>";
  html += "<div class='info-box'><strong>Current Version:</strong> " + String(FIRMWARE_VERSION) + "</div>";
  
  html += "<div id='update-status' style='margin:10px 0'></div>";
  
  html += "<button type='button' class='btn-secondary' onclick='checkForUpdates()' id='check-btn'>Check for Updates</button>";
  html += "<a href='/update'><button type='button' class='btn-secondary'>Manual Upload</button></a>";
  
  html += "<script>";
  html += "function checkForUpdates(){";
  html += "document.getElementById('check-btn').disabled=true;";
  html += "document.getElementById('check-btn').innerText='Checking...';";
  html += "fetch('/api/check-update').then(r=>r.json()).then(d=>{";
  html += "let status=document.getElementById('update-status');";
  html += "if(d.update_available){";
  html += "status.innerHTML='<div class=\"warning-box\"><strong>⚡ Update Available!</strong><br>Latest: v'+d.latest_version+'<br>'+d.release_notes+'<br><button class=\"btn-secondary\" onclick=\"autoUpdate()\">Install Update</button></div>';";
  html += "}else{";
  html += "status.innerHTML='<div class=\"info-box\">✓ You are running the latest version</div>';";
  html += "}";
  html += "document.getElementById('check-btn').disabled=false;";
  html += "document.getElementById('check-btn').innerText='Check for Updates';";
  html += "}).catch(e=>{";
  html += "document.getElementById('update-status').innerHTML='<div class=\"warning-box\">❌ Could not check for updates. Check internet connection.</div>';";
  html += "document.getElementById('check-btn').disabled=false;";
  html += "document.getElementById('check-btn').innerText='Check for Updates';";
  html += "});";
  html += "}";
  html += "function autoUpdate(){";
  html += "if(!confirm('Download and install update? Device will restart.'))return;";
  html += "document.getElementById('update-status').innerHTML='<div class=\"info-box\">⏳ Downloading update... Do not power off!</div>';";
  html += "fetch('/api/auto-update',{method:'POST'}).then(r=>r.json()).then(d=>{";
  html += "if(d.success){";
  html += "document.getElementById('update-status').innerHTML='<div class=\"info-box\">✓ Update successful! Device restarting...</div>';";
  html += "setTimeout(()=>location.href='/',15000);";
  html += "}else{";
  html += "document.getElementById('update-status').innerHTML='<div class=\"warning-box\">❌ Update failed: '+d.error+'</div>';";
  html += "}";
  html += "}).catch(e=>{";
  html += "document.getElementById('update-status').innerHTML='<div class=\"warning-box\">❌ Update failed. Try manual upload.</div>';";
  html += "});";
  html += "}";
  html += "</script>";
  
  html += "<h2>System Actions</h2>";
  html += "<form action='/api/restart' method='POST' style='margin-top:20px'>";
  html += "<button type='submit' class='btn-danger' onclick='return confirm(\"Restart device?\")'>Restart Device</button></form>";
  
  html += getHTMLFooter();
  server.send(200, "text/html", html);
}

void handleSaveSettings() {
  preferences.begin("thermostat", false);
  
  // Save device name
  if (server.hasArg("device_name")) {
    preferences.putString("device_name", server.arg("device_name"));
  }
  
  // Save WiFi settings
  if (server.hasArg("wifi_ssid")) {
    preferences.putString("wifi_ssid", server.arg("wifi_ssid"));
  }
  if (server.hasArg("wifi_pass") && server.arg("wifi_pass").length() > 0) {
    preferences.putString("wifi_pass", server.arg("wifi_pass"));
  }
  
  // Save MQTT settings
  if (server.hasArg("mqtt_broker")) {
    preferences.putString("mqtt_broker", server.arg("mqtt_broker"));
  }
  if (server.hasArg("mqtt_port")) {
    // Note: Storing as float because Preferences doesn't have putInt for some reason
    preferences.putFloat("mqtt_port", server.arg("mqtt_port").toFloat());
  }
  if (server.hasArg("mqtt_user")) {
    preferences.putString("mqtt_user", server.arg("mqtt_user"));
  }
  if (server.hasArg("mqtt_pass") && server.arg("mqtt_pass").length() > 0) {
    preferences.putString("mqtt_pass", server.arg("mqtt_pass"));
  }
  
  // Save PID settings
  if (server.hasArg("kp")) {
    Kp = server.arg("kp").toFloat();
    preferences.putFloat("Kp", Kp);
  }
  if (server.hasArg("ki")) {
    Ki = server.arg("ki").toFloat();
    preferences.putFloat("Ki", Ki);
  }
  if (server.hasArg("kd")) {
    Kd = server.arg("kd").toFloat();
    preferences.putFloat("Kd", Kd);
  }
  
  preferences.end();
  
  // Reset PID
  integral = 0;
  previousError = 0;
  
  addLog("Settings saved, restarting...");
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='5;url=/'>";
  html += "<title>Settings Saved</title>";
  html += "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;text-align:center;padding-top:50px}";
  html += ".message{max-width:400px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  html += "h1{color:#4CAF50}</style></head><body>";
  html += "<div class='message'><h1>Settings Saved!</h1>";
  html += "<p>Device will restart in 5 seconds...</p>";
  html += "<p>If WiFi settings changed, connect to the new network.</p></div></body></html>";
  
  server.send(200, "text/html", html);
  
  delay(5000);
  ESP.restart();
}

void handleRestart() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='10;url=/'>";
  html += "<title>Restarting</title>";
  html += "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;text-align:center;padding-top:50px}";
  html += ".message{max-width:400px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}</style></head><body>";
  html += "<div class='message'><h1>Restarting...</h1>";
  html += "<p>Device will restart now. Page will reload in 10 seconds.</p></div></body></html>";
  
  server.send(200, "text/html", html);
  
  delay(1000);
  ESP.restart();
}

// ===== OTA UPDATE FUNCTIONS =====

void handleUpdate() {
  String html = getHTMLHeader("Firmware Update", "settings");
  
  preferences.begin("thermostat", true);
  String deviceName = preferences.getString("device_name", "Thermostat");
  preferences.end();
  
  html += "<div class='info-box'>";
  html += "<strong>Current Version:</strong> " + String(FIRMWARE_VERSION) + "<br>";
  html += "<strong>Device:</strong> " + deviceName + "</div>";
  
  html += "<div class='warning-box'><strong>⚠️ Warning:</strong><br>";
  html += "• Do not power off during update<br>";
  html += "• Update takes 30-60 seconds<br>";
  html += "• Device will restart automatically</div>";
  
  html += "<h2>Upload Firmware</h2>";
  html += "<form method='POST' action='/api/upload' enctype='multipart/form-data'>";
  html += "<input type='file' name='firmware' accept='.bin' required style='margin:20px 0'>";
  html += "<button type='submit'>Upload Firmware</button></form>";
  
  html += "<a href='/settings'><button type='button' class='btn-secondary'>Back to Settings</button></a>";
  
  html += getHTMLFooter();
  server.send(200, "text/html", html);
}

// ===== AUTO-UPDATE FROM GITHUB =====

void handleCheckUpdate() {
  if (!apMode && WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.github.com/repos/" + String(github_user) + "/" + String(github_repo) + "/releases/latest";
    
    http.begin(url);
    http.addHeader("Accept", "application/vnd.github.v3+json");
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      
      // Parse JSON manually (simple parsing for tag_name)
      int tagIndex = payload.indexOf("\"tag_name\"");
      if (tagIndex > 0) {
        int startQuote = payload.indexOf("\"", tagIndex + 12);
        int endQuote = payload.indexOf("\"", startQuote + 1);
        String latestVersion = payload.substring(startQuote + 1, endQuote);
        
        // Remove 'v' prefix if present
        if (latestVersion.startsWith("v")) {
          latestVersion = latestVersion.substring(1);
        }
        
        // Get release notes
        int bodyIndex = payload.indexOf("\"body\"");
        String releaseNotes = "";
        if (bodyIndex > 0) {
          int startBody = payload.indexOf("\"", bodyIndex + 8);
          int endBody = payload.indexOf("\"", startBody + 1);
          releaseNotes = payload.substring(startBody + 1, startBody + 100); // First 100 chars
          if (releaseNotes.length() > 97) releaseNotes = releaseNotes.substring(0, 97) + "...";
        }
        
        // Compare versions
        bool updateAvailable = (latestVersion != String(FIRMWARE_VERSION));
        
        String response = "{\"update_available\":" + String(updateAvailable ? "true" : "false");
        response += ",\"current_version\":\"" + String(FIRMWARE_VERSION) + "\"";
        response += ",\"latest_version\":\"" + latestVersion + "\"";
        response += ",\"release_notes\":\"" + releaseNotes + "\"";
        response += "}";
        
        server.send(200, "application/json", response);
        http.end();
        return;
      }
    }
    
    http.end();
  }
  
  // Error response
  server.send(500, "application/json", "{\"error\":\"Could not check for updates\"}");
}

void handleAutoUpdate() {
  if (apMode || WiFi.status() != WL_CONNECTED) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"No internet connection\"}");
    return;
  }
  
  addLog("Starting auto-update from GitHub");
  
  HTTPClient http;
  String url = "https://github.com/" + String(github_user) + "/" + String(github_repo) + "/releases/latest/download/" + String(github_firmware_asset);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200 || httpCode == 302) {
    // Follow redirect if needed
    if (httpCode == 302) {
      String redirectUrl = http.getLocation();
      http.end();
      http.begin(redirectUrl);
      httpCode = http.GET();
    }
    
    if (httpCode == 200) {
      int contentLength = http.getSize();
      
      if (contentLength > 0) {
        bool canBegin = Update.begin(contentLength);
        
        if (canBegin) {
          WiFiClient* stream = http.getStreamPtr();
          size_t written = Update.writeStream(*stream);
          
          if (written == contentLength) {
            if (Update.end()) {
              if (Update.isFinished()) {
                addLog("Update successful, restarting");
                server.send(200, "application/json", "{\"success\":true}");
                delay(1000);
                ESP.restart();
                return;
              }
            }
          }
        }
      }
    }
  }
  
  http.end();
  addLog("Update failed");
  server.send(500, "application/json", "{\"success\":false,\"error\":\"Download or install failed\"}");
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    
    // Stop all timers and tasks
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleUploadDone() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='15;url=/'>";
  html += "<title>Update Complete</title>";
  html += "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;text-align:center;padding-top:50px}";
  html += ".message{max-width:400px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  html += "h1{color:#4CAF50}</style></head><body>";
  html += "<div class='message'>";
  
  if (Update.hasError()) {
    html += "<h1 style='color:#f44336'>❌ Update Failed</h1>";
    html += "<p>Error occurred during update.</p>";
    html += "<p><a href='/update'>Try Again</a></p>";
  } else {
    html += "<h1>✅ Update Successful!</h1>";
    html += "<p>Device is restarting...</p>";
    html += "<p>New version: " + String(FIRMWARE_VERSION) + "</p>";
    html += "<p>Page will reload in 15 seconds.</p>";
  }
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
  
  if (!Update.hasError()) {
    delay(1000);
    ESP.restart();
  }
}

// ===== TFT DISPLAY FUNCTIONS =====

void initDisplay() {
  tft.init();
  tft.setRotation(1);  // Landscape orientation (0-3, try different values)
  tft.fillScreen(TFT_BLACK);
  
  // Show startup message
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(50, 110);
  tft.println("Reptile Thermostat");
  tft.setCursor(80, 140);
  tft.setTextSize(1);
  tft.println("Initializing...");
  
  delay(2000);
}

void drawMainScreen() {
  tft.fillScreen(TFT_BLACK);
  
  // Header bar
  tft.fillRect(0, 0, 320, 35, TFT_DARKGREEN);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  tft.setTextSize(2);
  tft.setCursor(5, 10);
  
  preferences.begin("thermostat", true);
  String deviceName = preferences.getString("device_name", "Thermostat");
  preferences.end();
  
  // Truncate device name if too long
  if (deviceName.length() > 13) {
    deviceName = deviceName.substring(0, 13);
  }
  tft.print(deviceName);
  
  // WiFi status icon (top right)
  tft.setTextSize(1);
  tft.setCursor(245, 8);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_GREEN, TFT_DARKGREEN);
    tft.print("WiFi");
  } else if (apMode) {
    tft.setTextColor(TFT_ORANGE, TFT_DARKGREEN);
    tft.print("AP");
  } else {
    tft.setTextColor(TFT_RED, TFT_DARKGREEN);
    tft.print("No WiFi");
  }
  
  // MQTT status (below WiFi)
  tft.setCursor(245, 20);
  if (mqtt.connected()) {
    tft.setTextColor(TFT_GREEN, TFT_DARKGREEN);
    tft.print("MQTT");
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_DARKGREEN);
    tft.print("----");
  }
  
  // Current Temperature Label
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.print("Current:");
  
  // Target Temperature Label  
  tft.setCursor(10, 120);
  tft.print("Target:");
  
  // Draw button boxes
  // MINUS button
  tft.fillRect(210, 115, 45, 45, TFT_DARKGREY);
  tft.drawRect(210, 115, 45, 45, TFT_WHITE);
  
  // PLUS button  
  tft.fillRect(265, 115, 45, 45, TFT_DARKGREY);
  tft.drawRect(265, 115, 45, 45, TFT_WHITE);
  
  // SIMPLE VIEW button (bottom left)
  tft.fillRect(10, 185, 70, 50, TFT_PURPLE);
  tft.drawRect(10, 185, 70, 50, TFT_WHITE);
  
  // SETTINGS button (bottom right)
  tft.fillRect(240, 185, 70, 50, TFT_NAVY);
  tft.drawRect(240, 185, 70, 50, TFT_WHITE);
  
  // Button labels
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(222, 123);
  tft.print("-");
  tft.setCursor(277, 123);
  tft.print("+");
  
  // Simple button label
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_PURPLE);
  tft.setCursor(17, 205);
  tft.print("SIMPLE");
  
  // Settings button label
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setCursor(248, 202);
  tft.print("SETUP");
  
  displayNeedsUpdate = true;
}

void updateDisplay() {
  if (!displayNeedsUpdate) return;
  
  // Current Temperature (large, clear area first)
  tft.fillRect(120, 45, 180, 40, TFT_BLACK);  // Clear larger area
  tft.setTextSize(4);
  tft.setCursor(120, 50);
  
  // Color based on heating state
  if (heatingState) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  } else {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
  }
  
  char tempStr[10];
  sprintf(tempStr, "%.1fC", currentTemp);
  tft.print(tempStr);
  
  // Target Temperature (moved left, away from buttons)
  tft.fillRect(120, 115, 80, 35, TFT_BLACK);  // Smaller clear area
  tft.setTextSize(3);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(120, 120);
  sprintf(tempStr, "%.1fC", targetTemp);
  tft.print(tempStr);
  
  // Status (between buttons, smaller area)
  tft.fillRect(90, 185, 140, 50, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(90, 195);
  
  if (mode == "off") {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.print("OFF");
  } else if (heatingState) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("HEAT");
    tft.setTextSize(1);
    tft.setCursor(90, 215);
    tft.print(powerOutput);
    tft.print("%");
  } else {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("IDLE");
  }
  
  displayNeedsUpdate = false;
}

void checkTouch() {
  uint16_t t_x = 0, t_y = 0;
  bool pressed = tft.getTouch(&t_x, &t_y);
  
  if (pressed) {
    Serial.print("Raw Touch: x=");
    Serial.print(t_x);
    Serial.print(", y=");
    Serial.println(t_y);
    
    int screen_x = map(t_y, 0, 320, 0, 320);
    int screen_y = map(t_x, 0, 240, 0, 240);
    
    Serial.print("Mapped Touch: x=");
    Serial.print(screen_x);
    Serial.print(", y=");
    Serial.println(screen_y);
    
    if (currentScreen == 0) {
      // MAIN SCREEN TOUCH HANDLING
      
      // Check if minus button pressed
      if (screen_x >= 100 && screen_x <= 150 && screen_y >= 100 && screen_y <= 140) {
        Serial.println("MINUS button detected!");
        targetTemp -= 0.5;
        if (targetTemp < 15.0) targetTemp = 15.0;
        
        preferences.begin("thermostat", false);
        preferences.putFloat("target", targetTemp);
        preferences.end();
        
        integral = 0;
        displayNeedsUpdate = true;
        publishStatus();
        
        delay(300);
      }
      
      // Check if plus button pressed
      else if (screen_x >= 100 && screen_x <= 150 && screen_y >= 40 && screen_y <= 80) {
        Serial.println("PLUS button detected!");
        targetTemp += 0.5;
        if (targetTemp > 45.0) targetTemp = 45.0;
        
        preferences.begin("thermostat", false);
        preferences.putFloat("target", targetTemp);
        preferences.end();
        
        integral = 0;
        displayNeedsUpdate = true;
        publishStatus();
        
        delay(300);
      }
      
      // Check if SETTINGS button pressed (bottom right: raw y around 194)
      else if (screen_x >= 185 && screen_x <= 220 && screen_y >= 40 && screen_y <= 90) {
        Serial.println("SETTINGS button detected!");
        currentScreen = 1;
        drawSettingsScreen();
        delay(300);
      }
      
      // Check if SIMPLE button pressed (bottom left: raw touch shows x=291, y=205)
      else if (screen_x >= 200 && screen_x <= 215 && screen_y >= 270 && screen_y <= 310) {
        Serial.println("SIMPLE button detected!");
        currentScreen = 2;
        drawSimpleScreen();
        delay(300);
      }
      
    } else if (currentScreen == 1) {
      // SETTINGS SCREEN TOUCH HANDLING
      
      // MODE buttons (y around 90-110 for all three)
      // AUTO button (x: 30-110)
      if (screen_x >= 30 && screen_x <= 110 && screen_y >= 90 && screen_y <= 130) {
        Serial.println("AUTO mode selected!");
        mode = "auto";
        preferences.begin("thermostat", false);
        preferences.putString("mode", mode);
        preferences.end();
        integral = 0;
        drawSettingsScreen();  // Redraw to show selection
        delay(300);
      }
      
      // MANUAL ON button (x: 120-200)
      else if (screen_x >= 120 && screen_x <= 200 && screen_y >= 90 && screen_y <= 130) {
        Serial.println("MANUAL ON mode selected!");
        mode = "on";
        preferences.begin("thermostat", false);
        preferences.putString("mode", mode);
        preferences.end();
        drawSettingsScreen();
        delay(300);
      }
      
      // OFF button (x: 210-290)
      else if (screen_x >= 210 && screen_x <= 290 && screen_y >= 90 && screen_y <= 130) {
        Serial.println("OFF mode selected!");
        mode = "off";
        preferences.begin("thermostat", false);
        preferences.putString("mode", mode);
        preferences.end();
        drawSettingsScreen();
        delay(300);
      }
      
      // BACK button (raw touch shows x=62, y=15 for settings back button)
      else if (screen_x >= 5 && screen_x <= 25 && screen_y >= 50 && screen_y <= 80) {
        Serial.println("BACK button detected!");
        currentScreen = 0;
        drawMainScreen();
        delay(300);
      }
    } else if (currentScreen == 2) {
      // SIMPLE SCREEN TOUCH HANDLING - tap anywhere to return
      Serial.println("Returning to main screen from simple view");
      currentScreen = 0;
      drawMainScreen();
      delay(300);
    }
  }
}

void drawSimpleScreen() {
  tft.fillScreen(TFT_BLACK);
  
  preferences.begin("thermostat", true);
  String deviceName = preferences.getString("device_name", "Thermostat");
  preferences.end();
  
  // Small header with device name
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print(deviceName);
  
  // Instruction
  tft.setCursor(160, 10);
  tft.print("Tap to exit");
  
  // Draw the temperature immediately (don't wait for update)
  tft.fillRect(0, 30, 320, 150, TFT_BLACK);
  
  // Color based on heating state
  if (heatingState) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  } else {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
  }
  
  // Format temperature - MUCH SIMPLER
  char tempStr[10];
  sprintf(tempStr, "%.1f", currentTemp);
  
  // Giant text
  tft.setTextSize(14);
  tft.setCursor(10, 50);
  tft.print(tempStr);
  
  // "C" right after, same line
  tft.setTextSize(10);
  tft.print("C");  // Just print it right after!
  
  // Target and status at bottom
  tft.setTextSize(3);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 190);
  tft.print("Target: ");
  tft.print(targetTemp, 1);
  tft.print("C");
  
  // Status indicator
  tft.setTextSize(2);
  tft.setCursor(10, 215);
  if (mode == "off") {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.print("OFF");
  } else if (heatingState) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("HEATING ");
    tft.print(powerOutput);
    tft.print("%");
  } else {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("IDLE");
  }
}

void updateSimpleDisplay() {
  static float lastDisplayedTemp = -999;
  static bool lastHeatingState = false;
  
  // Only update if something changed
  if (abs(currentTemp - lastDisplayedTemp) < 0.1 && heatingState == lastHeatingState) {
    return;
  }
  
  lastDisplayedTemp = currentTemp;
  lastHeatingState = heatingState;
  
  // Clear temperature area
  tft.fillRect(0, 30, 320, 150, TFT_BLACK);
  
  // Color based on heating state
  if (heatingState) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  } else {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
  }
  
  // Format temperature
  char tempStr[10];
  sprintf(tempStr, "%.1f", currentTemp);
  
  // Giant text
  tft.setTextSize(14);
  tft.setCursor(10, 50);
  tft.print(tempStr);
  
  // "C" right after on same line
  tft.setTextSize(10);
  tft.print("C");
  
  // Update status at bottom
  tft.fillRect(0, 180, 320, 60, TFT_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 190);
  tft.print("Target: ");
  tft.print(targetTemp, 1);
  tft.print("C");
  
  tft.setTextSize(2);
  tft.setCursor(10, 215);
  if (mode == "off") {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.print("OFF");
  } else if (heatingState) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("HEATING ");
    tft.print(powerOutput);
    tft.print("%");
  } else {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("IDLE");
  }
}

void drawSettingsScreen() {
  tft.fillScreen(TFT_BLACK);
  
  // Header
  tft.fillRect(0, 0, 320, 35, TFT_DARKGREEN);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  tft.setTextSize(2);
  tft.setCursor(5, 10);
  tft.print("Settings");
  
  // Back button (top right)
  tft.fillRect(250, 5, 60, 25, TFT_NAVY);
  tft.drawRect(250, 5, 60, 25, TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(265, 13);
  tft.print("BACK");
  
  // Device Name section
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.print("Device Name:");
  
  preferences.begin("thermostat", true);
  String deviceName = preferences.getString("device_name", "Thermostat");
  preferences.end();
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 75);
  tft.print(deviceName);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(10, 95);
  tft.print("(Change via web interface)");
  
  // Mode Selection
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 120);
  tft.print("Mode:");
  
  // Mode buttons - highlight current mode
  int yPos = 145;
  
  // AUTO button
  if (mode == "auto") {
    tft.fillRect(10, yPos, 90, 40, TFT_DARKGREEN);
    tft.drawRect(10, yPos, 90, 40, TFT_GREEN);
  } else {
    tft.fillRect(10, yPos, 90, 40, TFT_DARKGREY);
    tft.drawRect(10, yPos, 90, 40, TFT_WHITE);
  }
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, mode == "auto" ? TFT_DARKGREEN : TFT_DARKGREY);
  tft.setCursor(25, 157);
  tft.print("AUTO");
  
  // MANUAL ON button
  if (mode == "on") {
    tft.fillRect(110, yPos, 90, 40, TFT_MAROON);
    tft.drawRect(110, yPos, 90, 40, TFT_RED);
  } else {
    tft.fillRect(110, yPos, 90, 40, TFT_DARKGREY);
    tft.drawRect(110, yPos, 90, 40, TFT_WHITE);
  }
  tft.setTextColor(TFT_WHITE, mode == "on" ? TFT_MAROON : TFT_DARKGREY);
  tft.setCursor(125, 157);
  tft.print("ON");
  
  // OFF button
  if (mode == "off") {
    tft.fillRect(210, yPos, 90, 40, TFT_NAVY);
    tft.drawRect(210, yPos, 90, 40, TFT_BLUE);
  } else {
    tft.fillRect(210, yPos, 90, 40, TFT_DARKGREY);
    tft.drawRect(210, yPos, 90, 40, TFT_WHITE);
  }
  tft.setTextColor(TFT_WHITE, mode == "off" ? TFT_NAVY : TFT_DARKGREY);
  tft.setCursor(230, 157);
  tft.print("OFF");
  
  // Info section
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 200);
  tft.print("IP: ");
  if (WiFi.status() == WL_CONNECTED) {
    tft.print(WiFi.localIP().toString());
  } else if (apMode) {
    tft.print(WiFi.softAPIP().toString());
  } else {
    tft.print("Not connected");
  }
  
  tft.setCursor(10, 212);
  tft.print("MQTT: ");
  if (mqtt.connected()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("Connected");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("Disconnected");
  }
  
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(10, 224);
  tft.print("FW: ");
  tft.print(FIRMWARE_VERSION);
}

// ===== SCHEDULE SYSTEM =====

void loadSchedule() {
  preferences.begin("thermostat", true);
  scheduleEnabled = preferences.getBool("sched_enabled", false);
  scheduleSlotCount = preferences.getInt("sched_count", 0);
  
  for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
    String prefix = "s" + String(i) + "_";
    schedule[i].enabled = preferences.getBool((prefix + "en").c_str(), false);
    schedule[i].hour = preferences.getInt((prefix + "h").c_str(), 0);
    schedule[i].minute = preferences.getInt((prefix + "m").c_str(), 0);
    schedule[i].targetTemp = preferences.getFloat((prefix + "t").c_str(), 28.0);
    schedule[i].days = preferences.getString((prefix + "d").c_str(), "SMTWTFS");
  }
  preferences.end();
  
  if (scheduleSlotCount == 0) {
    // Initialize default schedule (2 slots)
    scheduleSlotCount = 2;
    schedule[0] = {true, 7, 0, 28.0, "SMTWTFS"};  // 7:00 AM - 28°C
    schedule[1] = {true, 22, 0, 24.0, "SMTWTFS"}; // 10:00 PM - 24°C
  }
}

void saveSchedule() {
  preferences.begin("thermostat", false);
  preferences.putBool("sched_enabled", scheduleEnabled);
  preferences.putInt("sched_count", scheduleSlotCount);
  
  for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
    String prefix = "s" + String(i) + "_";
    preferences.putBool((prefix + "en").c_str(), schedule[i].enabled);
    preferences.putInt((prefix + "h").c_str(), schedule[i].hour);
    preferences.putInt((prefix + "m").c_str(), schedule[i].minute);
    preferences.putFloat((prefix + "t").c_str(), schedule[i].targetTemp);
    preferences.putString((prefix + "d").c_str(), schedule[i].days);
  }
  preferences.end();
  
  addLog("Schedule saved");
}

void checkSchedule() {
  if (!scheduleEnabled) return;
  
  // Get current time
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  int currentHour = timeinfo->tm_hour;
  int currentMinute = timeinfo->tm_min;
  int currentDay = timeinfo->tm_wday; // 0=Sunday
  
  String dayChar = "SMTWTFS";
  char todayChar = dayChar[currentDay];
  
  // Check each schedule slot
  for (int i = 0; i < scheduleSlotCount; i++) {
    if (!schedule[i].enabled) continue;
    
    // Check if today is in the schedule
    if (schedule[i].days.indexOf(todayChar) < 0) continue;
    
    // Check if this is the scheduled time
    if (schedule[i].hour == currentHour && schedule[i].minute == currentMinute) {
      if (targetTemp != schedule[i].targetTemp) {
        targetTemp = schedule[i].targetTemp;
        integral = 0;
        preferences.begin("thermostat", false);
        preferences.putFloat("target", targetTemp);
        preferences.end();
        
        addLog("Schedule: Temp set to " + String(targetTemp, 1) + "°C");
        publishStatus();
        displayNeedsUpdate = true;
      }
    }
  }
  
  // Calculate next schedule change
  int nextHour = 99;
  int nextMinute = 99;
  float nextTemp = 0;
  
  for (int i = 0; i < scheduleSlotCount; i++) {
    if (!schedule[i].enabled) continue;
    if (schedule[i].days.indexOf(todayChar) < 0) continue;
    
    int schedMinutes = schedule[i].hour * 60 + schedule[i].minute;
    int currentMinutes = currentHour * 60 + currentMinute;
    
    if (schedMinutes > currentMinutes) {
      int testMinutes = schedule[i].hour * 60 + schedule[i].minute;
      if (testMinutes < (nextHour * 60 + nextMinute)) {
        nextHour = schedule[i].hour;
        nextMinute = schedule[i].minute;
        nextTemp = schedule[i].targetTemp;
      }
    }
  }
  
  if (nextHour < 99) {
    char buffer[20];
    sprintf(buffer, "%02d:%02d", nextHour, nextMinute);
    nextScheduleTime = String(buffer);
    nextScheduleTemp = nextTemp;
  } else {
    nextScheduleTime = "";
  }
}

void handleSchedule() {
  String html = getHTMLHeader("Schedule", "schedule");
  
  // Schedule enable/disable
  html += "<div style='margin:20px 0;display:flex;justify-content:space-between;align-items:center'>";
  html += "<h2 style='margin:0'>Temperature Schedule</h2>";
  html += "<label style='display:flex;align-items:center;gap:10px'>";
  html += "<span>Schedule " + String(scheduleEnabled ? "Enabled" : "Disabled") + "</span>";
  html += "<input type='checkbox' id='schedule-enabled' " + String(scheduleEnabled ? "checked" : "") + " ";
  html += "onchange='toggleSchedule()' style='width:auto;height:20px'>";
  html += "</label></div>";
  
  if (scheduleEnabled && nextScheduleTime.length() > 0) {
    html += "<div class='info-box'>📅 Next: " + String(nextScheduleTemp, 1) + "°C at " + nextScheduleTime + "</div>";
  }
  
  html += "<form id='schedule-form'>";
  
  // Schedule slots
  for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
    if (i >= scheduleSlotCount && i > 3) continue; // Show empty slots up to 4, then only if used
    
    bool isActive = (i < scheduleSlotCount) && schedule[i].enabled;
    
    html += "<div class='schedule-slot' style='border:2px solid " + String(isActive ? "#4CAF50" : "#ddd") + ";";
    html += "padding:15px;border-radius:10px;margin:15px 0;background:" + String(isActive ? "#f1f8f4" : "#f9f9f9") + "'>";
    
    html += "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:10px'>";
    html += "<strong>Slot " + String(i + 1) + "</strong>";
    html += "<label style='display:flex;align-items:center;gap:5px'>";
    html += "<input type='checkbox' name='enabled" + String(i) + "' " + String(isActive ? "checked" : "") + " style='width:auto'>";
    html += "<span>Active</span></label></div>";
    
    // Time
    html += "<div style='display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:10px 0'>";
    html += "<div><label>Hour</label><input type='number' name='hour" + String(i) + "' value='" + String(schedule[i].hour) + "' min='0' max='23'></div>";
    html += "<div><label>Minute</label><input type='number' name='minute" + String(i) + "' value='" + String(schedule[i].minute) + "' min='0' max='59'></div>";
    html += "<div><label>Temp (°C)</label><input type='number' name='temp" + String(i) + "' value='" + String(schedule[i].targetTemp, 1) + "' step='0.5' min='15' max='45'></div>";
    html += "</div>";
    
    // Days
    html += "<div><label>Active Days:</label>";
    html += "<div style='display:flex;gap:5px;margin-top:5px'>";
    String days = "SMTWTFS";
    String dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (int d = 0; d < 7; d++) {
      bool checked = schedule[i].days.indexOf(days[d]) >= 0;
      html += "<label style='flex:1;text-align:center;padding:8px;background:" + String(checked ? "#4CAF50" : "#ddd") + ";";
      html += "color:" + String(checked ? "white" : "#666") + ";border-radius:5px;cursor:pointer'>";
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
  if (scheduleSlotCount < MAX_SCHEDULE_SLOTS) {
    html += "<button type='button' onclick='addSlot()' class='btn-secondary'>➕ Add Slot</button>";
  } else {
    html += "<div></div>";
  }
  if (scheduleSlotCount > 1) {
    html += "<button type='button' onclick='removeSlot()' class='btn-danger'>➖ Remove Last</button>";
  }
  html += "</div>";
  
  html += "<button type='button' onclick='saveSchedule()'>Save Schedule</button>";
  html += "</form>";
  
  html += "<script>";
  html += "let slotCount=" + String(scheduleSlotCount) + ";";
  html += "function toggleSchedule(){";
  html += "fetch('/api/save-schedule',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},";
  html += "body:'schedule_enabled='+document.getElementById('schedule-enabled').checked}).then(()=>location.reload());}";
  html += "function addSlot(){if(slotCount<" + String(MAX_SCHEDULE_SLOTS) + "){slotCount++;location.reload();}}";
  html += "function removeSlot(){if(slotCount>1){slotCount--;location.reload();}}";
  html += "function saveSchedule(){";
  html += "let form=document.getElementById('schedule-form');";
  html += "let data='slot_count='+slotCount;";
  html += "for(let i=0;i<" + String(MAX_SCHEDULE_SLOTS) + ";i++){";
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
  
  html += getHTMLFooter();
  server.send(200, "text/html", html);
}

void handleSaveSchedule() {
  // Toggle schedule enabled
  if (server.hasArg("schedule_enabled")) {
    scheduleEnabled = (server.arg("schedule_enabled") == "true");
    preferences.begin("thermostat", false);
    preferences.putBool("sched_enabled", scheduleEnabled);
    preferences.end();
    server.send(200, "text/plain", "OK");
    return;
  }
  
  // Save full schedule
  if (server.hasArg("slot_count")) {
    scheduleSlotCount = server.arg("slot_count").toInt();
    
    for (int i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
      schedule[i].enabled = server.hasArg("enabled" + String(i)) && (server.arg("enabled" + String(i)) == "true");
      schedule[i].hour = server.arg("hour" + String(i)).toInt();
      schedule[i].minute = server.arg("minute" + String(i)).toInt();
      schedule[i].targetTemp = server.arg("temp" + String(i)).toFloat();
      schedule[i].days = server.arg("days" + String(i));
      if (schedule[i].days.length() == 0) schedule[i].days = "SMTWTFS"; // Default to all days
    }
    
    saveSchedule();
  }
  
  server.send(200, "text/plain", "OK");
}

// ===== LOGGING SYSTEM =====

void addLog(String message) {
  // Get timestamp
  unsigned long uptime = millis() / 1000; // seconds
  unsigned long hours = uptime / 3600;
  unsigned long minutes = (uptime % 3600) / 60;
  unsigned long seconds = uptime % 60;
  
  char timestamp[20];
  sprintf(timestamp, "[%02lu:%02lu:%02lu]", hours, minutes, seconds);
  
  // Add to circular buffer
  logMessages[logIndex] = String(timestamp) + " " + message;
  logIndex = (logIndex + 1) % MAX_LOGS;
  
  Serial.println(String(timestamp) + " " + message);
}

// ===== COMMON HTML HEADER =====

String getHTMLHeader(String title, String activePage) {
  preferences.begin("thermostat", true);
  String deviceName = preferences.getString("device_name", "Thermostat");
  preferences.end();
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>" + title + " - " + deviceName + "</title>";
  html += "<style>";
  html += "body{font-family:Arial;margin:0;padding:20px;background:#f0f0f0}";
  html += "@media (max-width:640px){body{padding:10px}}";
  html += ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  html += "@media (max-width:640px){.container{padding:15px}}";
  html += ".header{background:linear-gradient(135deg,#4CAF50,#45a049);color:white;padding:20px;border-radius:10px 10px 0 0;margin:-20px -20px 20px -20px;text-align:center}";
  html += ".header h1{margin:0;font-size:24px}";
  html += ".header .subtitle{margin:5px 0 0 0;font-size:12px;opacity:0.9}";
  html += ".nav{display:flex;flex-wrap:wrap;justify-content:center;gap:10px;margin-bottom:20px}";
  html += ".nav a{flex:1;min-width:100px;padding:12px 20px;background:#2196F3;color:white;text-decoration:none;border-radius:5px;text-align:center;transition:background 0.3s}";
  html += ".nav a:hover{background:#0b7dda}";
  html += ".nav a.active{background:#4CAF50}";
  html += "@media (max-width:640px){.nav a{min-width:80px;padding:10px 15px;font-size:14px}}";
  html += "h2{color:#666;border-bottom:2px solid #4CAF50;padding-bottom:5px;margin-top:30px}";
  html += ".status{display:flex;justify-content:space-between;margin:20px 0;padding:15px;background:#e8f5e9;border-radius:5px}";
  html += "@media (max-width:640px){.status{flex-direction:column;gap:10px}}";
  html += ".control{margin:20px 0}";
  html += "label{display:block;margin:10px 0 5px;font-weight:bold}";
  html += "input,select{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box}";
  html += "button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin-top:10px}";
  html += "button:hover{background:#45a049}";
  html += ".btn-secondary{background:#2196F3}";
  html += ".btn-secondary:hover{background:#0b7dda}";
  html += ".btn-danger{background:#f44336}";
  html += ".btn-danger:hover{background:#da190b}";
  html += ".info-box{background:#e3f2fd;padding:15px;border-radius:5px;margin:10px 0;border-left:4px solid #2196F3}";
  html += ".warning-box{background:#fff3cd;padding:15px;border-radius:5px;margin:10px 0;border-left:4px solid #ffc107}";
  html += ".stat-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin:20px 0}";
  html += ".stat-card{background:#f5f5f5;padding:15px;border-radius:5px;text-align:center}";
  html += ".stat-value{font-size:24px;font-weight:bold;color:#4CAF50}";
  html += ".stat-label{font-size:12px;color:#666;margin-top:5px}";
  html += ".log-entry{padding:10px;border-bottom:1px solid #eee;font-family:monospace;font-size:14px}";
  html += ".log-entry:last-child{border-bottom:none}";
  html += ".footer{text-align:center;margin-top:30px;padding-top:20px;border-top:1px solid #ddd;color:#666;font-size:12px}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<div class='header'><h1>" + deviceName + "</h1>";
  html += "<div class='subtitle'>ESP32 Reptile Thermostat v" + String(FIRMWARE_VERSION) + "</div></div>";
  html += "<div class='nav'>";
  html += "<a href='/' class='" + String(activePage == "home" ? "active" : "") + "'>🏠 Home</a>";
  html += "<a href='/schedule' class='" + String(activePage == "schedule" ? "active" : "") + "'>📅 Schedule</a>";
  html += "<a href='/info' class='" + String(activePage == "info" ? "active" : "") + "'>ℹ️ Info</a>";
  html += "<a href='/logs' class='" + String(activePage == "logs" ? "active" : "") + "'>📋 Logs</a>";
  html += "<a href='/settings' class='" + String(activePage == "settings" ? "active" : "") + "'>⚙️ Settings</a>";
  html += "</div>";
  
  return html;
}

String getHTMLFooter() {
  String html = "<div class='footer'>";
  html += "ESP32 Reptile Thermostat v" + String(FIRMWARE_VERSION);
  html += " | Uptime: ";
  
  unsigned long uptime = millis() / 1000;
  unsigned long days = uptime / 86400;
  unsigned long hours = (uptime % 86400) / 3600;
  unsigned long minutes = (uptime % 3600) / 60;
  
  if (days > 0) html += String(days) + "d ";
  html += String(hours) + "h " + String(minutes) + "m";
  html += "</div></div></body></html>";
  
  return html;
}

// ===== INFO PAGE =====

void handleInfo() {
  String html = getHTMLHeader("Device Info", "info");
  
  if (apMode) {
    html += "<div class='warning-box'><strong>⚠️ AP Mode Active</strong><br>";
    html += "Connect to WiFi network in <a href='/settings'>Settings</a></div>";
  }
  
  html += "<h2>Device Information</h2>";
  
  preferences.begin("thermostat", true);
  String deviceName = preferences.getString("device_name", "Thermostat");
  preferences.end();
  
  html += "<div class='stat-grid'>";
  html += "<div class='stat-card'><div class='stat-value'>" + deviceName + "</div><div class='stat-label'>Device Name</div></div>";
  html += "<div class='stat-card'><div class='stat-value'>" + String(FIRMWARE_VERSION) + "</div><div class='stat-label'>Firmware</div></div>";
  
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
  if (WiFi.status() == WL_CONNECTED) {
    html += "<strong>WiFi:</strong> Connected ✓<br>";
    html += "<strong>SSID:</strong> " + WiFi.SSID() + "<br>";
    html += "<strong>IP Address:</strong> " + WiFi.localIP().toString() + "<br>";
    html += "<strong>Signal Strength:</strong> " + String(WiFi.RSSI()) + " dBm<br>";
    html += "<strong>MAC Address:</strong> " + WiFi.macAddress() + "<br>";
    
    // mDNS hostname
    preferences.begin("thermostat", true);
    String hostname = preferences.getString("device_name", "Thermostat");
    preferences.end();
    hostname.replace(" ", "-");
    hostname.toLowerCase();
    html += "<strong>mDNS:</strong> " + hostname + ".local";
  } else if (apMode) {
    html += "<strong>Mode:</strong> Access Point<br>";
    html += "<strong>SSID:</strong> " + String(ap_ssid) + "<br>";
    html += "<strong>IP Address:</strong> " + WiFi.softAPIP().toString();
  } else {
    html += "<strong>WiFi:</strong> Not Connected ❌";
  }
  html += "</div>";
  
  html += "<h2>MQTT Status</h2>";
  html += "<div class='info-box'>";
  if (mqtt.connected()) {
    html += "<strong>Status:</strong> Connected ✓<br>";
    
    preferences.begin("thermostat", true);
    String broker = preferences.getString("mqtt_broker", mqtt_server);
    preferences.end();
    
    html += "<strong>Broker:</strong> " + broker + ":" + String(mqtt_port) + "<br>";
    html += "<strong>Client ID:</strong> " + String(mqtt_client_id);
  } else {
    html += "<strong>Status:</strong> Disconnected ❌<br>";
    html += "MQTT may be disabled or broker unreachable";
  }
  html += "</div>";
  
  html += "<h2>Sensor Information</h2>";
  html += "<div class='stat-grid'>";
  html += "<div class='stat-card'><div class='stat-value'>" + String(currentTemp, 1) + "°C</div><div class='stat-label'>Current Temp</div></div>";
  html += "<div class='stat-card'><div class='stat-value'>" + String(targetTemp, 1) + "°C</div><div class='stat-label'>Target Temp</div></div>";
  html += "<div class='stat-card'><div class='stat-value'>" + String(powerOutput) + "%</div><div class='stat-label'>Power Output</div></div>";
  html += "<div class='stat-card'><div class='stat-value'>" + mode + "</div><div class='stat-label'>Mode</div></div>";
  html += "</div>";
  
  html += "<h2>PID Parameters</h2>";
  html += "<div class='info-box'>";
  html += "<strong>Kp:</strong> " + String(Kp, 2) + " | ";
  html += "<strong>Ki:</strong> " + String(Ki, 2) + " | ";
  html += "<strong>Kd:</strong> " + String(Kd, 2);
  html += "</div>";
  
  html += getHTMLFooter();
  server.send(200, "text/html", html);
}

// ===== LOGS PAGE =====

void handleLogs() {
  String html = getHTMLHeader("System Logs", "logs");
  
  html += "<h2>Recent Events</h2>";
  html += "<div class='info-box'>";
  html += "Showing last " + String(MAX_LOGS) + " log entries (newest first)";
  html += "</div>";
  
  html += "<div style='background:#f9f9f9;border-radius:5px;padding:10px;max-height:500px;overflow-y:auto'>";
  
  bool hasLogs = false;
  // Display logs in reverse order (newest first)
  for (int i = 0; i < MAX_LOGS; i++) {
    int idx = (logIndex - 1 - i + MAX_LOGS) % MAX_LOGS;
    if (logMessages[idx].length() > 0) {
      html += "<div class='log-entry'>" + logMessages[idx] + "</div>";
      hasLogs = true;
    }
  }
  
  if (!hasLogs) {
    html += "<div class='log-entry'>No logs yet...</div>";
  }
  
  html += "</div>";
  
  html += "<div style='margin-top:20px'>";
  html += "<button onclick='location.reload()' class='btn-secondary'>Refresh Logs</button>";
  html += "</div>";
  
  html += getHTMLFooter();
  server.send(200, "text/html", html);
}