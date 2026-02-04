# ESP32-S3-WROOM-1-N8R8 Pinout & Wiring Reference

**Target Module:** ESP32-S3-WROOM-1-N8R8 (8MB Flash, 8MB PSRAM)
**Application:** Reptile Thermostat with TFT Display

---

## Module Specifications

| Parameter | Value |
|-----------|-------|
| Flash | 8MB (Quad SPI) |
| PSRAM | 8MB (Octal SPI) |
| CPU | Dual-core LX7 @ 240MHz |
| GPIO Count | 45 (36 usable) |
| USB | Native USB-OTG (GPIO 19/20) |
| Operating Voltage | 3.3V |
| WiFi | 2.4GHz 802.11 b/g/n |

---

## Pin Assignment Table

### Output Control (5 channels)

| Function | GPIO | Type | Notes |
|----------|------|------|-------|
| SSR Output 1 | **GPIO 4** | Digital OUT | Primary heat output |
| SSR Output 2 | **GPIO 5** | Digital OUT | Secondary heat |
| SSR Output 3 | **GPIO 6** | Digital OUT | Tertiary heat |
| SSR Output 4 | **GPIO 7** | Digital OUT | Auxiliary/lighting |
| AC Dimmer PWM | **GPIO 15** | PWM OUT | RobotDyn dimmer control |
| Zero-Cross Detect | **GPIO 16** | Digital IN | Interrupt-capable, pull-up |

### Temperature Sensors

| Function | GPIO | Type | Notes |
|----------|------|------|-------|
| OneWire Bus | **GPIO 17** | Bidirectional | DS18B20 sensors, 4.7kΩ pull-up to 3.3V |

### TFT Display (ILI9341 SPI)

| Function | GPIO | Type | Notes |
|----------|------|------|-------|
| SPI MOSI | **GPIO 11** | SPI | Data to display |
| SPI MISO | **GPIO 13** | SPI | Data from display (touch read) |
| SPI SCK | **GPIO 12** | SPI | Clock |
| TFT CS | **GPIO 10** | Digital OUT | Display chip select |
| TFT DC | **GPIO 9** | Digital OUT | Data/Command select |
| TFT RST | **GPIO 8** | Digital OUT | Display reset |
| TFT Backlight | **GPIO 14** | PWM OUT | Optional brightness control |

### Touch Controller (XPT2046)

| Function | GPIO | Type | Notes |
|----------|------|------|-------|
| Touch CS | **GPIO 21** | Digital OUT | Touch chip select |
| Touch IRQ | **GPIO 47** | Digital IN | Touch interrupt (optional) |

### USB Programming (Native)

| Function | GPIO | Type | Notes |
|----------|------|------|-------|
| USB D- | **GPIO 19** | USB | **DO NOT USE** - Reserved |
| USB D+ | **GPIO 20** | USB | **DO NOT USE** - Reserved |

### Status & Debug

| Function | GPIO | Type | Notes |
|----------|------|------|-------|
| Status LED | **GPIO 48** | Digital OUT | RGB LED or standard LED |
| UART TX | **GPIO 43** | UART | Debug serial (optional) |
| UART RX | **GPIO 44** | UART | Debug serial (optional) |

---

## Strapping Pins - CRITICAL

These pins have specific requirements at boot. **Do not pull low/high incorrectly!**

| GPIO | Function at Boot | Safe Usage |
|------|------------------|------------|
| GPIO 0 | Boot mode select | Pull-up (default boot), LOW = download mode |
| GPIO 3 | JTAG select | Leave floating or pull-up |
| GPIO 45 | VDD_SPI voltage | **Must be LOW** for 3.3V flash |
| GPIO 46 | Boot mode | Pull-down or floating (ROM messages) |

**Recommendation:** Avoid using GPIO 0, 3, 45, 46 for outputs.

---

## Pins to AVOID

| GPIO | Reason |
|------|--------|
| 19, 20 | USB D-/D+ (reserved for programming) |
| 26-32 | Used internally for PSRAM (N8R8 variant) |
| 33-37 | Used internally for Octal Flash/PSRAM |
| 0, 3, 45, 46 | Strapping pins |

---

## EasyEDA Schematic Notes

### Power Supply Section
```
USB 5V ──┬── AMS1117-3.3 ──┬── 3.3V Rail
         │                 │
        10µF              10µF + 100nF
         │                 │
        GND               GND

ESP32-S3 Pins:
- VDD (3V3): Connect to 3.3V rail
- GND: Common ground
- EN: 10kΩ pull-up to 3.3V + 100nF to GND (RC delay)
```

### USB Connection
```
USB-C Connector:
- VBUS → 5V input (with protection diode)
- D- → GPIO 19 (direct, no resistors needed)
- D+ → GPIO 20 (direct, no resistors needed)
- GND → Common ground
- CC1/CC2 → 5.1kΩ to GND each (for USB-C)
```

### SSR Output Circuit (per channel)
```
GPIO ──[330Ω]──┬── LED+ (optocoupler)
               │
              LED- ── GND

Optocoupler recommendation: PC817 or MOC3021 (for AC)
SSR: Fotek SSR-25DA or similar (DC control, AC load)
```

### AC Dimmer Connection (RobotDyn)
```
ESP32-S3          RobotDyn Dimmer
─────────         ───────────────
GPIO 15  ────────  PWM
GPIO 16  ────────  Z-C (zero cross)
3.3V     ────────  VCC
GND      ────────  GND
```

### OneWire Bus (DS18B20)
```
        3.3V
         │
        4.7kΩ
         │
GPIO 17 ─┴─── DATA (yellow) ── DS18B20 #1 ── DS18B20 #2 ── DS18B20 #3
              VDD  (red)   ─── 3.3V
              GND  (black) ─── GND
```

### TFT Display Wiring (ILI9341 + XPT2046)
```
ESP32-S3          Display Module
─────────         ──────────────
GPIO 11  ────────  MOSI (SDI)
GPIO 13  ────────  MISO (SDO)
GPIO 12  ────────  SCK (CLK)
GPIO 10  ────────  TFT_CS
GPIO 9   ────────  DC (RS)
GPIO 8   ────────  RST
GPIO 14  ────────  LED (backlight, via transistor for PWM)
GPIO 21  ────────  T_CS (touch)
GPIO 47  ────────  T_IRQ (optional)
3.3V     ────────  VCC
GND      ────────  GND
```

---

## Complete GPIO Summary

| GPIO | Assignment | Direction |
|------|------------|-----------|
| 4 | SSR Output 1 | OUT |
| 5 | SSR Output 2 | OUT |
| 6 | SSR Output 3 | OUT |
| 7 | SSR Output 4 | OUT |
| 8 | TFT RST | OUT |
| 9 | TFT DC | OUT |
| 10 | TFT CS | OUT |
| 11 | SPI MOSI | OUT |
| 12 | SPI SCK | OUT |
| 13 | SPI MISO | IN |
| 14 | TFT Backlight | PWM OUT |
| 15 | Dimmer PWM | PWM OUT |
| 16 | Zero-Cross | IN (interrupt) |
| 17 | OneWire Bus | I/O |
| 19 | USB D- | **RESERVED** |
| 20 | USB D+ | **RESERVED** |
| 21 | Touch CS | OUT |
| 43 | UART TX (debug) | OUT |
| 44 | UART RX (debug) | IN |
| 47 | Touch IRQ | IN |
| 48 | Status LED | OUT |

**Total used:** 18 GPIO (+ 2 USB reserved)
**Available for expansion:** GPIO 1, 2, 18, 38, 39, 40, 41, 42

---

## Decoupling Capacitors

Place close to ESP32-S3 module:

| Location | Capacitor |
|----------|-----------|
| VDD (3V3) pin | 100nF ceramic |
| VDD (3V3) pin | 10µF electrolytic |
| EN pin | 100nF to GND |
| Each VDD pin | 100nF ceramic |

---

## EasyEDA Component Suggestions

| Component | LCSC Part # | Notes |
|-----------|-------------|-------|
| ESP32-S3-WROOM-1-N8R8 | C2913202 | Main module |
| AMS1117-3.3 | C6186 | 3.3V LDO |
| USB-C Receptacle | C165948 | 16-pin mid-mount |
| PC817 Optocoupler | C66580 | SSR isolation |
| 4.7kΩ 0805 | C17673 | OneWire pull-up |
| 10kΩ 0805 | C17414 | EN pull-up |
| 330Ω 0805 | C17630 | LED current limit |
| 5.1kΩ 0805 | C23186 | USB-C CC resistors |
| 100nF 0805 | C49678 | Decoupling |
| 10µF 0805 | C19702 | Bulk decoupling |

---

## Migration from ESP32-WROOM-32

| Old Pin | Old Function | New Pin | Notes |
|---------|--------------|---------|-------|
| GPIO 4 | OneWire | GPIO 17 | Moved to avoid conflicts |
| GPIO 5 | Dimmer | GPIO 15 | Remapped |
| GPIO 27 | Zero-cross | GPIO 16 | Remapped |
| GPIO 18 | SPI SCK | GPIO 12 | New SPI bus |
| GPIO 19 | SPI MISO | GPIO 13 | 19 is now USB |
| GPIO 23 | SPI MOSI | GPIO 11 | New SPI bus |
| GPIO 15 | TFT CS | GPIO 10 | Remapped |
| GPIO 2 | TFT DC | GPIO 9 | Remapped |
| GPIO 33 | TFT RST | GPIO 8 | Remapped |
| GPIO 22 | Touch CS | GPIO 21 | Remapped |
| GPIO 25 | Light Dimmer | — | Use SSR Output 4 instead |
| — | SSR Outputs | GPIO 4-7 | **NEW: 4 channels** |

---

## Code Changes Required

Update `include/config.h`:

```cpp
// Temperature Sensor (DS18B20)
#define ONE_WIRE_BUS 17

// SSR Outputs (accent heat, secondary heat, etc.)
#define SSR_OUTPUT_1_PIN 4
#define SSR_OUTPUT_2_PIN 5
#define SSR_OUTPUT_3_PIN 6
#define SSR_OUTPUT_4_PIN 7

// AC Dimmer (RobotDyn)
#define DIMMER_HEAT_PIN 15
#define ZEROCROSS_PIN 16

// TFT Display (ILI9341)
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_SCK 12
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8
#define TFT_BL 14

// Touch Controller
#define TOUCH_CS 21
#define TOUCH_IRQ 47

// Status LED
#define STATUS_LED_PIN 48
```

Update `platformio.ini`:

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.mcu = esp32s3
board_build.flash_mode = qio
board_build.psram = enabled
board_upload.flash_size = 8MB
monitor_speed = 115200

build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

---

## Prototype Wiring Checklist

- [ ] 3.3V power supply stable (measure with multimeter)
- [ ] EN pin has RC circuit (10kΩ + 100nF)
- [ ] USB D+/D- connected directly (no resistors)
- [ ] USB-C CC pins have 5.1kΩ to GND
- [ ] OneWire has 4.7kΩ pull-up
- [ ] All SSR outputs have current-limiting resistors
- [ ] SPI bus connections verified (MOSI/MISO not swapped)
- [ ] Decoupling caps placed near module
- [ ] GPIO 45 pulled LOW (for 3.3V flash)
