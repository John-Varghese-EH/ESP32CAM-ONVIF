#include "telegram_manager.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static const char* TELEGRAM_API_HOST = "api.telegram.org";
static const int TELEGRAM_API_PORT = 443;

bool telegram_send_message(const char* message) {
    if (!appSettings.telegramEnabled) return false;
    if (strlen(appSettings.telegramBotToken) == 0 || strlen(appSettings.telegramChatId) == 0) {
        Serial.println("[TELEGRAM] Token or Chat ID not configured.");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate validation to save RAM/time
    HTTPClient http;

    String url = String("https://") + TELEGRAM_API_HOST + "/bot" + appSettings.telegramBotToken + "/sendMessage";
    url += "?chat_id=" + String(appSettings.telegramChatId);
    url += "&text=" + String(message);

    Serial.println("[TELEGRAM] Sending message...");
    if (http.begin(client, url)) {
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            Serial.println("[TELEGRAM] Message sent successfully.");
            http.end();
            return true;
        } else {
            Serial.printf("[TELEGRAM] Failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Serial.println("[TELEGRAM] Unable to connect");
    }
    return false;
}

bool telegram_send_photo(camera_fb_t* fb) {
    if (!appSettings.telegramEnabled) return false;
    if (strlen(appSettings.telegramBotToken) == 0 || strlen(appSettings.telegramChatId) == 0) {
        Serial.println("[TELEGRAM] Token or Chat ID not configured.");
        return false;
    }

    if (!fb || fb->len == 0) {
        Serial.println("[TELEGRAM] Invalid frame buffer.");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate validation
    
    if (!client.connect(TELEGRAM_API_HOST, TELEGRAM_API_PORT)) {
        Serial.println("[TELEGRAM] Connection failed!");
        return false;
    }

    Serial.println("[TELEGRAM] Connected to server, preparing to send photo...");

    String boundary = "----ESP32CAMBoundary" + String(millis());
    String url = "/bot" + String(appSettings.telegramBotToken) + "/sendPhoto";

    String head = "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
    head += String(appSettings.telegramChatId) + "\r\n";
    head += "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"photo\"; filename=\"snapshot.jpg\"\r\n";
    head += "Content-Type: image/jpeg\r\n\r\n";

    String tail = "\r\n--" + boundary + "--\r\n";

    uint32_t contentLength = head.length() + fb->len + tail.length();

    // Send HTTP POST headers
    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: " + String(TELEGRAM_API_HOST));
    client.println("Content-Length: " + String(contentLength));
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println();

    // Send body head
    client.print(head);

    // Send image data in chunks to avoid memory issues
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n += 1024) {
        if (n + 1024 < fbLen) {
            client.write(fbBuf, 1024);
            fbBuf += 1024;
        } else if (fbLen % 1024 > 0) {
            size_t remainder = fbLen % 1024;
            client.write(fbBuf, remainder);
        }
    }

    // Send body tail
    client.print(tail);

    // Wait for response
    int timeoutTimer = 10000;
    long startTimer = millis();
    bool state = false;
    while ((startTimer + timeoutTimer) > millis()) {
        Serial.print(".");
        delay(100);      
        while (client.available()) {
            char c = client.read();
            if (c == '\n') {
                if (state) {
                    Serial.println();
                    Serial.println("[TELEGRAM] Photo sent successfully!");
                    client.stop();
                    return true;
                }
                state = true;
            } else if (c != '\r') {
                state = false;
            }
        }
    }

    Serial.println();
    Serial.println("[TELEGRAM] Time out waiting for response.");
    client.stop();
    return false;
}
