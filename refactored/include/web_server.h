/**
 * web_server.h
 * HTTP Web Server and API Endpoints
 * 
 * Handles:
 * - Web page serving (Home, Schedule, Info, Logs, Settings)
 * - API endpoints for status and control
 * - OTA firmware updates (manual + GitHub auto-update)
 * - HTML generation helpers
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include <HTTPClient.h>
#include <Update.h>

// Callback function types for web server actions
typedef void (*WebServerCallback_t)(void);
typedef void (*TempModeCallback_t)(float temp, const char* mode);
typedef void (*ScheduleSaveCallback_t)(void);

// Function declarations

/**
 * Initialize web server
 * Sets up all routes and starts server on port 80
 */
void webserver_init(void);

/**
 * Web server task - call from main loop
 * Handles incoming HTTP requests
 */
void webserver_task(void);

/**
 * Set callback for temperature/mode changes from web interface
 * @param callback Function to call when settings changed
 */
void webserver_set_control_callback(TempModeCallback_t callback);

/**
 * Set callback for schedule save
 * @param callback Function to call when schedule saved
 */
void webserver_set_schedule_callback(ScheduleSaveCallback_t callback);

/**
 * Set callback for device restart
 * @param callback Function to call before restart
 */
void webserver_set_restart_callback(WebServerCallback_t callback);

/**
 * Set current thermostat state for web pages
 * Call this to update values before serving pages
 */
void webserver_set_state(float currentTemp, float targetTemp, 
                         bool heating, const char* mode, int power);

/**
 * Set device information for web pages
 */
void webserver_set_device_info(const char* deviceName, const char* firmwareVersion);

/**
 * Set network status for info page
 */
void webserver_set_network_status(bool connected, bool apMode, 
                                  const char* ssid, const char* ip);

/**
 * Add log entry for logs page
 * @param message Log message to add
 */
void webserver_add_log(const char* message);

/**
 * Set schedule data for schedule page
 * @param enabled Schedule enabled state
 * @param slotCount Number of active slots
 * @param slots Pointer to schedule slot array
 */
void webserver_set_schedule_data(bool enabled, int slotCount, void* slots);

/**
 * Get HTML header with navigation
 * @param title Page title
 * @param activePage Active navigation item ("home", "schedule", "info", "logs", "settings")
 * @return HTML header string (caller must free)
 */
String webserver_get_html_header(const char* title, const char* activePage);

/**
 * Get HTML footer
 * @param uptimeSeconds System uptime in seconds
 * @return HTML footer string (caller must free)
 */
String webserver_get_html_footer(unsigned long uptimeSeconds);

#endif // WEB_SERVER_H
