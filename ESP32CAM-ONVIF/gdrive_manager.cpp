#include "gdrive_manager.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "Base64.h"

// Task Handle
TaskHandle_t gDriveTaskHandle = NULL;

// Buffer for async transfer
static uint8_t* gDriveJpgBuf = NULL;
static size_t gDriveJpgLen = 0;

void gDriveUploadTask(void *pvParameters) {
    if (gDriveJpgBuf == NULL || gDriveJpgLen == 0) {
        Serial.println("[GDRIVE] Error: No valid image buffer.");
        vTaskDelete(NULL);
        return;
    }

    Serial.println("[GDRIVE] Starting upload task...");
    
    // We encode the JPEG to base64, which uses more memory, but simplifies the Google Apps Script side.
    // If memory is too constrained, we could use a custom URL-encoded binary stream or raw binary post.
    // However, given PSRAM we can attempt Base64.
    
    // Let's use raw binary upload to save memory, and the Apps script can read the post body.
    // Actually, sending binary in standard POST is better done with multipart/form-data.
    
    String boundary = "----ESP32CamBoundary123456789";
    String head = "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"file\"; filename=\"motion.jpg\"\r\n";
    head += "Content-Type: image/jpeg\r\n\r\n";
    
    String tail = "\r\n--" + boundary + "--\r\n";

    size_t contentLength = head.length() + gDriveJpgLen + tail.length();

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    Serial.printf("[GDRIVE] Connecting to %s\n", appSettings.googleDriveScriptUrl);
    
    if (http.begin(client, appSettings.googleDriveScriptUrl)) {
        http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        // This relies on the ESP32 HTTPClient sendRequest with a pointer buffer.
        // To save memory, we can manually construct the payload in a single buffer, or use the stream API.
        // Given PSRAM, allocating another buffer for the full HTTP payload is possible but risky.
        // Let's use the ESP32 HTTPClient's sendRequest that accepts a buffer, and we must construct the buffer.
        
        uint8_t* payloadBuf = (uint8_t*)heap_caps_malloc(contentLength, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (payloadBuf != NULL) {
            size_t offset = 0;
            memcpy(payloadBuf + offset, head.c_str(), head.length());
            offset += head.length();
            memcpy(payloadBuf + offset, gDriveJpgBuf, gDriveJpgLen);
            offset += gDriveJpgLen;
            memcpy(payloadBuf + offset, tail.c_str(), tail.length());
            
            int httpCode = http.sendRequest("POST", payloadBuf, contentLength);
            if (httpCode > 0) {
                Serial.printf("[GDRIVE] Upload successful! HTTP Code: %d\n", httpCode);
                String response = http.getString();
                Serial.println(response);
            } else {
                Serial.printf("[GDRIVE] Upload failed! Error: %s\n", http.errorToString(httpCode).c_str());
            }
            
            free(payloadBuf);
        } else {
            Serial.println("[GDRIVE] Error: Failed to allocate payload buffer in PSRAM.");
        }
        
        http.end();
    } else {
        Serial.println("[GDRIVE] Error: Unable to connect.");
    }

    // Clean up
    free(gDriveJpgBuf);
    gDriveJpgBuf = NULL;
    gDriveJpgLen = 0;
    
    Serial.println("[GDRIVE] Upload task finished.");
    gDriveTaskHandle = NULL;
    vTaskDelete(NULL);
}

void uploadToGDriveAsync(camera_fb_t* fb) {
    if (!appSettings.googleDriveEnabled || strlen(appSettings.googleDriveScriptUrl) == 0) {
        return;
    }

    if (gDriveTaskHandle != NULL) {
        Serial.println("[GDRIVE] Upload already in progress. Skipping.");
        return;
    }

    // Allocate buffer in PSRAM
    gDriveJpgBuf = (uint8_t*)heap_caps_malloc(fb->len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (gDriveJpgBuf == NULL) {
        Serial.println("[GDRIVE] Failed to allocate PSRAM for GDrive buffer.");
        return;
    }
    
    // Copy the image frame
    memcpy(gDriveJpgBuf, fb->buf, fb->len);
    gDriveJpgLen = fb->len;

    // Create the task
    xTaskCreate(
        gDriveUploadTask,
        "GDriveUpload",
        8192,
        NULL,
        1, // low priority
        &gDriveTaskHandle
    );
}

void initGDrive() {
    // Nothing special to initialize on boot for GDrive
    Serial.println("[GDRIVE] Initialized.");
}
