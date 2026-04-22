/*
  ESP32-CAM Advanced ONVIF + RTSP + WebConfig + SD + Motion Detection

  Features:
  - ONVIF WS-Discovery responder + minimal SOAP device service
  - RTSP MJPEG streaming on port 554
  - Basic web server on port 80 for configuration placeholder
  - SD card initialization for recording (expand as needed)
  - Basic motion detection stub
  
  Made with ❤️ by J0X
*/

#include "camera_control.h"
#include "rtsp_server.h"
#include "onvif_server.h"
#include "web_config.h"
#include "sd_recorder.h"
#include "motion_detection.h"
#include "config.h"
#include "wifi_manager.h"
#include "serial_console.h"
#include "auto_flash.h"
#include "status_led.h"
#include "mqtt_manager.h"
#ifdef BLUETOOTH_ENABLED
  #include "bluetooth_manager.h"
#endif
#include <esp_task_wdt.h>
#include <Preferences.h>

#define WDT_TIMEOUT 30 // 30 seconds hardware watchdog

Preferences preferences;

// Task Handles
TaskHandle_t rtspTaskHandle = NULL;
TaskHandle_t onvifTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t wdtTaskHandle = NULL;
TaskHandle_t lowPrioTaskHandle = NULL;

// Task Declarations
void rtsp_stream_task(void *pvParameters);
void onvif_http_task(void *pvParameters);
void wifi_mgmt_task(void *pvParameters);
void watchdog_task(void *pvParameters);
void low_prio_task(void *pvParameters);

TaskHandle_t mqttTaskHandle = NULL;

void mqtt_task(void* pvParam) {
    esp_task_wdt_add(NULL);
    while (appSettings.mqttEnabled) {
        esp_task_wdt_reset();
        if (WiFi.status() == WL_CONNECTED) {
            handle_mqtt();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    // Self-delete if disabled at runtime to recover RAM
    mqttTaskHandle = NULL;
    vTaskDelete(NULL);
}

void setup() {
  // Production speed: 115200. Debug output minimized via platformio.ini
  Serial.begin(115200);
  Serial.setDebugOutput(false); 
  
  Serial.println("\n\n--- ESP32-CAM STARTING ---");
  
  // Initialize NVS for boot counter
  preferences.begin("system", false);
  uint32_t bootCount = preferences.getUInt("boot_count", 0);
  
  // Basic boot crash loop detection
  bootCount++;
  preferences.putUInt("boot_count", bootCount);
  
  // Check reset reason for WDT
  esp_reset_reason_t reset_reason = esp_reset_reason();
  if (reset_reason == ESP_RST_WDT || reset_reason == ESP_RST_TASK_WDT) {
      Serial.println("[CRITICAL] System recovered from Watchdog Reset!");
  }

  // Load Runtime Settings
  loadSettings();
  
  // Initialize Watchdog - Version-agnostic code for both v3.x and pre-v3 board managers
  esp_task_wdt_deinit();                  // ensure a watchdog is not already configured
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR == 3  
    // v3 board manager detected (ESP32 Arduino v3.0+ / IDF v5.x)
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000, // Convert seconds to milliseconds
        .idle_core_mask = (1 << 0) | (1 << 1), // Subscribe idle task on core 0 and 1
        .trigger_panic = true             // Enable panic
    };
    esp_task_wdt_init(&wdt_config);       // Pass the pointer to the configuration structure
  #else
    // pre v3 board manager assumed (ESP32 Arduino v2.x and earlier)
    esp_task_wdt_init(WDT_TIMEOUT, true);                      //enable panic so ESP32 restarts
  #endif
  Serial.println("[INFO] WDT Enabled");
  printBanner();
  
  // Initialize camera
  if (!camera_init()) fatalError("Camera init failed!");
  
  // Initialize WiFi - try stored credentials first, fallback to AP mode
  bool wifiConnected = wifiManager.begin();
  
  // Start web server (works in both AP and STA mode)
  web_config_start();
  
  // Only start these services if we're connected to a network
  if (wifiConnected) {
    rtsp_server_start();
    onvif_server_start();
    
    // Start Time Snyc
    configTzTime(appSettings.timeZone, appSettings.ntpServer);
    Serial.println("[INFO] NTP Time Sync started");
  } else {
    Serial.println("[INFO] WiFi not connected. RTSP/ONVIF disabled.");
    Serial.println("[INFO] Connect to AP: " + wifiManager.getSSID());
    Serial.println("[INFO] Browse to: http://" + wifiManager.getLocalIP().toString());
  }
  
  // Initialize other services (once)
  sd_recorder_init();
  motion_detection_init();
  auto_flash_init(); 
  status_led_init();
  status_led_flash(1); 
  
  // Bluetooth (after loadSettings)
  #ifdef BLUETOOTH_ENABLED
    btManager.begin();
  #endif
  
  init_mqtt();

  if(wifiConnected) {
      status_led_connected(); 
  }
  else status_led_wifi_connecting();
  
  Serial.println("[INFO] System Ready.");
  Serial.printf("[INFO] Free Heap: %u bytes | PSRAM: %u bytes\n", ESP.getFreeHeap(), ESP.getFreePsram());
}

void loop() {
  // Feed Watchdog
  esp_task_wdt_reset();
  uint32_t now = millis();
  
  // === HIGH PRIORITY — every cycle (~0ms target latency) ===
  rtsp_server_loop();   // Highest priority for streaming
  web_config_loop();    // Web UI
  
  // === MEDIUM PRIORITY — every 100ms ===
  static uint32_t lastMedium = 0;
  if (now - lastMedium > 100) {
      lastMedium = now;
      wifiManager.loop();   // Connectivity
      onvif_server_loop();  // Discovery/SOAP
  }
  
  // === LOW PRIORITY — every 500ms ===
  static uint32_t lastLow = 0;
  if (now - lastLow > 500) {
      lastLow = now;
      motion_detection_loop();
      sd_recorder_loop();
      serial_console_loop();
      auto_flash_loop();
      status_led_loop();
      #ifdef BLUETOOTH_ENABLED
        btManager.loop();
      #endif
  }
  
  yield();
}
