/**
 * mqtt_manager.h
 * MQTT Client and Home Assistant Auto-Discovery
 * 
 * Handles:
 * - MQTT connection/reconnection to broker
 * - Home Assistant MQTT auto-discovery
 * - Topic subscription and message callbacks
 * - Status publishing (temperature, state, mode)
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <Preferences.h>

// MQTT connection states
typedef enum {
    MQTT_STATE_DISCONNECTED,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED
} MQTTState_t;

// MQTT configuration
typedef struct {
    const char* server;
    int port;
    const char* user;
    const char* password;
    const char* clientId;
} MQTTConfig_t;

// Callback function types
typedef void (*MQTTMessageCallback_t)(const char* topic, const char* message);

// Function declarations

/**
 * Initialize MQTT manager
 * Sets up MQTT client with default or saved configuration
 */
void mqtt_init(void);

/**
 * MQTT task - call from main loop
 * Handles reconnection and message processing
 */
void mqtt_task(void);

/**
 * Connect to MQTT broker
 * Uses saved credentials from preferences
 * @return true if connection successful
 */
bool mqtt_connect(void);

/**
 * Disconnect from MQTT broker
 */
void mqtt_disconnect(void);

/**
 * Check if MQTT is connected
 * @return true if connected to broker
 */
bool mqtt_is_connected(void);

/**
 * Get current MQTT state
 * @return Current connection state
 */
MQTTState_t mqtt_get_state(void);

/**
 * Publish temperature value
 * @param temperature Current temperature in °C
 */
void mqtt_publish_temperature(float temperature);

/**
 * Publish heating state
 * @param heating true if heating, false if idle
 */
void mqtt_publish_state(bool heating);

/**
 * Publish current mode
 * @param mode Mode string ("auto", "on", "off")
 */
void mqtt_publish_mode(const char* mode);

/**
 * Publish complete status (JSON)
 * @param temperature Current temperature
 * @param setpoint Target temperature
 * @param heating Heating state
 * @param mode Operating mode
 * @param power Power output percentage (0-100)
 */
void mqtt_publish_status(float temperature, float setpoint, 
                         bool heating, const char* mode, int power);

/**
 * Send Home Assistant auto-discovery messages
 * Creates climate entity and temperature sensor
 * @param deviceName Friendly name for the device
 * @param deviceId Unique device identifier
 */
void mqtt_send_ha_discovery(const char* deviceName, const char* deviceId);

/**
 * Set callback for temperature setpoint changes
 * @param callback Function to call when setpoint message received
 */
void mqtt_set_setpoint_callback(MQTTMessageCallback_t callback);

/**
 * Set callback for mode changes
 * @param callback Function to call when mode message received
 */
void mqtt_set_mode_callback(MQTTMessageCallback_t callback);

/**
 * Save MQTT configuration to preferences
 * @param server Broker IP address or hostname
 * @param port Broker port (usually 1883)
 * @param user Username for authentication
 * @param password Password for authentication
 */
void mqtt_save_config(const char* server, int port, 
                      const char* user, const char* password);

/**
 * Get MQTT topic base path
 * Format: "reptile/{device_id}"
 * @return Base topic string (static, do not free)
 */
const char* mqtt_get_base_topic(void);

#endif // MQTT_MANAGER_H
