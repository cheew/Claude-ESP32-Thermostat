/**
 * mqtt_manager.c
 * MQTT Client and Home Assistant Auto-Discovery Implementation
 */

#include "mqtt_manager.h"
#include "console.h"
#include <Arduino.h>
#include <ArduinoJson.h>

// Default MQTT configuration
static const char* DEFAULT_MQTT_SERVER = "192.168.1.123";
static const int DEFAULT_MQTT_PORT = 1883;
static const char* DEFAULT_MQTT_USER = "admin";
static const char* DEFAULT_MQTT_PASSWORD = "Oasis0asis!!";
static const char* MQTT_CLIENT_ID = "esp32_thermostat";

// Home Assistant discovery
static const char* HA_DISCOVERY_PREFIX = "homeassistant";

// State variables
static WiFiClient espClient;
static PubSubClient mqttClient(espClient);
static MQTTState_t currentState = MQTT_STATE_DISCONNECTED;
static unsigned long lastConnectionAttempt = 0;
static const unsigned long CONNECTION_RETRY_INTERVAL = 5000; // 5 seconds

// Topic storage
static char baseTopic[64] = "reptile/thermostat_01";
static char tempTopic[80];
static char stateTopic[80];
static char modeTopic[80];
static char setTempTopic[80];
static char modeSetTopic[80];
static char statusTopic[80];

// Device info for HA discovery
static char deviceName[32] = "Reptile Thermostat";
static char deviceId[32] = "reptile_thermostat_01";

// Message callbacks
static MQTTMessageCallback_t setpointCallback = NULL;
static MQTTMessageCallback_t modeCallback = NULL;

// Forward declarations
static void buildTopics(void);
static void mqttCallback(char* topic, byte* payload, unsigned int length);

/**
 * Initialize MQTT manager
 */
void mqtt_init(void) {
    Serial.println("[MQTT] Initializing MQTT manager");
    
    // Load configuration from preferences
    Preferences prefs;
    prefs.begin("thermostat", true);
    String server = prefs.getString("mqtt_broker", DEFAULT_MQTT_SERVER);
    int port = (int)prefs.getFloat("mqtt_port", DEFAULT_MQTT_PORT);
    prefs.end();
    
    // Setup MQTT client
    mqttClient.setServer(server.c_str(), port);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
    
    // Build topic strings
    buildTopics();
    
    Serial.print("[MQTT] Configured for broker: ");
    Serial.print(server);
    Serial.print(":");
    Serial.println(port);
}

/**
 * MQTT task - handles reconnection and message loop
 */
void mqtt_task(void) {
    // Process incoming messages if connected
    if (mqttClient.connected()) {
        mqttClient.loop();
        
        if (currentState != MQTT_STATE_CONNECTED) {
            currentState = MQTT_STATE_CONNECTED;
        }
    } else {
        if (currentState != MQTT_STATE_DISCONNECTED) {
            Serial.println("[MQTT] Connection lost");
            currentState = MQTT_STATE_DISCONNECTED;
        }
        
        // Attempt reconnection at interval
        if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
            mqtt_connect();
        }
    }
}

/**
 * Connect to MQTT broker
 */
bool mqtt_connect(void) {
    lastConnectionAttempt = millis();
    
    if (mqttClient.connected()) {
        return true;
    }
    
    Serial.print("[MQTT] Attempting connection...");
    currentState = MQTT_STATE_CONNECTING;
    
    // Load credentials
    Preferences prefs;
    prefs.begin("thermostat", true);
    String server = prefs.getString("mqtt_broker", DEFAULT_MQTT_SERVER);
    String user = prefs.getString("mqtt_user", DEFAULT_MQTT_USER);
    String password = prefs.getString("mqtt_pass", DEFAULT_MQTT_PASSWORD);
    int port = (int)prefs.getFloat("mqtt_port", DEFAULT_MQTT_PORT);
    prefs.end();
    
    // Update server if changed
    mqttClient.setServer(server.c_str(), port);
    
    // Attempt connection
    if (mqttClient.connect(MQTT_CLIENT_ID, user.c_str(), password.c_str())) {
        Serial.println(" connected");
        currentState = MQTT_STATE_CONNECTED;
        
        // Subscribe to command topics
        mqttClient.subscribe(setTempTopic);
        mqttClient.subscribe(modeSetTopic);
        
        Serial.println("[MQTT] Subscribed to command topics");
        
        return true;
    } else {
        Serial.print(" failed, rc=");
        Serial.println(mqttClient.state());
        currentState = MQTT_STATE_DISCONNECTED;
        return false;
    }
}

/**
 * Disconnect from MQTT
 */
void mqtt_disconnect(void) {
    if (mqttClient.connected()) {
        mqttClient.disconnect();
        Serial.println("[MQTT] Disconnected");
    }
    currentState = MQTT_STATE_DISCONNECTED;
}

/**
 * Check if connected
 */
bool mqtt_is_connected(void) {
    return mqttClient.connected();
}

/**
 * Get current state
 */
MQTTState_t mqtt_get_state(void) {
    return currentState;
}

/**
 * Publish temperature
 */
void mqtt_publish_temperature(float temperature) {
    if (!mqttClient.connected()) return;

    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%.1f", temperature);
    mqttClient.publish(tempTopic, tempStr, true);

    console_add_event_f(CONSOLE_EVENT_MQTT, "MQTT PUB: %s = %s", tempTopic, tempStr);
}

/**
 * Publish heating state
 */
void mqtt_publish_state(bool heating) {
    if (!mqttClient.connected()) return;
    
    mqttClient.publish(stateTopic, heating ? "heating" : "idle", true);
}

/**
 * Publish mode
 */
void mqtt_publish_mode(const char* mode) {
    if (!mqttClient.connected()) return;
    
    mqttClient.publish(modeTopic, mode, true);
}

/**
 * Publish complete status
 */
void mqtt_publish_status(float temperature, float setpoint, 
                         bool heating, const char* mode, int power) {
    if (!mqttClient.connected()) return;
    
    // Publish individual topics
    mqtt_publish_temperature(temperature);
    mqtt_publish_state(heating);
    mqtt_publish_mode(mode);
    
    // Publish combined JSON status
    StaticJsonDocument<256> doc;
    doc["temperature"] = round(temperature * 10) / 10.0;
    doc["setpoint"] = setpoint;
    doc["heating"] = heating;
    doc["mode"] = mode;
    doc["power"] = power;
    
    char output[256];
    serializeJson(doc, output);
    mqttClient.publish(statusTopic, output, true);
}

/**
 * Publish extended status with system info
 */
void mqtt_publish_status_extended(float temperature, float setpoint,
                                   bool heating, const char* mode, int power,
                                   int wifiRssi, uint32_t freeHeap, unsigned long uptimeSeconds) {
    if (!mqttClient.connected()) return;

    // Publish individual topics
    mqtt_publish_temperature(temperature);
    mqtt_publish_state(heating);
    mqtt_publish_mode(mode);

    // Publish extended JSON status with system info
    StaticJsonDocument<512> doc;

    // Core thermostat data
    doc["temperature"] = round(temperature * 10) / 10.0;
    doc["setpoint"] = setpoint;
    doc["heating"] = heating;
    doc["mode"] = mode;
    doc["power"] = power;

    // System information
    doc["wifi_rssi"] = wifiRssi;
    doc["free_heap"] = freeHeap;
    doc["uptime"] = uptimeSeconds;

    // Calculated uptime breakdown
    JsonObject uptime = doc.createNestedObject("uptime_breakdown");
    uptime["days"] = uptimeSeconds / 86400;
    uptime["hours"] = (uptimeSeconds % 86400) / 3600;
    uptime["minutes"] = (uptimeSeconds % 3600) / 60;
    uptime["seconds"] = uptimeSeconds % 60;

    char output[512];
    serializeJson(doc, output);
    mqttClient.publish(statusTopic, output, true);
}

/**
 * Send Home Assistant auto-discovery
 */
void mqtt_send_ha_discovery(const char* devName, const char* devId) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] Cannot send HA discovery: not connected");
        return;
    }
    
    // Store device info
    strncpy(deviceName, devName, sizeof(deviceName) - 1);
    strncpy(deviceId, devId, sizeof(deviceId) - 1);
    
    Serial.println("[MQTT] Sending Home Assistant discovery...");
    
    // Temperature sensor
    StaticJsonDocument<512> tempDoc;
    tempDoc["name"] = String(deviceName) + " Temperature";
    tempDoc["state_topic"] = tempTopic;
    tempDoc["unit_of_measurement"] = "°C";
    tempDoc["device_class"] = "temperature";
    tempDoc["unique_id"] = String(deviceId) + "_temp";
    
    JsonObject device = tempDoc.createNestedObject("device");
    device["identifiers"][0] = deviceId;
    device["name"] = deviceName;
    device["model"] = "ESP32 Thermostat";
    device["manufacturer"] = "DIY";
    
    char tempPayload[512];
    serializeJson(tempDoc, tempPayload);
    
    char tempDiscTopic[128];
    snprintf(tempDiscTopic, sizeof(tempDiscTopic), 
             "%s/sensor/%s_temp/config", HA_DISCOVERY_PREFIX, deviceId);
    mqttClient.publish(tempDiscTopic, tempPayload, true);
    
    // Climate entity
    StaticJsonDocument<768> climateDoc;
    climateDoc["name"] = deviceName;
    climateDoc["mode_state_topic"] = modeTopic;
    climateDoc["mode_command_topic"] = modeSetTopic;
    climateDoc["temperature_state_topic"] = tempTopic;
    climateDoc["temperature_command_topic"] = setTempTopic;
    climateDoc["current_temperature_topic"] = tempTopic;
    climateDoc["temp_step"] = 0.5;
    climateDoc["min_temp"] = 15;
    climateDoc["max_temp"] = 45;
    climateDoc["unique_id"] = String(deviceId) + "_climate";
    
    JsonArray modes = climateDoc.createNestedArray("modes");
    modes.add("off");
    modes.add("heat");
    
    JsonObject climateDevice = climateDoc.createNestedObject("device");
    climateDevice["identifiers"][0] = deviceId;
    climateDevice["name"] = deviceName;
    climateDevice["model"] = "ESP32 Thermostat";
    climateDevice["manufacturer"] = "DIY";
    
    char climatePayload[768];
    serializeJson(climateDoc, climatePayload);
    
    char climateDiscTopic[128];
    snprintf(climateDiscTopic, sizeof(climateDiscTopic),
             "%s/climate/%s/config", HA_DISCOVERY_PREFIX, deviceId);
    mqttClient.publish(climateDiscTopic, climatePayload, true);

    // WiFi RSSI sensor
    StaticJsonDocument<512> rssiDoc;
    rssiDoc["name"] = String(deviceName) + " WiFi Signal";
    rssiDoc["state_topic"] = statusTopic;
    rssiDoc["value_template"] = "{{ value_json.wifi_rssi }}";
    rssiDoc["unit_of_measurement"] = "dBm";
    rssiDoc["device_class"] = "signal_strength";
    rssiDoc["unique_id"] = String(deviceId) + "_rssi";
    rssiDoc["entity_category"] = "diagnostic";

    JsonObject rssiDevice = rssiDoc.createNestedObject("device");
    rssiDevice["identifiers"][0] = deviceId;

    char rssiPayload[512];
    serializeJson(rssiDoc, rssiPayload);

    char rssiDiscTopic[128];
    snprintf(rssiDiscTopic, sizeof(rssiDiscTopic),
             "%s/sensor/%s_rssi/config", HA_DISCOVERY_PREFIX, deviceId);
    mqttClient.publish(rssiDiscTopic, rssiPayload, true);

    // Free heap sensor
    StaticJsonDocument<512> heapDoc;
    heapDoc["name"] = String(deviceName) + " Free Memory";
    heapDoc["state_topic"] = statusTopic;
    heapDoc["value_template"] = "{{ value_json.free_heap }}";
    heapDoc["unit_of_measurement"] = "bytes";
    heapDoc["unique_id"] = String(deviceId) + "_heap";
    heapDoc["entity_category"] = "diagnostic";
    heapDoc["icon"] = "mdi:memory";

    JsonObject heapDevice = heapDoc.createNestedObject("device");
    heapDevice["identifiers"][0] = deviceId;

    char heapPayload[512];
    serializeJson(heapDoc, heapPayload);

    char heapDiscTopic[128];
    snprintf(heapDiscTopic, sizeof(heapDiscTopic),
             "%s/sensor/%s_heap/config", HA_DISCOVERY_PREFIX, deviceId);
    mqttClient.publish(heapDiscTopic, heapPayload, true);

    // Uptime sensor
    StaticJsonDocument<512> uptimeDoc;
    uptimeDoc["name"] = String(deviceName) + " Uptime";
    uptimeDoc["state_topic"] = statusTopic;
    uptimeDoc["value_template"] = "{{ value_json.uptime }}";
    uptimeDoc["unit_of_measurement"] = "s";
    uptimeDoc["unique_id"] = String(deviceId) + "_uptime";
    uptimeDoc["entity_category"] = "diagnostic";
    uptimeDoc["icon"] = "mdi:clock-outline";

    JsonObject uptimeDevice = uptimeDoc.createNestedObject("device");
    uptimeDevice["identifiers"][0] = deviceId;

    char uptimePayload[512];
    serializeJson(uptimeDoc, uptimePayload);

    char uptimeDiscTopic[128];
    snprintf(uptimeDiscTopic, sizeof(uptimeDiscTopic),
             "%s/sensor/%s_uptime/config", HA_DISCOVERY_PREFIX, deviceId);
    mqttClient.publish(uptimeDiscTopic, uptimePayload, true);

    Serial.println("[MQTT] Home Assistant discovery sent (5 entities)");
}

/**
 * Set setpoint callback
 */
void mqtt_set_setpoint_callback(MQTTMessageCallback_t callback) {
    setpointCallback = callback;
}

/**
 * Set mode callback
 */
void mqtt_set_mode_callback(MQTTMessageCallback_t callback) {
    modeCallback = callback;
}

/**
 * Save MQTT configuration
 */
void mqtt_save_config(const char* server, int port, 
                      const char* user, const char* password) {
    Serial.println("[MQTT] Saving configuration");
    
    Preferences prefs;
    prefs.begin("thermostat", false);
    prefs.putString("mqtt_broker", server);
    prefs.putFloat("mqtt_port", (float)port);
    prefs.putString("mqtt_user", user);
    prefs.putString("mqtt_pass", password);
    prefs.end();
}

/**
 * Get base topic
 */
const char* mqtt_get_base_topic(void) {
    return baseTopic;
}

/**
 * Build topic strings
 */
static void buildTopics(void) {
    snprintf(tempTopic, sizeof(tempTopic), "%s/temperature", baseTopic);
    snprintf(stateTopic, sizeof(stateTopic), "%s/state", baseTopic);
    snprintf(modeTopic, sizeof(modeTopic), "%s/mode", baseTopic);
    snprintf(setTempTopic, sizeof(setTempTopic), "%s/setpoint/set", baseTopic);
    snprintf(modeSetTopic, sizeof(modeSetTopic), "%s/mode/set", baseTopic);
    snprintf(statusTopic, sizeof(statusTopic), "%s/status", baseTopic);
}

/**
 * MQTT message callback
 */
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Convert payload to string
    char message[256];
    if (length >= sizeof(message)) {
        length = sizeof(message) - 1;
    }
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.print("[MQTT] Message on ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(message);
    
    // Route to appropriate callback
    if (strcmp(topic, setTempTopic) == 0) {
        if (setpointCallback != NULL) {
            setpointCallback(topic, message);
        }
    } else if (strcmp(topic, modeSetTopic) == 0) {
        if (modeCallback != NULL) {
            modeCallback(topic, message);
        }
    }
}
