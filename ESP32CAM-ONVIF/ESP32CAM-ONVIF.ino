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
  
  // --- Create FreeRTOS Tasks ---
  xTaskCreatePinnedToCore(rtsp_stream_task, "RTSP_Task", 6144, NULL, 4, &rtspTaskHandle, 0);
  xTaskCreatePinnedToCore(onvif_http_task, "ONVIF_HTTP_Task", 6144, NULL, 3, &onvifTaskHandle, 1);
  xTaskCreatePinnedToCore(wifi_mgmt_task, "WiFi_Mgmt_Task", 4096, NULL, 6, &wifiTaskHandle, 1);
  xTaskCreatePinnedToCore(watchdog_task, "WDT_Task", 2048, NULL, 7, &wdtTaskHandle, 1);
  xTaskCreatePinnedToCore(low_prio_task, "Low_Prio_Task", 4096, NULL, 2, &lowPrioTaskHandle, 1);
  
  // MQTT is spawned dynamically by Watchdog Task to save RAM if disabled
}

void loop() {
  // Main loop is now empty. Everything runs in FreeRTOS tasks.
  vTaskDelay(pdMS_TO_TICKS(1000));
}

// === FreeRTOS Task Definitions ===

void rtsp_stream_task(void *pvParameters) {
    esp_task_wdt_add(NULL);
    while (1) {
        esp_task_wdt_reset();
        rtsp_server_loop();   // Highest priority for streaming
        vTaskDelay(pdMS_TO_TICKS(5)); // Small delay to prevent starving other tasks
    }
}

void onvif_http_task(void *pvParameters) {
    esp_task_wdt_add(NULL);
    while (1) {
        esp_task_wdt_reset();
        web_config_loop();    // Web UI
        onvif_server_loop();  // Discovery/SOAP
        vTaskDelay(pdMS_TO_TICKS(20)); // Web tasks can sleep a bit longer
    }
}

void wifi_mgmt_task(void *pvParameters) {
    esp_task_wdt_add(NULL);
    while (1) {
        esp_task_wdt_reset();
        wifiManager.loop();   // Connectivity checks
        vTaskDelay(pdMS_TO_TICKS(100)); // Run every 100ms
    }
}

void watchdog_task(void *pvParameters) {
    esp_task_wdt_add(NULL);
    while (1) {
        esp_task_wdt_reset();
        
        // Memory Leak Audit
        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t min_free = esp_get_minimum_free_heap_size();
        
        if (free_heap < 20480) { // Under 20KB free heap
            Serial.printf("[CRITICAL] Low Heap Memory! Free: %u. Forcing restart for stability.\n", free_heap);
            esp_restart();
        }
        
        // Dynamic Task Manager
        if (appSettings.mqttEnabled && mqttTaskHandle == NULL) {
            Serial.println("[INFO] Dynamic Task Manager: Spawning MQTT_Task");
            xTaskCreatePinnedToCore(mqtt_task, "MQTT_Task", 4096, NULL, 2, &mqttTaskHandle, 1);
        }

        // Stack HWM Logging (Optional, runs once every 30s)
        static uint32_t last_stack_log = 0;
        if (millis() - last_stack_log > 30000) {
            last_stack_log = millis();
            Serial.printf("[INFO] Free heap: %u | Min free: %u | PSRAM free: %u\n",
                free_heap, min_free, ESP.getFreePsram());
            
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
            Serial.printf("[INFO] WDT_Task HWM: %u bytes\n", hwm * sizeof(StackType_t));
            
            if(rtspTaskHandle) { hwm = uxTaskGetStackHighWaterMark(rtspTaskHandle); Serial.printf("[INFO] RTSP_Task HWM: %u bytes\n", hwm * sizeof(StackType_t)); }
            if(onvifTaskHandle) { hwm = uxTaskGetStackHighWaterMark(onvifTaskHandle); Serial.printf("[INFO] ONVIF_Task HWM: %u bytes\n", hwm * sizeof(StackType_t)); }
            if(wifiTaskHandle) { hwm = uxTaskGetStackHighWaterMark(wifiTaskHandle); Serial.printf("[INFO] WiFi_Task HWM: %u bytes\n", hwm * sizeof(StackType_t)); }
            if(mqttTaskHandle) { hwm = uxTaskGetStackHighWaterMark(mqttTaskHandle); Serial.printf("[INFO] MQTT_Task HWM: %u bytes\n", hwm * sizeof(StackType_t)); }
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Feed every 2s
    }
}

void low_prio_task(void *pvParameters) {
    esp_task_wdt_add(NULL);
    while (1) {
        esp_task_wdt_reset();
        motion_detection_loop();
        sd_recorder_loop();
        serial_console_loop();
        auto_flash_loop();
        status_led_loop();
        #ifdef BLUETOOTH_ENABLED
            btManager.loop();
        #endif
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms cycle
    }
}
