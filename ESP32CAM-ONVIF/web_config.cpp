#include "web_config.h"
#include <WebServer.h>
#include <WiFi.h>
#include "index_html.h" // Inline HTML
#include "rtsp_server.h"
#include "onvif_server.h"
#include "motion_detection.h"
#include "auto_flash.h"
#include "camera_control.h"
#include <FS.h>
#include <SPIFFS.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "wifi_manager.h"
#include "config.h"
#include <Update.h>
#include <esp_task_wdt.h>
#include "sd_recorder.h"
#ifdef BLUETOOTH_ENABLED
  #include "bluetooth_manager.h"
  #include "audio_manager.h"
#endif

WebServer webConfigServer(WEB_PORT);

// WEB_USER and WEB_PASS are defined in config.h


bool isAuthenticated(WebServer &server) {
    if (!server.authenticate(WEB_USER, WEB_PASS)) {
        server.requestAuthentication();
        return false;
    }
    return true;
}

void web_config_start() {
    // SPIFFS no longer required for index.html, but still needed for SD/Config persistence if used
    if (!SPIFFS.begin(true)) {
        Serial.println("[WARN] SPIFFS Mount Failed - Configs might not save");
    }

    // Serving Embedded HTML
    webConfigServer.on("/", HTTP_GET, []() {
        if (!webConfigServer.authenticate(WEB_USER, WEB_PASS)) {
           return webConfigServer.requestAuthentication();
        }
        webConfigServer.send_P(200, "text/html", index_html);
    });

    // --- API ENDPOINTS ---
    webConfigServer.on("/api/status", HTTP_GET, []() {
        if (!isAuthenticated(webConfigServer)) return;
        String json = "{";
        json += "\"status\":\"Online\",";
        json += "\"rtsp\":\"" + getRTSPUrl() + "\",";
        json += "\"onvif\":\"http://" + WiFi.localIP().toString() + ":" + String(ONVIF_PORT) + "/onvif/device_service\",";
        json += "\"onvif_enabled\":" + String(onvif_is_enabled() ? "true" : "false") + ",";
        json += "\"motion\":" + String(motion_detected() ? "true" : "false") + ",";
        json += "\"recording\":" + String(sd_recorder_is_recording() ? "true" : "false") + ",";
        json += "\"sd_mounted\":" + String(sd_recorder_is_mounted() ? "true" : "false") + ",";
        json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"uptime\":" + String(millis() / 1000) + ",";
        json += "\"autoflash\":" + String(auto_flash_is_enabled() ? "true" : "false");
        json += "}";
        webConfigServer.send(200, "application/json", json);
    });

    // --- Change Camera Settings ---
    webConfigServer.on("/api/config", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, webConfigServer.arg("plain"));
        if (err) {
            webConfigServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        sensor_t * s = esp_camera_sensor_get();
        if (doc.containsKey("xclk")) {
            // To change xclk, you must re-init the camera. Not recommended at runtime.
            // Save to config and apply on reboot if needed.
        }
        if (doc.containsKey("resolution")) {
            String res = doc["resolution"].as<String>();
            if (res == "UXGA")      s->set_framesize(s, FRAMESIZE_UXGA);
            else if (res == "SXGA") s->set_framesize(s, FRAMESIZE_SXGA);
            else if (res == "XGA")  s->set_framesize(s, FRAMESIZE_XGA);
            else if (res == "SVGA") s->set_framesize(s, FRAMESIZE_SVGA);
            else if (res == "VGA")  s->set_framesize(s, FRAMESIZE_VGA);
            else if (res == "CIF")  s->set_framesize(s, FRAMESIZE_CIF);
            else if (res == "QVGA") s->set_framesize(s, FRAMESIZE_QVGA);
            else if (res == "QQVGA")s->set_framesize(s, FRAMESIZE_QQVGA);
        }
        if (doc.containsKey("quality"))     s->set_quality(s, doc["quality"]);
        if (doc.containsKey("brightness"))  s->set_brightness(s, doc["brightness"]);
        if (doc.containsKey("contrast"))    s->set_contrast(s, doc["contrast"]);
        if (doc.containsKey("saturation"))  s->set_saturation(s, doc["saturation"]);
        if (doc.containsKey("awb"))         s->set_whitebal(s, doc["awb"]);
        if (doc.containsKey("awb_gain"))    s->set_awb_gain(s, doc["awb_gain"]);
        if (doc.containsKey("wb_mode"))     s->set_wb_mode(s, doc["wb_mode"]);
        if (doc.containsKey("aec"))         s->set_aec2(s, doc["aec"]);
        if (doc.containsKey("aec2"))        s->set_aec2(s, doc["aec2"]);
        if (doc.containsKey("ae_level"))    s->set_ae_level(s, doc["ae_level"]);
        if (doc.containsKey("agc"))         s->set_gain_ctrl(s, doc["agc"]);
        if (doc.containsKey("gainceiling")) s->set_gainceiling(s, doc["gainceiling"]);
        if (doc.containsKey("bpc"))         s->set_bpc(s, doc["bpc"]);
        if (doc.containsKey("wpc"))         s->set_wpc(s, doc["wpc"]);
        if (doc.containsKey("raw_gma"))     s->set_raw_gma(s, doc["raw_gma"]);
        if (doc.containsKey("lenc"))        s->set_lenc(s, doc["lenc"]);
        if (doc.containsKey("hmirror"))     s->set_hmirror(s, doc["hmirror"]);
        if (doc.containsKey("vflip"))       s->set_vflip(s, doc["vflip"]);
        if (doc.containsKey("dcw"))         s->set_dcw(s, doc["dcw"]);
        webConfigServer.send(200, "application/json", "{\"ok\":1}");
    });

    // --- SD Card File List ---
    webConfigServer.on("/api/sd/list", HTTP_GET, []() {
        if (!isAuthenticated(webConfigServer)) return;
        String json = "[";
        File root = SD_MMC.open("/");
        File file = root.openNextFile();
        bool first = true;
        while(file){
            if (!first) json += ",";
            json += "\"" + String(file.name()) + "\"";
            file = root.openNextFile();
            first = false;
        }
        json += "]";
        webConfigServer.send(200, "application/json", json);
    });

    // --- SD Card Download ---
    // CSS and JS are now inline in index.html, no need to serve files
    // But keep stream logic etc.
    webConfigServer.on("/api/sd/download", HTTP_GET, []() {
        if (!isAuthenticated(webConfigServer)) return;
        if (!webConfigServer.hasArg("file")) {
            webConfigServer.send(400, "text/plain", "Missing file param");
            return;
        }
        String filename = "/" + webConfigServer.arg("file");
        File file = SD_MMC.open(filename, "r");
        if (!file) {
            webConfigServer.send(404, "text/plain", "File not found");
            return;
        }
        webConfigServer.streamFile(file, "application/octet-stream");
        file.close();
    });

    // --- SD Card Delete ---
    webConfigServer.on("/api/sd/delete", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        StaticJsonDocument<128> doc;
        DeserializationError err = deserializeJson(doc, webConfigServer.arg("plain"));
        if (err || !doc.containsKey("file")) {
            webConfigServer.send(400, "application/json", "{\"error\":\"Invalid request\"}");
            return;
        }
        String filename = "/" + doc["file"].as<String>();
        if (SD_MMC.remove(filename)) {
            webConfigServer.send(200, "application/json", "{\"ok\":1}");
        } else {
            webConfigServer.send(404, "application/json", "{\"error\":\"Delete failed\"}");
        }
    });

    // --- SD Recording Trigger ---
    webConfigServer.on("/api/record", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        
        StaticJsonDocument<128> doc;
        deserializeJson(doc, webConfigServer.arg("plain"));
        String action = doc["action"];
        
        if (action == "start") {
            sd_recorder_start_manual();
        } else if (action == "stop") {
            sd_recorder_stop_manual();
        }
        
        webConfigServer.send(200, "application/json", "{\"ok\":1}");
    });

    // --- Flash Control ---
    webConfigServer.on("/api/flash", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        StaticJsonDocument<64> doc;
        deserializeJson(doc, webConfigServer.arg("plain"));
        bool state = doc["state"];
        
        // If manual control is used, disable auto flash temporarily (or user should toggle it off first)
        // But for simplicity, we just set the LED.
        set_flash_led(state);
        webConfigServer.send(200, "application/json", "{}");
    });
    
    webConfigServer.on("/api/autoflash", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        StaticJsonDocument<64> doc;
        deserializeJson(doc, webConfigServer.arg("plain"));
        bool enabled = doc["enabled"];
        auto_flash_set_enabled(enabled);
        webConfigServer.send(200, "application/json", "{}");
    });
    
    // --- ONVIF Enable/Disable ---
    webConfigServer.on("/api/onvif/toggle", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        StaticJsonDocument<64> doc;
        deserializeJson(doc, webConfigServer.arg("plain"));
        bool enabled = doc["enabled"];
        onvif_set_enabled(enabled);
        webConfigServer.send(200, "application/json", "{}");
    });

    // --- Reboot ---
    webConfigServer.on("/reboot", []() {
        if (!isAuthenticated(webConfigServer)) return;
        webConfigServer.send(200, "application/json", "{\"ok\":1, \"msg\":\"Rebooting...\"}");
        delay(1000);
        ESP.restart();
    });

    // --- Factory Reset (stub) ---
    webConfigServer.on("/api/factory_reset", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        // Reset settings logic here
        webConfigServer.send(200, "application/json", "{\"ok\":1}");
        ESP.restart();
    });



    // --- Time Sync API ---
    webConfigServer.on("/api/time", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        StaticJsonDocument<64> doc;
        deserializeJson(doc, webConfigServer.arg("plain"));
        long epoch = doc["epoch"];
        if(epoch > 0) {
            struct timeval tv;
            tv.tv_sec = epoch;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL); 
            Serial.println("[INFO] Time set via Web");
            webConfigServer.send(200, "application/json", "{\"ok\":1}");
        } else {
             webConfigServer.send(400, "application/json", "{\"error\":\"Invalid time\"}");
        }
    });

    // --- OTA Firmware Update ---
    webConfigServer.on("/api/update", HTTP_POST, []() {
        webConfigServer.send(200, "application/json", (Update.hasError()) ? "{\"success\":false}" : "{\"success\":true}");
        delay(1000);
        ESP.restart();
    }, []() {
        HTTPUpload& upload = webConfigServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("[INFO] Update: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { 
                Serial.printf("[INFO] Update Success: %u\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
        }
    });
    
    // --- WiFi API Endpoints ---
    webConfigServer.on("/api/wifi/status", HTTP_GET, []() {
        if (!isAuthenticated(webConfigServer)) return;
        
        String json = "{";
        json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
        json += "\"ssid\":\"" + wifiManager.getSSID() + "\",";
        json += "\"ip\":\"" + wifiManager.getLocalIP().toString() + "\",";
        json += "\"mode\":\"" + String(wifiManager.isInAPMode() ? "AP" : "STA") + "\"";
        json += "}";
        
        webConfigServer.send(200, "application/json", json);
    });
    
    webConfigServer.on("/api/wifi/scan", HTTP_GET, []() {
        if (!isAuthenticated(webConfigServer)) return;
        
        int networksFound = wifiManager.scanNetworks();
        WiFiNetwork* networks = wifiManager.getScannedNetworks();
        
        String json = "{\"networks\":[";
        for (int i = 0; i < networksFound; i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"ssid\":\"" + networks[i].ssid + "\",";
            json += "\"rssi\":" + String(networks[i].rssi) + ",";
            json += "\"encType\":" + String(networks[i].encType);
            json += "}";
        }
        json += "]}";
        
        webConfigServer.send(200, "application/json", json);
    });
    
    webConfigServer.on("/api/wifi/connect", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, webConfigServer.arg("plain"));
        
        if (err || !doc.containsKey("ssid") || !doc.containsKey("password")) {
            webConfigServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid request\"}");
            return;
        }
        
        String ssid = doc["ssid"].as<String>();
        String password = doc["password"].as<String>();
        
        // Save credentials
        if (wifiManager.saveCredentials(ssid, password)) {
            // Try to connect
            bool connected = wifiManager.connectToNetwork(ssid, password);
            
            if (connected) {
                webConfigServer.send(200, "application/json", "{\"success\":true,\"message\":\"Connected\"}");
                
                // Optional: schedule a restart after a short delay to ensure response is sent
                delay(1000);
                ESP.restart();
            } else {
                webConfigServer.send(200, "application/json", "{\"success\":false,\"message\":\"Failed to connect\"}");
            }
        } else {
            webConfigServer.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save credentials\"}");
        }
    });

    // === STREAM ENDPOINT ===
    webConfigServer.on("/stream", HTTP_GET, []() {
        if (!isAuthenticated(webConfigServer)) return;
        WiFiClient client = webConfigServer.client();
        
        String response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "\r\n";
        client.print(response);

        Serial.println("[INFO] MJPEG Stream started");
        
        // Optimize TCP for streaming
        client.setTimeout(2); // Set low timeout for writes (2s) to prevent blocking
        
        int64_t last_frame = 0;
        const int frame_interval = 100; // 100ms = ~10 FPS

        while (client.connected()) {
            // CRITICAL: Feed the Watchdog Timer so the ESP32 doesn't reboot
            esp_task_wdt_reset();
            
            // Keep critical background tasks alive (God Loop Pattern)
            rtsp_server_loop();   
            onvif_server_loop();
            
            int64_t now = esp_timer_get_time() / 1000;
            if (now - last_frame < frame_interval) {
                // Yield to allow WiFi stack to process
                yield(); 
                delay(10); // Sleep 10ms to save CPU
                continue;
            }
            last_frame = now;

            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                Serial.println("[WARN] Frame buffer failed");
                delay(100);
                continue;
            }
            
            // Send buffer using chunked writes if needed, but client.write handles it.
            // Check if we can write to avoid stalling on full buffer
            // (Standard Client doesn't expose availableForWrite easily on all cores, but write() is blocking with timeout)
            
            size_t dataLen = fb->len; // Cache before releasing!
            size_t hlen = client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", dataLen);
            size_t wlen = client.write(fb->buf, dataLen);
            client.print("\r\n");
            
            esp_camera_fb_return(fb); // Release immediately
            
            if (wlen != dataLen) {
                 Serial.println("[WARN] Stream write failed (Client disconnected?)");
                 break;
            }
        }
        
        Serial.println("[INFO] MJPEG Stream stopped");
    });

    // --- Snapshot endpoint ---
    webConfigServer.on("/snapshot", HTTP_GET, []() {
        if (!isAuthenticated(webConfigServer)) return;
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            webConfigServer.send(500, "text/plain", "Camera Error");
            return;
        }
        webConfigServer.sendHeader("Content-Type", "image/jpeg");
        webConfigServer.send_P(200, "image/jpeg", (char*)fb->buf, fb->len);
        esp_camera_fb_return(fb);
    });

    // --- Bluetooth Endpoints ---
    #ifdef BLUETOOTH_ENABLED
    webConfigServer.on("/api/bt/status", HTTP_GET, []() {
        if (!isAuthenticated(webConfigServer)) return;
        String json = "{";
        json += "\"enabled\":" + String(appSettings.btEnabled ? "true" : "false") + ",";
        json += "\"stealth\":" + String(appSettings.btStealthMode ? "true" : "false") + ",";
        json += "\"mac\":\"" + appSettings.btPresenceMac + "\",";
        json += "\"userPresent\":" + String(btManager.isUserPresent() ? "true" : "false") + ",";
        json += "\"audioSource\":" + String(appSettings.audioSource) + ",";
        json += "\"gain\":" + String(appSettings.btMicGain) + ",";
        json += "\"timeout\":" + String(appSettings.btPresenceTimeout);
        json += "}";
        webConfigServer.send(200, "application/json", json);
    });

    webConfigServer.on("/api/bt/config", HTTP_POST, []() {
        if (!isAuthenticated(webConfigServer)) return;
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, webConfigServer.arg("plain"));
        if (err) {
            webConfigServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        if(doc.containsKey("enabled")) appSettings.btEnabled = doc["enabled"];
        if(doc.containsKey("stealth")) appSettings.btStealthMode = doc["stealth"];
        if(doc.containsKey("mac")) appSettings.btPresenceMac = doc["mac"].as<String>();
        if(doc.containsKey("gain")) appSettings.btMicGain = doc["gain"];
        if(doc.containsKey("timeout")) appSettings.btPresenceTimeout = doc["timeout"];
        if(doc.containsKey("audioSource")) {
             int src = doc["audioSource"];
             appSettings.audioSource = (AudioSource)src;
             audioManager.begin(); // Re-init audio with new source
        }
        
        saveSettings();
        
        // If enabling BT, start it
        if (appSettings.btEnabled) btManager.begin();
        
        webConfigServer.send(200, "application/json", "{\"ok\":1}");
    });

    webConfigServer.on("/api/bt/scan", HTTP_GET, []() {
        if (!isAuthenticated(webConfigServer)) return;
        // Trigger scan if not enabled?
        // Return latest cache
        webConfigServer.send(200, "application/json", btManager.getLastScanResult());
    });
    #endif // BLUETOOTH_ENABLED
    
    webConfigServer.begin();
        Serial.println("[INFO] Web config server started.");
    }

    void web_config_loop() {
        webConfigServer.handleClient();
    }