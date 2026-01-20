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
//   --- NVR/HVR Configuration ---
//   1. Go to Camera Management -> Add Camera
//   2. Protocol: ONVIF
//   3. Management Port: 8000 (Check ONVIF_PORT below)
//   4. Channel Port: 1
//   5. User: admin (or value of WEB_USER)
//   6. Password: <Your Password> (value of WEB_PASS)
//   7. NOTE: If "Offline (Parameter Error)", check Time Sync.
// ==============================================================================

// ==============================================================================
//   STEP 1: SELECT YOUR BOARD
// ==============================================================================
// Uncomment ONE of the following board definitions based on your hardware.
// This sets the correct camera pins automatically.

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
// #define BOARD_ESP32P4_FUNCTION_EV  // ESP32-P4-Function-EV-Board (H.264 HW Encoder!)
// #define BOARD_CUSTOM               // Custom board - define pins in board_config.h

// ==============================================================================
//   STEP 2: SELECT VIDEO CODEC
// ==============================================================================
// Choose your video encoding format. H.264 requires ESP32-P4 or ESP32-S3.

#define VIDEO_CODEC_MJPEG             // MJPEG - Works on ALL ESP32 boards (default)
// #define VIDEO_CODEC_H264           // H.264 - ESP32-P4 (HW) or ESP32-S3 (SW) only!

// H.264 Encoding Settings (only used if VIDEO_CODEC_H264 is defined)
#ifdef VIDEO_CODEC_H264
    // Encoder type: AUTO, HARDWARE (P4 only), or SOFTWARE (S3)
    #define H264_ENCODER_AUTO         // Auto-detect (recommended)
    // #define H264_ENCODER_HARDWARE  // Force HW encoder (ESP32-P4 only, 30fps 1080p)
    // #define H264_ENCODER_SOFTWARE  // Force SW encoder (ESP32-S3, ~17fps 320x192)
    
    #define H264_GOP            30      // Keyframe interval (1 = all I-frames, 30 = 1/sec at 30fps)
    #define H264_FPS            25      // Target framerate
    #define H264_BITRATE        2000000 // Target bitrate in bps (2 Mbps)
    #define H264_QP_MIN         20      // Minimum QP (lower = better quality, more bandwidth)
    #define H264_QP_MAX         35      // Maximum QP (higher = worse quality, less bandwidth)
    
    // Memory warning for ESP32-S3 Software Encoder
    // SW encoder needs ~1MB RAM. Limit resolution to prevent crashes.
    #define H264_SW_MAX_WIDTH   640     // Max width for SW encoder
    #define H264_SW_MAX_HEIGHT  480     // Max height for SW encoder
#endif

// ==============================================================================

// --- WiFi Settings ---
// Set these to your local network credentials for automatic connection
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// --- AP Mode Settings ---
// Fallback access point if WiFi connection fails
#define AP_SSID         "ESP32CAM-ONVIF"
#define AP_PASSWORD     "esp32cam"      // Min 8 characters

// --- Security ---
// Credentials for the Web Configuration Interface
#define WEB_USER        "admin"
#define WEB_PASS        "esp123"

// --- Static IP Settings (Optional) ---
#define STATIC_IP_ENABLED   true       // Set to true to use Static IP
#define STATIC_IP_ADDR      192,168,0,150
#define STATIC_GATEWAY      192,168,0,1
#define STATIC_SUBNET       255,255,255,0
#define STATIC_DNS          8,8,8,8

// --- Server Ports ---
#define WEB_PORT        80              // Web configuration interface
#define RTSP_PORT       554             // RTSP Streaming port (standard: 554)
#define ONVIF_PORT      8000            // ONVIF Service port (standard: 80, 8000, or 8080)
#define DEFAULT_ONVIF_ENABLED true      // Enable ONVIF service by default

// --- Flash LED Settings ---
// WARNING: Flash LED pins are board-specific and defined in board_config.h.
// GPIO 4 is standard for AI-Thinker ESP32-CAM but conflicts with SD Card Data 1.
// If FLASH_LED_ENABLED is true, SD card MUST use 1-bit mode on AI-Thinker boards.
// DISABLED BY DEFAULT to prevent pin conflicts on unsupported boards.
#define FLASH_LED_ENABLED false         // Set to true ONLY if your board supports flash LED
#define FLASH_LED_INVERT false          // false = High is ON (override in board_config.h if needed)
#define DEFAULT_AUTO_FLASH false        // Auto-flash disabled by default
// Note: FLASH_LED_PIN is defined per-board in board_config.h

// --- Status LED Settings ---
// WARNING: Status LED pins are board-specific and defined in board_config.h.
// DISABLED BY DEFAULT to prevent pin conflicts on unsupported boards.
#define STATUS_LED_ENABLED false        // Set to true ONLY if your board has a status LED
#define STATUS_LED_INVERT  true         // true = Low is ON (override in board_config.h if needed)
// Note: STATUS_LED_PIN is defined per-board in board_config.h

// --- PTZ (Servo) Settings ---
// Optional: Connect servos for Pan/Tilt control
// WARNING: Servo pins are board-specific. Ensure pins don't conflict with camera/SD/reset.
#define PTZ_ENABLED       false         // Set to true to enable servo control
// Note: SERVO_PAN_PIN and SERVO_TILT_PIN are defined per-board in board_config.h

// --- Recording Settings ---
#define ENABLE_DAILY_RECORDING  false   // If true, records continuously (loop overwrite)
#define RECORD_SEGMENT_SEC      300     // 5 minutes per file
#define MAX_DISK_USAGE_PCT      90      // Auto-delete oldest files if disk usage > 90%
#define ENABLE_MOTION_DETECTION false   // Disabled to save IRAM (enable if not using Bluetooth)

// --- Bluetooth Settings (Compile-Time) ---
// WARNING: Bluetooth + WiFi + Camera is VERY heavy on IRAM (instruction memory).
// BLUETOOTH_ENABLED is DISABLED by default to ensure the firmware compiles.
// 
// Bluetooth Stack Selection (Automatic):
//   - ESP32 Classic: Uses NimBLE (lightweight, BLE-only, ~1.2KB IRAM)
//   - ESP32-S3/P4: Uses Bluedroid (full-featured, BLE+Classic, ~2.2KB IRAM)
//
// To enable Bluetooth features (Presence Detection, Stealth Mode, Audio):
//   1. Uncomment the line below
//   2. For ESP32 Classic: Set Partition Scheme to "Huge APP (3MB No OTA/1MB SPIFFS)"
//   3. Recompile
// #define BLUETOOTH_ENABLED     // Uncomment to enable Bluetooth features

// Auto-detect Bluetooth stack based on chip (Arduino IDE compatible)
#ifdef BLUETOOTH_ENABLED
  #if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
    #define USE_BLUEDROID     // S3 → Bluedroid (full features, more RAM)
  #elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP32C3)
    #define USE_BLUEDROID     // C3 → Bluedroid
  #elif defined(CONFIG_IDF_TARGET_ESP32P4) || defined(ESP32P4)
    #define USE_BLUEDROID     // P4 → Bluedroid
  #else
    #define USE_NIMBLE        // ESP32 Classic → NimBLE (lighter)
  #endif
#endif

// --- Device Information (ONVIF) ---
// These appear in your DVR/NVR during discovery
#define DEVICE_MANUFACTURER "John-Varghese-EH"
#define DEVICE_MODEL        "ESP32-CAM-ONVIF"
#define DEVICE_VERSION      "1.0"
#define DEVICE_HARDWARE_ID  "ESP32CAM-J0X"

// --- Time Settings ---
#define NTP_SERVER      "pool.ntp.org"
#define GMT_OFFSET_SEC  19800           // India (UTC+5:30) = 5.5 * 3600 = 19800
#define DAYLIGHT_OFFSET 0               // No DST in India

// --- Debugging ---
#define DEBUG_MODE      false           // Set to false to disable all serial output (saves CPU)
#define DEBUG_LEVEL     0               // 1=Errors Only, 2=Info/Actions, 3=Verbose (Parsing)

#if DEBUG_MODE
    #define LOG_E(x) Serial.println("[ERROR] " + String(x))
    #define LOG_I(x) if(DEBUG_LEVEL>=2) Serial.println("[INFO] " + String(x))
    #define LOG_D(x) if(DEBUG_LEVEL>=3) Serial.println("[DEBUG] " + String(x))
#else
    #define LOG_E(x)
    #define LOG_I(x)
    #define LOG_D(x)
#endif

// --- Helper Functions ---
void printBanner();
void fatalError(const char* msg);

// --- Runtime Settings (Persisted to SPIFFS) ---
enum AudioSource {
    AUDIO_SOURCE_NONE = 0,
    AUDIO_SOURCE_HARDWARE_I2S = 1,
    AUDIO_SOURCE_BLUETOOTH_HFP = 2
};

struct AppSettings {
    bool btEnabled;
    bool btStealthMode;
    String btPresenceMac;
    int btPresenceTimeout; // Seconds
    int btMicGain;        // 0-100
    int hwMicGain;        // 0-100
    AudioSource audioSource;
};

extern AppSettings appSettings;
void loadSettings();
void saveSettings();
