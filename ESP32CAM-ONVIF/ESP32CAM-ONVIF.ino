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
#ifdef BLUETOOTH_ENABLED
  #include "bluetooth_manager.h"
#endif
#include <esp_task_wdt.h>

#define WDT_TIMEOUT 30 // 30 seconds hardware watchdog

void setup() {
  // Production speed: 115200. Debug output minimized via platformio.ini
  Serial.begin(115200);
  Serial.setDebugOutput(false); 
  
  Serial.println("\n\n--- ESP32-CAM STARTING ---");
  
  // Load Runtime Settings
  loadSettings();
  
  // Initialize Watchdog - Version-agnostic code for both v3.x and pre-v3 board managers
  esp_task_wdt_deinit();                  // ensure a watchdog is not already configured
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR == 3  
    // v3 board manager detected (ESP32 Arduino v3.0+ / IDF v5.x)
    // Create and initialize the watchdog timer(WDT) configuration structure
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000, // Convert seconds to milliseconds
        .idle_core_mask = (1 << 0) | (1 << 1), // Subscribe idle task on core 0 and 1
        .trigger_panic = true             // Enable panic
    };
    // Initialize the WDT with the configuration structure
    esp_task_wdt_init(&wdt_config);       // Pass the pointer to the configuration structure
    esp_task_wdt_add(NULL);               // Add current thread to WDT watch    
    esp_task_wdt_reset();                 // reset timer
  #else
    // pre v3 board manager assumed (ESP32 Arduino v2.x and earlier)
    esp_task_wdt_init(WDT_TIMEOUT, true);                      //enable panic so ESP32 restarts
    esp_task_wdt_add(NULL);                                    //add current thread to WDT watch   
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
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);
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
  
  if(wifiConnected) {
      status_led_connected(); 
  }
  else status_led_wifi_connecting();
  
  Serial.println("[INFO] System Ready.");
}

void loop() {
  // Feed Watchdog
  esp_task_wdt_reset();
  
  // Critical Loops (Keep minimal blocking)
  rtsp_server_loop();   // Highest priority for streaming
  wifiManager.loop();   // Connectivity
  web_config_loop();    // Web UI
  onvif_server_loop();  // Discovery/SOAP
  
  // Background Tasks
  motion_detection_loop();
  sd_recorder_loop();
  serial_console_loop();
  auto_flash_loop();
  status_led_loop();
  #ifdef BLUETOOTH_ENABLED
    btManager.loop();
  #endif
  
  // Optional: Power saving delay if NO clients connected? 
  // For RTSP low latency, we usually avoid delay, but a yield() helps watchdog.
  yield(); 
}
