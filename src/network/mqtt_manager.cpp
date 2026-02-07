/**
 * mqtt_manager.c
 * MQTT Client and Home Assistant Auto-Discovery Implementation
 */

#include "mqtt_manager.h"
#include "console.h"
#include "output_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// Default MQTT configuration
static const char* DEFAULT_MQTT_SERVER = "192.168.1.123";
static const int DEFAULT_MQTT_PORT = 1883;
static const char* DEFAULT_MQTT_USER = "admin";
static const char* DEFAULT_MQTT_PASSWORD = "Oasis0asis!!";
static const char* MQTT_CLIENT_ID_PREFIX = "esp32_thermostat";

// Home Assistant discovery
static const char* HA_DISCOVERY_PREFIX = "homeassistant";

// State variables
static WiFiClient espClient;
static PubSubClient mqttClient(espClient);
static MQTTState_t currentState = MQTT_STATE_DISCONNECTED;
static unsigned long lastConnectionAttempt = 0;
static const unsigned long CONNECTION_RETRY_INTERVAL = 5000; // 5 seconds

// Topic storage
static char baseTopic[64] = "";
static char tempTopic[80];
static char stateTopic[80];
static char modeTopic[80];
static char setTempTopic[80];
static char modeSetTopic[80];
static char statusTopic[80];

// Device info for HA discovery
static char deviceName[32] = "Reptile Thermostat";
static char deviceId[32] = "";
static char mqttClientId[48] = "";

// Message callbacks
static MQTTMessageCallback_t setpointCallback = NULL;
static MQTTMessageCallback_t modeCallback = NULL;

// Cloud MQTT configuration constants
static const int CLOUD_DEFAULT_PORT = 8883;
static const char* CLOUD_CLIENT_ID_PREFIX = "esp32_thermostat_cloud";
static const unsigned long CLOUD_RECONNECT_INTERVAL = 10000; // 10 seconds
static const int CLOUD_BUFFER_SIZE = 512;

// Cloud topic base - different namespace from local
static char cloudBaseTopic[64] = "";
static char cloudClientId[64] = "";

// Forward declarations
static void buildTopics(void);
static void mqttCallback(char* topic, byte* payload, unsigned int length);
static void buildDeviceIds(void);

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
    
    // Build unique IDs (client ID, device ID, base topic)
    buildDeviceIds();

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
    if (mqttClient.connect(mqttClientId, user.c_str(), password.c_str())) {
        Serial.println(" connected");
        currentState = MQTT_STATE_CONNECTED;

        // Subscribe to command topics for all 3 outputs
        for (int i = 1; i <= 3; i++) {
            char setTempTopicOut[80];
            char modeSetTopicOut[80];
            snprintf(setTempTopicOut, sizeof(setTempTopicOut), "%s/output%d/setpoint/set", baseTopic, i);
            snprintf(modeSetTopicOut, sizeof(modeSetTopicOut), "%s/output%d/mode/set", baseTopic, i);
            mqttClient.subscribe(setTempTopicOut);
            mqttClient.subscribe(modeSetTopicOut);
        }

        // Also subscribe to legacy topics for backwards compatibility
        mqttClient.subscribe(setTempTopic);
        mqttClient.subscribe(modeSetTopic);

        Serial.println("[MQTT] Subscribed to command topics (3 outputs + legacy)");

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
 * Publish all 3 outputs status (multi-output)
 */
void mqtt_publish_all_outputs(int wifiRssi, uint32_t freeHeap, unsigned long uptimeSeconds) {
    if (!mqttClient.connected()) return;

    // Publish each output individually
    for (int i = 0; i < 3; i++) {
        OutputConfig_t* output = output_manager_get_output(i);
        if (!output) continue;

        int outputNum = i + 1;
        char topicBuf[80];

        // Temperature topic
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/temperature", baseTopic, outputNum);
        char tempStr[8];
        snprintf(tempStr, sizeof(tempStr), "%.1f", output->currentTemp);
        mqttClient.publish(topicBuf, tempStr, true);

        // Setpoint topic
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/setpoint", baseTopic, outputNum);
        char setpointStr[8];
        snprintf(setpointStr, sizeof(setpointStr), "%.1f", output->targetTemp);
        mqttClient.publish(topicBuf, setpointStr, true);

        // State topic (heating/idle)
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/state", baseTopic, outputNum);
        mqttClient.publish(topicBuf, output->heating ? "heating" : "idle", true);

        // Mode topic (map to HA modes)
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/mode", baseTopic, outputNum);
        const char* haMode = "off";
        if (output->controlMode != CONTROL_MODE_OFF && output->enabled) {
            haMode = "heat";
        }
        mqttClient.publish(topicBuf, haMode, true);

        // Power topic
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/power", baseTopic, outputNum);
        char powerStr[8];
        snprintf(powerStr, sizeof(powerStr), "%d", output->currentPower);
        mqttClient.publish(topicBuf, powerStr, true);

        // Status topic (JSON with all data)
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/status", baseTopic, outputNum);
        StaticJsonDocument<384> doc;
        doc["temperature"] = round(output->currentTemp * 10) / 10.0;
        doc["setpoint"] = output->targetTemp;
        doc["heating"] = output->heating;
        doc["mode"] = output_manager_get_mode_name(output->controlMode);
        doc["power"] = output->currentPower;
        doc["enabled"] = output->enabled;
        doc["name"] = output->name;

        // Only include system info on output 1
        if (i == 0) {
            doc["wifi_rssi"] = wifiRssi;
            doc["free_heap"] = freeHeap;
            doc["uptime"] = uptimeSeconds;
        }

        char jsonBuf[384];
        serializeJson(doc, jsonBuf);
        mqttClient.publish(topicBuf, jsonBuf, true);
    }

    console_add_event(CONSOLE_EVENT_MQTT, "MQTT PUB: All 3 outputs published");
}

/**
 * Send Home Assistant auto-discovery (Multi-Output)
 */
void mqtt_send_ha_discovery(const char* devName, const char* devId) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] Cannot send HA discovery: not connected");
        return;
    }

    // Store device info
    strncpy(deviceName, devName, sizeof(deviceName) - 1);
    strncpy(deviceId, devId, sizeof(deviceId) - 1);

    Serial.println("[MQTT] Sending Home Assistant discovery (multi-output)...");

    // Create 3 climate entities (one per output)
    for (int i = 1; i <= 3; i++) {
        OutputConfig_t* output = output_manager_get_output(i - 1);
        if (!output) continue;

        char topicBuf[128];
        char payloadBuf[768];

        // Build climate discovery config
        StaticJsonDocument<768> doc;
        doc["name"] = String(output->name) + " (" + String(deviceName) + ")";

        // Topics for this output
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/mode", baseTopic, i);
        doc["mode_state_topic"] = topicBuf;
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/mode/set", baseTopic, i);
        doc["mode_command_topic"] = topicBuf;

        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/temperature", baseTopic, i);
        doc["current_temperature_topic"] = topicBuf;

        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/setpoint", baseTopic, i);
        doc["temperature_state_topic"] = topicBuf;
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/setpoint/set", baseTopic, i);
        doc["temperature_command_topic"] = topicBuf;

        doc["temp_step"] = 0.5;
        doc["min_temp"] = 15;
        doc["max_temp"] = 45;
        doc["unique_id"] = String(deviceId) + "_output" + String(i);

        JsonArray modes = doc.createNestedArray("modes");
        modes.add("off");
        modes.add("heat");

        // Device info (groups all entities under one device)
        JsonObject device = doc.createNestedObject("device");
        device["identifiers"][0] = deviceId;
        device["name"] = deviceName;
        device["model"] = "ESP32 Multi-Output Thermostat";
        device["manufacturer"] = "DIY";
        device["sw_version"] = "2.2.0";

        serializeJson(doc, payloadBuf);

        // Publish discovery config
        snprintf(topicBuf, sizeof(topicBuf), "%s/climate/%s_output%d/config",
                 HA_DISCOVERY_PREFIX, deviceId, i);
        mqttClient.publish(topicBuf, payloadBuf, true);

        delay(50); // Small delay between publishes
    }

    // System diagnostic sensors (attached to device, not specific output)

    // WiFi RSSI sensor
    StaticJsonDocument<512> rssiDoc;
    rssiDoc["name"] = String(deviceName) + " WiFi Signal";
    char statusTopic1[80];
    snprintf(statusTopic1, sizeof(statusTopic1), "%s/output1/status", baseTopic);
    rssiDoc["state_topic"] = statusTopic1;
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
    heapDoc["state_topic"] = statusTopic1;
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
    uptimeDoc["state_topic"] = statusTopic1;
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

    Serial.println("[MQTT] Home Assistant discovery sent (3 climates + 3 diagnostics)");
    console_add_event(CONSOLE_EVENT_MQTT, "MQTT: HA discovery published");
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
 * Get device ID (unique per device)
 */
const char* mqtt_get_device_id(void) {
    return deviceId;
}

/**
 * Build unique IDs and topics based on chip ID
 */
static void buildDeviceIds(void) {
    uint64_t chipId = ESP.getEfuseMac();
    uint32_t shortId = (uint32_t)(chipId & 0xFFFFFF);

    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%06x", shortId);

    snprintf(deviceId, sizeof(deviceId), "reptile_thermostat_%s", suffix);
    snprintf(baseTopic, sizeof(baseTopic), "reptile/%s", deviceId);
    snprintf(mqttClientId, sizeof(mqttClientId), "%s_%s", MQTT_CLIENT_ID_PREFIX, suffix);
    snprintf(cloudBaseTopic, sizeof(cloudBaseTopic), "thermostat/%s", deviceId);
    snprintf(cloudClientId, sizeof(cloudClientId), "%s_%s", CLOUD_CLIENT_ID_PREFIX, suffix);
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

    // Check for multi-output topics (format: base/output1/setpoint/set)
    for (int i = 1; i <= 3; i++) {
        char setTempTopicOut[80];
        char modeSetTopicOut[80];
        snprintf(setTempTopicOut, sizeof(setTempTopicOut), "%s/output%d/setpoint/set", baseTopic, i);
        snprintf(modeSetTopicOut, sizeof(modeSetTopicOut), "%s/output%d/mode/set", baseTopic, i);

        if (strcmp(topic, setTempTopicOut) == 0) {
            // Setpoint command for specific output
            float target = atof(message);
            if (target >= 15.0 && target <= 45.0) {
                output_manager_set_target(i - 1, target);
                output_manager_save_config();
                console_add_event_f(CONSOLE_EVENT_MQTT, "MQTT SET: Output %d target = %.1f", i, target);
            }
            return;
        } else if (strcmp(topic, modeSetTopicOut) == 0) {
            // Mode command for specific output
            ControlMode_t mode = CONTROL_MODE_OFF;
            if (strcmp(message, "heat") == 0 || strcmp(message, "on") == 0) {
                mode = CONTROL_MODE_PID;
            } else if (strcmp(message, "off") == 0) {
                mode = CONTROL_MODE_OFF;
            }
            output_manager_set_mode(i - 1, mode);
            output_manager_save_config();
            console_add_event_f(CONSOLE_EVENT_MQTT, "MQTT SET: Output %d mode = %s", i, message);
            return;
        }
    }

    // Legacy topic support (applies to Output 1)
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

// ============================================
// CLOUD MQTT BROKER (HiveMQ Cloud / TLS)
// ============================================

// ISRG Root X1 - Let's Encrypt root CA (used by HiveMQ Cloud)
// Valid until June 4, 2035
static const char CLOUD_CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// Cloud MQTT state variables
static WiFiClientSecure cloudWifiClient;
static PubSubClient cloudMqttClient(cloudWifiClient);
static MQTTState_t cloudCurrentState = MQTT_STATE_DISCONNECTED;
static unsigned long cloudLastConnectionAttempt = 0;
static bool cloudEnabled = false;
static bool cloudInitialized = false;

// Forward declaration
static bool cloud_mqtt_connect(void);
static void cloudMqttCallback(char* topic, byte* payload, unsigned int length);

/**
 * Initialize cloud MQTT connection
 */
void cloud_mqtt_init(void) {
    Serial.println("[CLOUD-MQTT] Initializing cloud MQTT");

    // Load cloud config from preferences
    Preferences prefs;
    prefs.begin("thermostat", true);
    cloudEnabled = prefs.getBool("cloud_enabled", false);
    String server = prefs.getString("cloud_broker", "");
    int port = prefs.getInt("cloud_port", CLOUD_DEFAULT_PORT);
    prefs.end();

    if (!cloudEnabled || server.length() == 0) {
        Serial.println("[CLOUD-MQTT] Cloud MQTT disabled or not configured");
        return;
    }

    // Ensure IDs are initialized
    if (deviceId[0] == '\0') {
        buildDeviceIds();
    }

    // Configure TLS with CA certificate
    cloudWifiClient.setCACert(CLOUD_CA_CERT);

    // Setup cloud MQTT client
    cloudMqttClient.setServer(server.c_str(), port);
    cloudMqttClient.setCallback(cloudMqttCallback);
    cloudMqttClient.setBufferSize(CLOUD_BUFFER_SIZE);
    cloudInitialized = true;

    Serial.print("[CLOUD-MQTT] Configured for: ");
    Serial.print(server);
    Serial.print(":");
    Serial.println(port);
}

/**
 * Cloud MQTT task - handles reconnection and message loop
 */
void cloud_mqtt_task(void) {
    if (!cloudEnabled || !cloudInitialized) return;

    if (cloudMqttClient.connected()) {
        cloudMqttClient.loop();
        if (cloudCurrentState != MQTT_STATE_CONNECTED) {
            cloudCurrentState = MQTT_STATE_CONNECTED;
        }
    } else {
        if (cloudCurrentState != MQTT_STATE_DISCONNECTED) {
            Serial.println("[CLOUD-MQTT] Connection lost");
            cloudCurrentState = MQTT_STATE_DISCONNECTED;
        }

        // Attempt reconnection at interval (10s for cloud)
        if (millis() - cloudLastConnectionAttempt >= CLOUD_RECONNECT_INTERVAL) {
            cloud_mqtt_connect();
        }
    }
}

/**
 * Connect to cloud MQTT broker with TLS
 */
static bool cloud_mqtt_connect(void) {
    cloudLastConnectionAttempt = millis();

    if (cloudMqttClient.connected()) return true;

    Serial.print("[CLOUD-MQTT] Attempting TLS connection...");
    cloudCurrentState = MQTT_STATE_CONNECTING;

    // Load credentials
    Preferences prefs;
    prefs.begin("thermostat", true);
    String server = prefs.getString("cloud_broker", "");
    String user = prefs.getString("cloud_user", "");
    String password = prefs.getString("cloud_pass", "");
    int port = prefs.getInt("cloud_port", CLOUD_DEFAULT_PORT);
    prefs.end();

    if (server.length() == 0) {
        Serial.println(" no server configured");
        cloudCurrentState = MQTT_STATE_DISCONNECTED;
        return false;
    }

    // Update server in case it changed
    cloudMqttClient.setServer(server.c_str(), port);

    // Build LWT topic
    char lwtTopic[80];
    snprintf(lwtTopic, sizeof(lwtTopic), "%s/status/online", cloudBaseTopic);

    // Connect with LWT (Last Will and Testament)
    if (cloudMqttClient.connect(
            cloudClientId,
            user.c_str(), password.c_str(),
            lwtTopic, 1, true, "false")) {

        Serial.println(" connected (TLS)");
        cloudCurrentState = MQTT_STATE_CONNECTED;

        // Publish online status
        cloudMqttClient.publish(lwtTopic, "true", true);

        // Subscribe to command topics for all 3 outputs
        for (int i = 1; i <= 3; i++) {
            char setTempTopicBuf[80];
            char modeSetTopicBuf[80];
            snprintf(setTempTopicBuf, sizeof(setTempTopicBuf),
                     "%s/output%d/setpoint/set", cloudBaseTopic, i);
            snprintf(modeSetTopicBuf, sizeof(modeSetTopicBuf),
                     "%s/output%d/mode/set", cloudBaseTopic, i);
            cloudMqttClient.subscribe(setTempTopicBuf);
            cloudMqttClient.subscribe(modeSetTopicBuf);
        }

        Serial.println("[CLOUD-MQTT] Subscribed to command topics (3 outputs)");
        console_add_event(CONSOLE_EVENT_MQTT, "Cloud MQTT connected (TLS)");
        return true;
    } else {
        Serial.print(" failed, rc=");
        Serial.println(cloudMqttClient.state());
        cloudCurrentState = MQTT_STATE_DISCONNECTED;
        return false;
    }
}

/**
 * Cloud MQTT message callback
 * Routes commands through same output_manager as local broker
 */
static void cloudMqttCallback(char* topic, byte* payload, unsigned int length) {
    char message[256];
    if (length >= sizeof(message)) length = sizeof(message) - 1;
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.print("[CLOUD-MQTT] Message on ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(message);

    // Check for output command topics
    for (int i = 1; i <= 3; i++) {
        char setTempTopicBuf[80];
        char modeSetTopicBuf[80];
        snprintf(setTempTopicBuf, sizeof(setTempTopicBuf),
                 "%s/output%d/setpoint/set", cloudBaseTopic, i);
        snprintf(modeSetTopicBuf, sizeof(modeSetTopicBuf),
                 "%s/output%d/mode/set", cloudBaseTopic, i);

        if (strcmp(topic, setTempTopicBuf) == 0) {
            float target = atof(message);
            if (target >= 15.0 && target <= 45.0) {
                output_manager_set_target(i - 1, target);
                output_manager_save_config();
                console_add_event_f(CONSOLE_EVENT_MQTT,
                    "CLOUD SET: Output %d target = %.1f", i, target);
            } else {
                Serial.printf("[CLOUD-MQTT] Rejected setpoint %.1f (out of range 15-45)\n", target);
            }
            return;
        } else if (strcmp(topic, modeSetTopicBuf) == 0) {
            ControlMode_t mode = CONTROL_MODE_OFF;
            if (strcmp(message, "heat") == 0 || strcmp(message, "on") == 0) {
                mode = CONTROL_MODE_PID;
            } else if (strcmp(message, "off") == 0) {
                mode = CONTROL_MODE_OFF;
            }
            output_manager_set_mode(i - 1, mode);
            output_manager_save_config();
            console_add_event_f(CONSOLE_EVENT_MQTT,
                "CLOUD SET: Output %d mode = %s", i, message);
            return;
        }
    }
}

/**
 * Publish all outputs to cloud broker
 */
void cloud_mqtt_publish_all_outputs(int wifiRssi, uint32_t freeHeap,
                                     unsigned long uptimeSeconds) {
    if (!cloudEnabled || !cloudMqttClient.connected()) return;

    for (int i = 0; i < 3; i++) {
        OutputConfig_t* output = output_manager_get_output(i);
        if (!output) continue;

        int outputNum = i + 1;
        char topicBuf[80];

        // Temperature
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/temperature",
                 cloudBaseTopic, outputNum);
        char tempStr[8];
        snprintf(tempStr, sizeof(tempStr), "%.1f", output->currentTemp);
        cloudMqttClient.publish(topicBuf, tempStr, true);

        // Setpoint
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/setpoint",
                 cloudBaseTopic, outputNum);
        char setpointStr[8];
        snprintf(setpointStr, sizeof(setpointStr), "%.1f", output->targetTemp);
        cloudMqttClient.publish(topicBuf, setpointStr, true);

        // State (heating/idle)
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/state",
                 cloudBaseTopic, outputNum);
        cloudMqttClient.publish(topicBuf,
            output->heating ? "heating" : "idle", true);

        // Mode
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/mode",
                 cloudBaseTopic, outputNum);
        const char* haMode = "off";
        if (output->controlMode != CONTROL_MODE_OFF && output->enabled) {
            haMode = "heat";
        }
        cloudMqttClient.publish(topicBuf, haMode, true);

        // Power
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/power",
                 cloudBaseTopic, outputNum);
        char powerStr[8];
        snprintf(powerStr, sizeof(powerStr), "%d", output->currentPower);
        cloudMqttClient.publish(topicBuf, powerStr, true);

        // Status JSON
        snprintf(topicBuf, sizeof(topicBuf), "%s/output%d/status",
                 cloudBaseTopic, outputNum);
        StaticJsonDocument<384> doc;
        doc["temperature"] = round(output->currentTemp * 10) / 10.0;
        doc["setpoint"] = output->targetTemp;
        doc["heating"] = output->heating;
        doc["mode"] = output_manager_get_mode_name(output->controlMode);
        doc["power"] = output->currentPower;
        doc["enabled"] = output->enabled;
        doc["name"] = output->name;

        if (i == 0) {
            doc["wifi_rssi"] = wifiRssi;
            doc["free_heap"] = freeHeap;
            doc["uptime"] = uptimeSeconds;
        }

        char jsonBuf[384];
        serializeJson(doc, jsonBuf);
        cloudMqttClient.publish(topicBuf, jsonBuf, true);
    }
}

/**
 * Check if cloud MQTT is connected
 */
bool cloud_mqtt_is_connected(void) {
    return cloudEnabled && cloudMqttClient.connected();
}

/**
 * Check if cloud MQTT is enabled
 */
bool cloud_mqtt_is_enabled(void) {
    return cloudEnabled;
}

/**
 * Save cloud MQTT configuration
 */
void cloud_mqtt_save_config(bool enabled, const char* server, int port,
                             const char* user, const char* password) {
    Serial.println("[CLOUD-MQTT] Saving configuration");
    Preferences prefs;
    prefs.begin("thermostat", false);
    prefs.putBool("cloud_enabled", enabled);
    prefs.putString("cloud_broker", server);
    prefs.putInt("cloud_port", port);
    prefs.putString("cloud_user", user);
    prefs.putString("cloud_pass", password);
    prefs.end();
}
