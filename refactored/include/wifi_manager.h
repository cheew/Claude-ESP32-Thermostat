/**
 * wifi_manager.h
 * WiFi Connection, AP Mode, and mDNS Management
 * 
 * Handles:
 * - WiFi station mode connection/reconnection
 * - Access Point mode for initial setup
 * - mDNS/Bonjour service advertisement
 * - Network status monitoring
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// WiFi connection states
typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE
} WiFiState_t;

// AP Mode configuration
typedef struct {
    const char* ssid;
    const char* password;
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
} APConfig_t;

// Function declarations

/**
 * Initialize WiFi manager
 * Loads saved credentials and attempts connection
 * Falls back to AP mode if no credentials or connection fails
 */
void wifi_init(void);

/**
 * WiFi task - call from main loop
 * Handles reconnection logic
 */
void wifi_task(void);

/**
 * Connect to WiFi using saved or provided credentials
 * @param ssid WiFi network name (NULL to use saved)
 * @param password WiFi password (NULL to use saved)
 * @return true if connection successful
 */
bool wifi_connect(const char* ssid, const char* password);

/**
 * Start Access Point mode
 */
void wifi_start_ap_mode(void);

/**
 * Setup mDNS responder with device name
 * Creates hostname from device name (spaces->hyphens, lowercase)
 * @param deviceName Display name for the device
 */
void wifi_setup_mdns(const char* deviceName);

/**
 * Get current WiFi state
 * @return Current connection state
 */
WiFiState_t wifi_get_state(void);

/**
 * Check if currently in AP mode
 * @return true if in AP mode
 */
bool wifi_is_ap_mode(void);

/**
 * Get current IP address as string
 * @return IP address (static string, do not free)
 */
const char* wifi_get_ip_address(void);

/**
 * Get WiFi signal strength (RSSI)
 * @return Signal strength in dBm (-100 to 0)
 */
int wifi_get_rssi(void);

/**
 * Get current SSID
 * @return SSID string (static, do not free)
 */
const char* wifi_get_ssid(void);

/**
 * Get MAC address
 * @return MAC address string (static, do not free)
 */
const char* wifi_get_mac_address(void);

/**
 * Save WiFi credentials to preferences
 * @param ssid Network SSID
 * @param password Network password
 */
void wifi_save_credentials(const char* ssid, const char* password);

/**
 * Sanitize hostname - removes invalid characters
 * Converts spaces to hyphens, removes non-alphanumeric except hyphens
 * @param name Input device name
 * @param output Output buffer for sanitized hostname
 * @param maxLen Maximum length of output buffer
 */
void wifi_sanitize_hostname(const char* name, char* output, size_t maxLen);

#endif // WIFI_MANAGER_H
