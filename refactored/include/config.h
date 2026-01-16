#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// FIRMWARE INFORMATION
// ============================================
#define FIRMWARE_VERSION "2.2.0"
#define DEVICE_MODEL "ESP32 Reptile Thermostat"
#define MANUFACTURER "DIY"

// ============================================
// HARDWARE PIN DEFINITIONS
// ============================================

// Temperature Sensor (DS18B20)
#define ONE_WIRE_BUS 4           // GPIO4 - Temperature sensor data

// AC Dimmer (RobotDyn)
#define DIMMER_HEAT_PIN 5        // GPIO5 - PWM control for heat dimmer
#define DIMMER_LIGHT_PIN 25      // GPIO25 - PWM control for light dimmer (future)
#define ZEROCROSS_PIN 27         // GPIO27 - Zero-cross detection (shared)

// TFT Display (ILI9341 - JC2432S028)
// SPI pins defined in TFT_eSPI User_Setup.h:
// MOSI = GPIO 23
// MISO = GPIO 19
// SCK  = GPIO 18
// CS   = GPIO 15
// DC   = GPIO 2
// RST  = GPIO 33
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 33
#define TOUCH_CS 22              // Touch controller CS

// ============================================
// DISPLAY SETTINGS
// ============================================
#define DISPLAY_UPDATE_INTERVAL 500    // ms - How often to update display
#define DISPLAY_ROTATION 1             // 0-3, 1=landscape
#define TOUCH_DEBOUNCE_MS 300          // Touch debounce delay

// ============================================
// TEMPERATURE SENSOR SETTINGS
// ============================================
#define TEMP_READ_INTERVAL 2000        // ms - How often to read temperature
#define TEMP_MIN_VALID -50.0           // °C - Minimum valid temperature
#define TEMP_MAX_VALID 100.0           // °C - Maximum valid temperature
#define TEMP_CHANGE_THRESHOLD 0.1      // °C - Minimum change to trigger update
#define TEMP_ERROR_LOG_INTERVAL 60000  // ms - Log sensor errors once per minute

// ============================================
// THERMOSTAT LIMITS
// ============================================
#define TEMP_MIN_SETPOINT 15.0         // °C - Minimum target temperature
#define TEMP_MAX_SETPOINT 45.0         // °C - Maximum target temperature
#define TEMP_DEFAULT_TARGET 28.0       // °C - Default target on first boot
#define TEMP_MIN_NIGHT 20.0            // °C - Safety minimum for night mode

// ============================================
// PID CONTROL SETTINGS
// ============================================
#define PID_UPDATE_INTERVAL 1000       // ms - PID calculation interval
#define PID_DEFAULT_KP 10.0            // Proportional gain
#define PID_DEFAULT_KI 0.5             // Integral gain
#define PID_DEFAULT_KD 5.0             // Derivative gain
#define PID_INTEGRAL_MIN -10.0         // Anti-windup minimum
#define PID_INTEGRAL_MAX 10.0          // Anti-windup maximum
#define PID_OUTPUT_MIN 0               // Minimum power output %
#define PID_OUTPUT_MAX 100             // Maximum power output %

// ============================================
// NETWORK SETTINGS (DEFAULTS)
// ============================================
#define DEFAULT_WIFI_SSID "mesh"
#define DEFAULT_WIFI_PASSWORD "Oasis0asis"
#define WIFI_CONNECT_TIMEOUT 20        // Attempts before giving up
#define WIFI_RETRY_DELAY 500           // ms between connection attempts

// Access Point Mode
#define AP_SSID "ReptileThermostat"
#define AP_PASSWORD "thermostat123"
#define AP_IP "192.168.4.1"

// mDNS
#define MDNS_DOMAIN ".local"

// ============================================
// MQTT SETTINGS (DEFAULTS)
// ============================================
#define DEFAULT_MQTT_SERVER "192.168.1.123"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_USER "admin"
#define DEFAULT_MQTT_PASSWORD "Oasis0asis!!"
#define MQTT_CLIENT_ID "esp32_thermostat"
#define MQTT_RECONNECT_INTERVAL 5000   // ms - Try reconnecting every 5s
#define MQTT_BUFFER_SIZE 512
#define MQTT_PUBLISH_INTERVAL 30000    // ms - Status update interval

// Home Assistant Discovery
#define HA_DISCOVERY_PREFIX "homeassistant"
#define DEFAULT_DEVICE_NAME "Reptile Thermostat"
#define DEFAULT_DEVICE_ID "reptile_thermostat_01"

// ============================================
// WEB SERVER SETTINGS
// ============================================
#define WEB_SERVER_PORT 80

// ============================================
// OTA UPDATE SETTINGS
// ============================================
#define GITHUB_USER "cheew"
#define GITHUB_REPO "Claude-ESP32-Thermostat"
#define GITHUB_FIRMWARE_FILE "firmware.bin"

// ============================================
// LOGGING SYSTEM
// ============================================
#define MAX_LOG_ENTRIES 20             // Circular buffer size

// ============================================
// SCHEDULING SYSTEM
// ============================================
#define MAX_SCHEDULE_SLOTS 8           // Maximum time slots per day
#define SCHEDULE_CHECK_INTERVAL 60000  // ms - Check schedule every minute

// ============================================
// STORAGE NAMESPACES
// ============================================
#define PREFS_NAMESPACE "thermostat"
#define PREFS_SCHEDULE_NAMESPACE "schedule"

// ============================================
// SYSTEM SETTINGS
// ============================================
#define SERIAL_BAUD_RATE 115200
#define BOOT_DELAY_MS 2000             // Display startup message duration

#endif // CONFIG_H
