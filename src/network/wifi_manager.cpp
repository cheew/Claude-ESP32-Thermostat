/**
 * wifi_manager.c
 * WiFi Connection, AP Mode, and mDNS Management Implementation
 */

#include "wifi_manager.h"
#include <Arduino.h>

// Default WiFi credentials (fallback)
static const char* DEFAULT_SSID = "mesh";
static const char* DEFAULT_PASSWORD = "Oasis0asis";

// AP Mode configuration
static const char* AP_SSID = "ReptileThermostat";
static const char* AP_PASSWORD = "thermostat123";
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

// State variables
static WiFiState_t currentState = WIFI_STATE_DISCONNECTED;
static bool apMode = false;
static unsigned long lastConnectionAttempt = 0;
static const unsigned long CONNECTION_RETRY_INTERVAL = 30000; // 30 seconds

// Static buffers for return values
static char ipAddressBuffer[16] = "0.0.0.0";
static char ssidBuffer[33] = "";
static char macAddressBuffer[18] = "";

// Forward declarations
static void connectToWiFi(const char* ssid, const char* password);
static void updateIPAddress(void);

/**
 * Initialize WiFi manager
 */
void wifi_init(void) {
    Serial.println("[WiFi] Initializing WiFi manager");
    
    // Load saved credentials
    Preferences prefs;
    prefs.begin("thermostat", true);
    String savedSSID = prefs.getString("wifi_ssid", "");
    prefs.end();
    
    if (savedSSID.length() == 0) {
        // No saved credentials - start in AP mode
        Serial.println("[WiFi] No saved credentials, starting AP mode");
        wifi_start_ap_mode();
    } else {
        // Try to connect with saved credentials
        Serial.println("[WiFi] Connecting with saved credentials");
        wifi_connect(NULL, NULL);
    }
}

/**
 * WiFi task - handles reconnection
 */
void wifi_task(void) {
    // In AP mode: periodically try to reconnect to saved WiFi
    if (apMode) {
        if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
            Serial.println("[WiFi] AP mode: Attempting to reconnect to WiFi");
            wifi_connect(NULL, NULL);  // Will switch out of AP mode if successful
        }
        return;
    }

    // Check connection status
    if (WiFi.status() != WL_CONNECTED) {
        if (currentState != WIFI_STATE_DISCONNECTED) {
            Serial.println("[WiFi] Connection lost");
            currentState = WIFI_STATE_DISCONNECTED;
        }

        // Attempt reconnection at interval
        if (millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
            Serial.println("[WiFi] Attempting reconnection...");
            wifi_connect(NULL, NULL);
        }
    } else {
        if (currentState != WIFI_STATE_CONNECTED) {
            Serial.println("[WiFi] Connection established");
            currentState = WIFI_STATE_CONNECTED;
            updateIPAddress();
        }
    }
}

/**
 * Connect to WiFi
 */
bool wifi_connect(const char* ssid, const char* password) {
    lastConnectionAttempt = millis();
    
    // Load credentials from preferences if not provided
    String useSSID, usePassword;
    
    if (ssid == NULL || password == NULL) {
        Preferences prefs;
        prefs.begin("thermostat", true);
        useSSID = prefs.getString("wifi_ssid", DEFAULT_SSID);
        usePassword = prefs.getString("wifi_pass", DEFAULT_PASSWORD);
        prefs.end();
    } else {
        useSSID = String(ssid);
        usePassword = String(password);
    }
    
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(useSSID);
    
    currentState = WIFI_STATE_CONNECTING;
    WiFi.begin(useSSID.c_str(), usePassword.c_str());
    
    // Wait for connection (max 10 seconds)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Connected successfully");
        Serial.print("[WiFi] IP address: ");
        Serial.println(WiFi.localIP());
        
        currentState = WIFI_STATE_CONNECTED;
        apMode = false;
        updateIPAddress();
        
        // Store SSID for reference
        useSSID.toCharArray(ssidBuffer, sizeof(ssidBuffer));
        
        // Store MAC address
        String mac = WiFi.macAddress();
        mac.toCharArray(macAddressBuffer, sizeof(macAddressBuffer));
        
        return true;
    } else {
        Serial.println("[WiFi] Connection failed, starting AP mode");
        wifi_start_ap_mode();
        return false;
    }
}

/**
 * Start Access Point mode
 */
void wifi_start_ap_mode(void) {
    Serial.println("[WiFi] Starting Access Point mode");
    
    apMode = true;
    currentState = WIFI_STATE_AP_MODE;
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    Serial.print("[WiFi] AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("[WiFi] AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    updateIPAddress();
    strncpy(ssidBuffer, AP_SSID, sizeof(ssidBuffer));
}

/**
 * Setup mDNS responder
 */
void wifi_setup_mdns(const char* deviceName) {
    if (apMode || WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Cannot setup mDNS: not connected to WiFi");
        return;
    }
    
    // Sanitize hostname
    char hostname[32];
    wifi_sanitize_hostname(deviceName, hostname, sizeof(hostname));
    
    Serial.print("[WiFi] Setting up mDNS: ");
    Serial.print(hostname);
    Serial.println(".local");
    
    if (MDNS.begin(hostname)) {
        // Add HTTP service
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "type", "reptile-thermostat");
        MDNS.addServiceTxt("http", "tcp", "version", "2.2.0");
        MDNS.addServiceTxt("http", "tcp", "name", deviceName);
        
        Serial.println("[WiFi] mDNS responder started successfully");
    } else {
        Serial.println("[WiFi] Error starting mDNS responder");
    }
}

/**
 * Get current WiFi state
 */
WiFiState_t wifi_get_state(void) {
    return currentState;
}

/**
 * Check if in AP mode
 */
bool wifi_is_ap_mode(void) {
    return apMode;
}

/**
 * Get IP address as string
 */
const char* wifi_get_ip_address(void) {
    return ipAddressBuffer;
}

/**
 * Get WiFi signal strength
 */
int wifi_get_rssi(void) {
    if (WiFi.status() == WL_CONNECTED && !apMode) {
        return WiFi.RSSI();
    }
    return -100; // No signal
}

/**
 * Get current SSID
 */
const char* wifi_get_ssid(void) {
    return ssidBuffer;
}

/**
 * Get MAC address
 */
const char* wifi_get_mac_address(void) {
    return macAddressBuffer;
}

/**
 * Save WiFi credentials
 */
void wifi_save_credentials(const char* ssid, const char* password) {
    Serial.println("[WiFi] Saving WiFi credentials");
    
    Preferences prefs;
    prefs.begin("thermostat", false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", password);
    prefs.end();
}

/**
 * Sanitize hostname for mDNS
 */
void wifi_sanitize_hostname(const char* name, char* output, size_t maxLen) {
    if (name == NULL || output == NULL || maxLen == 0) {
        return;
    }
    
    // Convert to lowercase, replace spaces with hyphens
    // Remove invalid characters (keep only alphanumeric and hyphen)
    size_t outIdx = 0;
    size_t inLen = strlen(name);
    
    for (size_t i = 0; i < inLen && outIdx < maxLen - 1; i++) {
        char c = name[i];
        
        // Skip leading/trailing spaces
        if (c == ' ') {
            if (outIdx > 0 && i < inLen - 1) {
                output[outIdx++] = '-';
            }
        }
        // Keep alphanumeric
        else if (isalnum(c)) {
            output[outIdx++] = tolower(c);
        }
        // Keep existing hyphens
        else if (c == '-' && outIdx > 0) {
            output[outIdx++] = c;
        }
        // Skip other characters
    }
    
    // Remove trailing hyphen if present
    if (outIdx > 0 && output[outIdx - 1] == '-') {
        outIdx--;
    }
    
    output[outIdx] = '\0';
    
    // Default if empty
    if (outIdx == 0) {
        strncpy(output, "thermostat", maxLen);
    }
}

/**
 * Update IP address buffer
 */
static void updateIPAddress(void) {
    if (apMode) {
        IPAddress ip = WiFi.softAPIP();
        snprintf(ipAddressBuffer, sizeof(ipAddressBuffer), 
                 "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    } else if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(ipAddressBuffer, sizeof(ipAddressBuffer),
                 "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    } else {
        strncpy(ipAddressBuffer, "0.0.0.0", sizeof(ipAddressBuffer));
    }
}
