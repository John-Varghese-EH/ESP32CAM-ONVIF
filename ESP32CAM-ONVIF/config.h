#pragma once
#include <Arduino.h>

// ==============================================================================
//   ESP32-CAM ONVIF/RTSP/HVR Configuration
// ==============================================================================
//   --- Project Attribution ---
//   GitHub: John-Varghese-EH
//   Project: ESP32-CAM ONVIF/RTSP/HVR
//   Version: 1.3 (Multi-Board + H.264 Support)
//
//   --- Supported Boards ---
//   ESP32-CAM (AI-Thinker), M5Stack Camera, TTGO T-Camera, Freenove ESP32-S3,
//   Seeed XIAO ESP32S3 Sense, ESP-EYE, ESP32-P4-Function-EV, and more!
//
//   --- Quick Start Guide ---
//   1. Select your board in SECTION 1 below
//   2. Choose video codec (MJPEG or H.264) in SECTION 2
//   3. Configure WiFi credentials in SECTION 3
//   4. Upload and enjoy!
//
//   --- Feature Summary ---
//   ✅ ONVIF Profile S compliant
//   ✅ RTSP streaming (554)
//   ✅ Web configuration UI (80)
//   ✅ WiFi Manager with AP fallback
//   ✅ SD Card recording (optional)
//   ✅ Bluetooth audio & presence detection (optional, compile-time)
//   ✅ PTZ servo control (optional)
//   ✅ Motion detection (optional)
//   ✅ Auto flash for night vision (optional)
// ==============================================================================


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 1: BOARD SELECTION [CRITICAL - SELECT ONE]                     ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
// Uncomment ONLY ONE board definition. This automatically configures GPIO pins.
// Pin mappings are defined in board_config.h

#define BOARD_AI_THINKER_ESP32CAM     // Most common ESP32-CAM (with OV2640)
// #define BOARD_M5STACK_CAMERA       // M5Stack ESP32 Camera Module
// #define BOARD_M5STACK_PSRAM        // M5Stack Camera with PSRAM
// #define BOARD_M5STACK_WIDE         // M5Stack Wide Camera
// #define BOARD_M5STACK_UNITCAM      // M5Stack Unit Cam
// #define BOARD_TTGO_T_CAMERA        // TTGO T-Camera with OLED
// #define BOARD_TTGO_T_JOURNAL       // TTGO T-Journal
// #define BOARD_WROVER_KIT           // ESP-WROVER-KIT
// #define BOARD_ESP_EYE              // ESP-EYE from Espressif
// #define BOARD_FREENOVE_ESP32S3     // Freenove ESP32-S3-WROOM CAM Board
// #define BOARD_SEEED_XIAO_S3        // Seeed XIAO ESP32S3 Sense
// #define BOARD_ESP32S3_EYE          // ESP32-S3-EYE from Espressif
// #define BOARD_ESP32P4_FUNCTION_EV  // ESP32-P4-Function-EV-Board (H.264 HW!)
// #define BOARD_CUSTOM               // Custom board - define pins in board_config.h


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 2: VIDEO CODEC SELECTION [CRITICAL]                            ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
// Choose between MJPEG (all boards) or H.264 (ESP32-P4/S3 only)

#define VIDEO_CODEC_MJPEG             // MJPEG - Works on ALL ESP32 boards (default)
// #define VIDEO_CODEC_H264           // H.264 - ESP32-P4 (HW) or ESP32-S3 (SW) only!

// --- H.264 Encoding Settings (only used if VIDEO_CODEC_H264 is defined) ---
#ifdef VIDEO_CODEC_H264
    // Encoder type: AUTO (recommended), HARDWARE (P4 only), or SOFTWARE (S3)
    #define H264_ENCODER_AUTO         // Auto-detect based on chip
    // #define H264_ENCODER_HARDWARE  // Force HW encoder (ESP32-P4: 30fps @ 1080p)
    // #define H264_ENCODER_SOFTWARE  // Force SW encoder (ESP32-S3: ~17fps @ 320x192)
    
    // Encoding parameters
    #define H264_GOP            30      // Keyframe interval (30 = 1 I-frame/sec at 30fps)
    #define H264_FPS            25      // Target framerate
    #define H264_BITRATE        2000000 // Target bitrate in bps (2 Mbps)
    #define H264_QP_MIN         20      // Min quantization (lower = better quality)
    #define H264_QP_MAX         35      // Max quantization (higher = worse quality)
    
    // ESP32-S3 Software Encoder Memory Limits
    // SW encoder needs ~1MB RAM. Limit resolution to prevent crashes.
    #define H264_SW_MAX_WIDTH   640     // Max width for SW encoder
    #define H264_SW_MAX_HEIGHT  480     // Max height for SW encoder
#endif


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 3: NETWORK CONFIGURATION [REQUIRED]                            ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

// --- WiFi Station Mode Settings ---
// Set these to your local network for automatic connection
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// --- Access Point (AP) Fallback Settings ---
// If WiFi connection fails, device creates its own network
#define AP_SSID         "ESP32CAM-ONVIF"
#define AP_PASSWORD     "esp32cam"      // Minimum 8 characters

// --- Static IP Configuration [OPTIONAL] ---
// Set STATIC_IP_ENABLED to false to use DHCP (automatic IP)
#define STATIC_IP_ENABLED   false       // Set to true for static IP
#define STATIC_IP_ADDR      192,168,0,150
#define STATIC_GATEWAY      192,168,0,1
#define STATIC_SUBNET       255,255,255,0
#define STATIC_DNS          8,8,8,8

// --- Network Service Ports ---
#define WEB_PORT        80              // Web configuration interface
#define RTSP_PORT       554             // RTSP Streaming (standard: 554)
#define ONVIF_PORT      8000            // ONVIF Device Management (standard: 80/8000/8080)


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 4: SECURITY & AUTHENTICATION [REQUIRED]                        ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
// Credentials for Web UI, ONVIF, and RTSP access

#define WEB_USER        "admin"
#define WEB_PASS        "esp123"

// ⚠️ SECURITY WARNING: Change default credentials before deploying!


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 5: CAMERA & VIDEO SETTINGS [OPTIONAL]                          ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
// Default camera settings (can be changed via Web UI at runtime)

// Resolution: FRAMESIZE_UXGA (1600x1200), FRAMESIZE_HD (1280x720), 
//             FRAMESIZE_VGA (640x480), FRAMESIZE_QVGA (320x240)
// Quality: 4-63 (lower = better quality, larger files, more bandwidth)
// These are initial defaults only - adjustable in real-time via Web UI

// Note: Settings are handled by esp_camera library at runtime


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 6: RECORDING & STORAGE [OPTIONAL]                              ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

#define ENABLE_DAILY_RECORDING  false   // Continuous recording (loop overwrite)
#define RECORD_SEGMENT_SEC      300     // 5 minutes per file (300 seconds)
#define MAX_DISK_USAGE_PCT      90      // Auto-delete oldest when disk > 90%
#define ENABLE_MOTION_DETECTION false   // Motion-triggered recording (saves IRAM when disabled)

// 💡 TIP: Enable motion detection only if you're NOT using Bluetooth to save memory


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 7: HARDWARE FEATURES [OPTIONAL]                                ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

// --- Flash LED Settings ---
// ⚠️ WARNING: GPIO 4 (AI-Thinker) conflicts with SD Card Data1 line.
//   - If FLASH_LED_ENABLED = true, SD card MUST use 1-bit mode
//   - DISABLED by default for maximum compatibility
#define FLASH_LED_ENABLED false         // Enable flash LED (set true if supported)
#define FLASH_LED_INVERT  false         // false = High is ON (board-specific)
#define DEFAULT_AUTO_FLASH false        // Auto-enable flash in low light
// Note: FLASH_LED_PIN is defined per-board in board_config.h

// --- Status LED Settings ---
// ⚠️ WARNING: Not all boards have a status LED. Check your board schematic.
//   - DISABLED by default to prevent conflicts
#define STATUS_LED_ENABLED false        // Enable status LED (set true if supported)
#define STATUS_LED_INVERT  true         // true = Low is ON (common anode LED)
// Note: STATUS_LED_PIN is defined per-board in board_config.h

// --- PTZ (Pan-Tilt-Zoom) Servo Control ---
// Optional: Connect servos for remote camera positioning
// ⚠️ WARNING: Ensure servo pins don't conflict with camera/SD/reset pins
#define PTZ_ENABLED       false         // Enable servo control
#define PTZ_PAN_MIN       -90           // Pan servo minimum angle (degrees)
#define PTZ_PAN_MAX       90            // Pan servo maximum angle (degrees)
#define PTZ_TILT_MIN      -45           // Tilt servo minimum angle (degrees)
#define PTZ_TILT_MAX      45            // Tilt servo maximum angle (degrees)
// Note: SERVO_PAN_PIN and SERVO_TILT_PIN defined in board_config.h


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 8: BLUETOOTH & AUDIO [OPTIONAL - COMPILE TIME]                 ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

// ⚠️ CRITICAL: Bluetooth + WiFi + Camera is VERY IRAM-intensive!
// BLUETOOTH_ENABLED is DISABLED by default to ensure firmware compiles.
//
// Bluetooth Stack Selection (Automatic):
//   - ESP32 Classic: NimBLE (lightweight, BLE-only, ~1.2KB IRAM)
//   - ESP32-S3/P4:   Bluedroid (full-featured, BLE+Classic, ~2.2KB IRAM)
//
// Features when enabled:
//   ✅ Presence Detection - Detect if you're home via phone's Bluetooth MAC
//   ✅ Stealth Mode - Auto-disable LEDs when WiFi lost AND user absent
//   ✅ Bluetooth Audio (HFP) - Use BT mic/headset as wireless microphone
//
// To enable Bluetooth:
//   1. Uncomment the line below
//   2. For ESP32 Classic: Set Partition Scheme to "Huge APP (3MB No OTA/1MB SPIFFS)"
//      Arduino IDE: Tools → Partition Scheme → Huge APP (3MB No OTA/1MB SPIFFS)
//   3. Recompile and upload

// #define BLUETOOTH_ENABLED     // ← Uncomment to enable Bluetooth features

// Auto-detect Bluetooth stack based on chip
#ifdef BLUETOOTH_ENABLED
  #if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
    #define USE_BLUEDROID     // ESP32-S3 → Bluedroid (more features, more RAM)
  #elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP32C3)
    #define USE_BLUEDROID     // ESP32-C3 → Bluedroid
  #elif defined(CONFIG_IDF_TARGET_ESP32P4) || defined(ESP32P4)
    #define USE_BLUEDROID     // ESP32-P4 → Bluedroid
  #else
    #define USE_NIMBLE        // ESP32 Classic → NimBLE (lighter)
  #endif
#endif


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 9: ONVIF & NVR/DVR COMPATIBILITY                               ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
// Settings for NVR/HVR integration

#define DEFAULT_ONVIF_ENABLED true      // Enable ONVIF service by default

// --- Device Information (shown in DVR/NVR during discovery) ---
#define DEVICE_MANUFACTURER "John-Varghese-EH"
#define DEVICE_MODEL        "ESP32-CAM-ONVIF"
#define DEVICE_VERSION      "1.3"
#define DEVICE_HARDWARE_ID  "ESP32CAM-J0X"

// --- NVR Configuration Guide ---
// For Hikvision/Dahua/Unifi/Blue Iris:
//   1. Protocol: ONVIF
//   2. Management Port: 8000 (ONVIF_PORT above)
//   3. Channel Port: 1
//   4. User: admin (WEB_USER)
//   5. Password: esp123 (WEB_PASS)
//   6. If "Offline (Parameter Error)" → Check time sync, use TCP transport


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 10: ADVANCED SETTINGS                                          ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

// --- Time Synchronization ---
#define NTP_SERVER      "pool.ntp.org"
#define GMT_OFFSET_SEC  19800           // India (UTC+5:30) = 5.5 * 3600 = 19800
#define DAYLIGHT_OFFSET 0               // Daylight saving offset (seconds)

// --- Debugging & Serial Output ---
#define DEBUG_MODE      false           // Set true to enable serial debug output
#define DEBUG_LEVEL     0               // 1=Errors, 2=Info/Actions, 3=Verbose

#if DEBUG_MODE
    #define LOG_E(x) Serial.println("[ERROR] " + String(x))
    #define LOG_I(x) if(DEBUG_LEVEL>=2) Serial.println("[INFO] " + String(x))
    #define LOG_D(x) if(DEBUG_LEVEL>=3) Serial.println("[DEBUG] " + String(x))
#else
    #define LOG_E(x)
    #define LOG_I(x)
    #define LOG_D(x)
#endif


// ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
// ┃ SECTION 11: RUNTIME SETTINGS STRUCTURE                                 ┃
// ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
// These settings are stored in SPIFFS and persist across reboots
// Modified via Web UI, not typically changed in code

enum AudioSource {
    AUDIO_SOURCE_NONE = 0,
    AUDIO_SOURCE_HARDWARE_I2S = 1,
    AUDIO_SOURCE_BLUETOOTH_HFP = 2
};

struct AppSettings {
    bool btEnabled;
    bool btStealthMode;
    String btPresenceMac;
    int btPresenceTimeout;      // Seconds
    int btMicGain;             // 0-100
    int hwMicGain;             // 0-100
    AudioSource audioSource;
};

// --- Helper Functions ---
void printBanner();
void fatalError(const char* msg);

extern AppSettings appSettings;
void loadSettings();
void saveSettings();

// ==============================================================================
//   END OF CONFIGURATION
// ==============================================================================
