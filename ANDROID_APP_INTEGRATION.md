# Android App Integration Guide

## ESP32 Multi-Output Thermostat - Network Discovery & API Integration

This document provides all technical details needed to integrate an Android app with the ESP32 Thermostat using mDNS discovery and REST API.

---

## 1. mDNS Service Discovery

### Service Details
- **Service Type**: `_http._tcp.local.`
- **Service Name**: `havoc` (configurable via device settings)
- **Default Hostname**: `havoc.local`
- **Port**: `80` (HTTP)

### mDNS Implementation (ESP32 Side)
Located in: `src/network/wifi_manager.cpp`

```cpp
// mDNS responder is initialized after WiFi connection
MDNS.begin(hostname);  // hostname = "havoc" by default
MDNS.addService("http", "tcp", 80);
```

### Android mDNS Discovery (NsdManager)

**Step 1: Add permissions to AndroidManifest.xml**
```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
```

**Step 2: Discover service using NsdManager**
```kotlin
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.content.Context

class ThermostatDiscovery(private val context: Context) {
    private val nsdManager: NsdManager by lazy {
        context.getSystemService(Context.NSD_SERVICE) as NsdManager
    }

    private val serviceType = "_http._tcp."

    private val discoveryListener = object : NsdManager.DiscoveryListener {
        override fun onDiscoveryStarted(serviceType: String) {
            println("mDNS discovery started for $serviceType")
        }

        override fun onServiceFound(serviceInfo: NsdServiceInfo) {
            println("Service found: ${serviceInfo.serviceName}")
            // Check if this is our thermostat
            if (serviceInfo.serviceName.contains("havoc", ignoreCase = true) ||
                serviceInfo.serviceName.contains("thermostat", ignoreCase = true)) {
                nsdManager.resolveService(serviceInfo, resolveListener)
            }
        }

        override fun onServiceLost(serviceInfo: NsdServiceInfo) {
            println("Service lost: ${serviceInfo.serviceName}")
        }

        override fun onDiscoveryStopped(serviceType: String) {
            println("Discovery stopped")
        }

        override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
            println("Discovery failed: $errorCode")
        }

        override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
            println("Stop discovery failed: $errorCode")
        }
    }

    private val resolveListener = object : NsdManager.ResolveListener {
        override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
            println("Resolve failed: $errorCode")
        }

        override fun onServiceResolved(serviceInfo: NsdServiceInfo) {
            println("Service resolved:")
            println("  Host: ${serviceInfo.host}")
            println("  Port: ${serviceInfo.port}")

            val baseUrl = "http://${serviceInfo.host.hostAddress}:${serviceInfo.port}"
            println("  Base URL: $baseUrl")

            // Now you can make API calls to baseUrl
            onThermostatDiscovered(baseUrl)
        }
    }

    fun startDiscovery() {
        nsdManager.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
    }

    fun stopDiscovery() {
        nsdManager.stopServiceDiscovery(discoveryListener)
    }

    // Callback to handle discovered thermostat
    private fun onThermostatDiscovered(baseUrl: String) {
        // Connect to API and fetch device info
        // Store baseUrl for future API calls
    }
}
```

**Step 3: Alternative - Direct .local resolution**
If mDNS discovery is problematic, you can try direct hostname resolution:

```kotlin
import java.net.InetAddress

fun resolveMdnsHostname(hostname: String = "havoc.local"): String? {
    return try {
        val address = InetAddress.getByName(hostname)
        "http://${address.hostAddress}"
    } catch (e: Exception) {
        println("Failed to resolve $hostname: ${e.message}")
        null
    }
}
```

---

## 2. REST API Reference

### Base URL
After mDNS discovery: `http://{IP_ADDRESS}:80`

Example: `http://192.168.1.236:80`

### Authentication
The device supports optional PIN-based authentication ("secure mode").

**Check if secure mode is enabled:**
```http
GET /api/status
```
Response includes `"secureMode": true/false`

**When secure mode is OFF:** All endpoints are open (WiFi network security only)

**When secure mode is ON:** Protected endpoints return `401 Unauthorized` or redirect to `/login`

**Protected endpoints:**
- `POST /api/output/*/control` - Temperature/mode changes
- `POST /api/output/*/config` - Output configuration
- `POST /api/save-settings` - Save device settings
- `POST /api/restart` - Restart device
- `POST /api/upload` - Firmware upload

**Public endpoints (always accessible):**
- `GET /api/status` - Device status
- `GET /api/outputs` - All outputs status
- `GET /api/output/*` - Single output status
- `GET /api/sensors` - Sensor readings
- `GET /api/info` - Device info

**Login API:**
```http
POST /api/login
Content-Type: application/json

{
  "pin": "1234"
}
```

**Success Response:**
```json
{"success": true}
```
Also sets `Set-Cookie: session=<token>` header

**Failure Response:**
```json
{"success": false, "error": "Invalid PIN"}
```
Status: `401 Unauthorized`

**Android Implementation:**
```kotlin
// Check if login required
suspend fun checkSecureMode(): Boolean {
    val status = api.getStatus()
    return status.secureMode
}

// Login and store session cookie
suspend fun login(pin: String): Boolean {
    val response = api.login(LoginRequest(pin))
    return response.success
}

// OkHttp cookie handling
val cookieJar = object : CookieJar {
    private val cookies = mutableListOf<Cookie>()

    override fun saveFromResponse(url: HttpUrl, cookies: List<Cookie>) {
        this.cookies.addAll(cookies)
    }

    override fun loadForRequest(url: HttpUrl): List<Cookie> = cookies
}

val client = OkHttpClient.Builder()
    .cookieJar(cookieJar)
    .build()
```

---

## 3. Multi-Output API Endpoints

### Get All Outputs Status
```http
GET /api/outputs
```

**Response:**
```json
{
  "outputs": [
    {
      "id": 1,
      "name": "Output 1",
      "enabled": true,
      "hardwareType": "AC Dimmer",
      "deviceType": "Light",
      "mode": "PID",
      "sensor": "28FF273C63140291",
      "temp": "18.9",
      "target": "20.0",
      "power": 0,
      "heating": false,
      "sensorHealth": "OK",
      "faultState": "None",
      "inFault": false
    },
    {
      "id": 2,
      "name": "Output 2",
      "enabled": false,
      "sensorHealth": "OK",
      "faultState": "None",
      "inFault": false
      // ... similar structure
    },
    {
      "id": 3,
      "name": "Output 3",
      "enabled": false,
      "sensorHealth": "Stale",
      "faultState": "Sensor Stale",
      "inFault": true
      // ... similar structure
    }
  ]
}
```

**New Fields (v2.2.0+):**
- `sensorHealth`: "OK" | "Stale" | "Error"
- `faultState`: "None" | "Sensor Stale" | "Sensor Error" | "Over Temp" | "Under Temp" | "Heater No Rise" | "Heater Runaway"
- `inFault`: boolean - true if any fault is active

### Get Single Output Status
```http
GET /api/output/{1|2|3}
```

**Response:** Single output object (same structure as above)

### Control Output (Set Temperature/Mode)
```http
POST /api/output/{1|2|3}/control
Content-Type: application/json

{
  "target": 22.5,           // Optional: target temperature (°C)
  "mode": "pid",            // Optional: off|manual|pid|onoff|schedule
  "manualPower": 75         // Optional: manual power 0-100 (only for manual mode)
}
```

**Response:**
```json
{
  "success": true,
  "message": "Output 1 updated"
}
```

**Control Modes:**
- `off` - Output disabled
- `manual` - Manual power control (0-100%)
- `pid` - PID temperature control (auto)
- `onoff` - Simple on/off with hysteresis
- `schedule` - Scheduled operation (not yet implemented)

### Configure Output
```http
POST /api/output/{1|2|3}/config
Content-Type: application/json

{
  "name": "Living Room",              // Optional: display name
  "enabled": true,                     // Optional: enable/disable output
  "hardwareType": "triac",             // Optional: triac|relay|pwm
  "deviceType": "heater",              // Optional: heater|cooler|humidifier|dehumidifier|fan
  "sensorAddress": "28FF273C63140291", // Optional: sensor MAC address
  "pid": {                             // Optional: PID parameters
    "kp": 50.0,
    "ki": 0.5,
    "kd": 10.0
  },
  "onoff": {                           // Optional: On/Off parameters
    "hysteresis": 0.5
  }
}
```

**Response:**
```json
{
  "success": true,
  "message": "Output 1 configured"
}
```

### Clear Output Fault (v2.2.0+)
```http
POST /api/output/{1|2|3}/clear-fault
```

**Response (Success):**
```json
{
  "ok": true,
  "data": {
    "message": "Fault cleared"
  }
}
```

**Response (Failure - condition still active):**
```json
{
  "ok": false,
  "error": {
    "code": "FAULT_ACTIVE",
    "message": "Cannot clear fault - condition still active",
    "currentFault": "Over Temp"
  }
}
```

---

## 4. Safety & Health API (v2.2.0+)

### Get System Health
```http
GET /api/v1/health
```

**Response:**
```json
{
  "ok": true,
  "data": {
    "uptime": 3600,
    "freeHeap": 212932,
    "minFreeHeap": 180000,
    "heapSize": 327680,
    "chipModel": "ESP32-D0WDQ6",
    "cpuFreqMHz": 240,
    "flash": {
      "size": 4194304,
      "speed": 40000000
    },
    "network": {
      "wifiConnected": true,
      "ssid": "mesh",
      "rssi": -45,
      "ip": "192.168.1.236"
    },
    "sensors": {
      "total": 2,
      "healthy": 2
    },
    "outputs": {
      "total": 3,
      "inFault": 0,
      "heating": 1
    },
    "faults": [],
    "build": {
      "version": "2.2.0"
    }
  }
}
```

**When faults are active:**
```json
{
  "ok": true,
  "data": {
    // ... other fields ...
    "outputs": {
      "total": 3,
      "inFault": 1,
      "heating": 0
    },
    "faults": [
      {
        "outputId": 2,
        "outputName": "Heat Mat",
        "fault": "Sensor Stale",
        "sensorHealth": "Stale",
        "durationSec": 45
      }
    ]
  }
}
```

### Single Output with Safety Details
```http
GET /api/output/{1|2|3}
```

**Response includes safety and fault sections (v2.2.0+):**
```json
{
  "id": 1,
  "name": "Lights",
  // ... existing fields ...
  "safety": {
    "maxTempC": "40.0",
    "minTempC": "5.0",
    "faultTimeoutSec": 30,
    "faultMode": "off",
    "capPowerPct": 30,
    "autoResume": false
  },
  "fault": {
    "sensorHealth": "OK",
    "state": "None",
    "inFault": false
  }
}
```

**Safety Fault Modes:**
- `off` - Turn output OFF when sensor fails (safest)
- `hold` - Hold last known good power level
- `cap` - Cap power to configured percentage

---

## 5. Sensor API Endpoints

### Get All Sensors
```http
GET /api/sensors
```

**Response:**
```json
{
  "sensors": [
    {
      "address": "28FF273C63140291",
      "name": "Temperature Sensor 1",
      "temperature": 18.9,
      "lastRead": 1234567890
    }
  ]
}
```

### Set Sensor Name
```http
POST /api/sensor/name
Content-Type: application/json

{
  "address": "28FF273C63140291",
  "name": "Living Room Sensor"
}
```

---

## 5. System API Endpoints

### Get System Status
```http
GET /api/status
```

**Response:**
```json
{
  "uptime": 3600,
  "freeHeap": 212932,
  "wifiConnected": true,
  "wifiSSID": "mesh",
  "wifiRSSI": -45,
  "ipAddress": "192.168.1.236",
  "mqttConnected": true
}
```

### Get Device Info
```http
GET /api/info
```

**Response:**
```json
{
  "deviceName": "havoc",
  "firmwareVersion": "v2.1.1",
  "hardwareVersion": "ESP32-D0WDQ6",
  "macAddress": "78:E3:6D:18:DF:3C"
}
```

### Get Console Events (Real-time logs)
```http
GET /api/console
```

**Response:**
```json
{
  "events": [
    {
      "timestamp": 1234567890,
      "type": "temp",
      "message": "Temp: 18.9°C"
    },
    {
      "timestamp": 1234567895,
      "type": "pid",
      "message": "PID: power=0%"
    }
  ]
}
```

### Restart Device
```http
POST /api/restart
```

---

## 6. Legacy API (Output 1 Only - Backward Compatibility)

### Set Temperature/Mode (Legacy)
```http
POST /api/set
Content-Type: application/json

{
  "temp": 22.5,
  "mode": "auto"
}
```

**Note:** This only controls Output 1. Use `/api/output/1/control` instead for new implementations.

---

## 7. Real-time Updates

### Polling Strategy
The device does **not** support WebSockets or Server-Sent Events. Use polling:

**Recommended polling intervals:**
- `/api/outputs` - Every 2-5 seconds for temperature updates
- `/api/status` - Every 10-30 seconds for system status
- `/api/console` - Every 5-10 seconds if showing live logs

**Example Kotlin polling:**
```kotlin
import kotlinx.coroutines.*

class ThermostatPoller(private val baseUrl: String) {
    private var pollingJob: Job? = null

    fun startPolling(intervalMs: Long = 3000) {
        pollingJob = CoroutineScope(Dispatchers.IO).launch {
            while (isActive) {
                try {
                    val outputs = fetchOutputs()
                    // Update UI with new data
                    withContext(Dispatchers.Main) {
                        updateUI(outputs)
                    }
                } catch (e: Exception) {
                    println("Polling error: ${e.message}")
                }
                delay(intervalMs)
            }
        }
    }

    fun stopPolling() {
        pollingJob?.cancel()
    }

    private suspend fun fetchOutputs(): OutputsResponse {
        // Use Retrofit or OkHttp to fetch /api/outputs
    }
}
```

---

## 8. MQTT Integration (Optional)

If MQTT broker is configured, the device publishes to:

**Topics:**
- `thermostat/{deviceName}/output/{1|2|3}/state` - Output state
- `thermostat/{deviceName}/output/{1|2|3}/temperature` - Current temp
- `thermostat/{deviceName}/output/{1|2|3}/target` - Target temp
- `thermostat/{deviceName}/output/{1|2|3}/power` - Power level
- `thermostat/{deviceName}/output/{1|2|3}/heating` - Heating status

**Subscribe for real-time updates instead of HTTP polling**

---

## 9. Error Handling

### HTTP Status Codes
- `200 OK` - Success
- `400 Bad Request` - Invalid parameters
- `401 Unauthorized` - Authentication required (secure mode enabled)
- `404 Not Found` - Invalid endpoint or output ID
- `500 Internal Server Error` - Device error

### Common Issues

**mDNS Discovery Issues:**
- Ensure Android device is on the same WiFi network as ESP32
- Check that multicast is enabled on router
- Some Android devices have buggy mDNS - fallback to IP scanning
- Try direct hostname resolution: `InetAddress.getByName("havoc.local")`

**Connection Issues:**
- Check ESP32 is not in AP mode (WiFi must be connected)
- Verify port 80 is accessible (no firewall blocking)
- Use `http://` not `https://` (no SSL/TLS)

**API Response Issues:**
- Always include `Content-Type: application/json` header for POST requests
- Output IDs are 1-based in URLs but 0-based in responses (id: 0, 1, 2)
- Temperature values are in Celsius (float)

---

## 10. Example Android Architecture

### Recommended Libraries
```gradle
// Networking
implementation 'com.squareup.retrofit2:retrofit:2.9.0'
implementation 'com.squareup.retrofit2:converter-gson:2.9.0'
implementation 'com.squareup.okhttp3:okhttp:4.11.0'

// Coroutines
implementation 'org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3'

// ViewModel & LiveData
implementation 'androidx.lifecycle:lifecycle-viewmodel-ktx:2.6.2'
implementation 'androidx.lifecycle:lifecycle-livedata-ktx:2.6.2'
```

### Retrofit API Interface
```kotlin
interface ThermostatApi {
    @GET("/api/outputs")
    suspend fun getOutputs(): OutputsResponse

    @GET("/api/output/{id}")
    suspend fun getOutput(@Path("id") id: Int): OutputResponse

    @POST("/api/output/{id}/control")
    suspend fun controlOutput(
        @Path("id") id: Int,
        @Body request: ControlRequest
    ): ApiResponse

    @GET("/api/status")
    suspend fun getStatus(): StatusResponse

    @GET("/api/info")
    suspend fun getInfo(): InfoResponse
}

data class ControlRequest(
    val target: Float? = null,
    val mode: String? = null,
    val manualPower: Int? = null
)
```

---

## 11. Quick Start Checklist

- [ ] Add mDNS/network permissions to AndroidManifest.xml
- [ ] Implement NsdManager service discovery for `_http._tcp.`
- [ ] Handle discovered IP address and construct base URL
- [ ] Create Retrofit interface for API endpoints
- [ ] Implement polling for `/api/outputs` (every 3-5 seconds)
- [ ] Parse JSON responses with Gson/Moshi
- [ ] Display 3 outputs with current/target temps
- [ ] Implement temperature adjustment UI (±0.5°C buttons)
- [ ] Implement mode selection (OFF, MANUAL, PID, ON/OFF)
- [ ] Add error handling for network failures
- [ ] Test with actual ESP32 device on same network

---

## 12. Testing Without mDNS

If mDNS is problematic during development:

1. **Hardcode IP address temporarily:**
   ```kotlin
   val baseUrl = "http://192.168.1.236"
   ```

2. **Or add manual IP entry field in settings**

3. **Check IP address from:**
   - ESP32 serial monitor on boot
   - Router's DHCP client list
   - Web interface at Info screen (tap 'i' button on TFT display)

---

## Contact & Support

- **Project GitHub**: https://github.com/cheew/Claude-ESP32-Thermostat
- **ESP32 Hostname**: `havoc.local` (default)
- **Default Port**: `80`
- **API Documentation**: This file

Good luck building the Android app! 🚀
