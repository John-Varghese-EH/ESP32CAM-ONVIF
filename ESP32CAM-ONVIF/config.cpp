#include "config.h"
#include <Arduino.h>

void printBanner() {
  Serial.println();
  Serial.println("==============================================");
  Serial.println("  ESP32CAM-ONVIF Professional Camera Firmware ");
  Serial.println("         Made with \xE2\x9D\xA4\xEF\xB8\x8F  by J0X           ");
  Serial.println("==============================================");
}

void fatalError(const char* msg) {
  Serial.println();
  Serial.print("[FATAL] "); Serial.println(msg);
  Serial.println("[FATAL] System halted.");
  while (1) delay(1000);
}

// --- Persistence Implementation ---
#include <ArduinoJson.h>
#include <SPIFFS.h>

AppSettings appSettings;

static const char* SETTINGS_FILE = "/settings.json";

void loadSettings() {
    // defaults
    appSettings.btEnabled = false;
    appSettings.btStealthMode = false;
    appSettings.btPresenceMac = "";
    appSettings.btPresenceTimeout = 120;
    appSettings.btMicGain = 50;
    appSettings.hwMicGain = 50;
    // Auto-enable Mic for boards with built-in Microphones
    #if defined(BOARD_ESP_EYE) || defined(BOARD_ESP32S3_EYE) || defined(BOARD_TTGO_T_CAMERA) || defined(BOARD_TTGO_T_JOURNAL) || defined(BOARD_SEEED_XIAO_S3)
        appSettings.audioSource = AUDIO_SOURCE_HARDWARE_I2S;
    #else
        appSettings.audioSource = AUDIO_SOURCE_NONE;
    #endif

    if (!SPIFFS.exists(SETTINGS_FILE)) {
        Serial.println("[INFO] No settings file found, using defaults.");
        return;
    }

    File file = SPIFFS.open(SETTINGS_FILE, "r");
    if (!file) {
        Serial.println("[ERROR] Failed to open settings file");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println("[ERROR] Failed to parse settings file");
        return;
    }

    if(doc.containsKey("btEnabled")) appSettings.btEnabled = doc["btEnabled"];
    if(doc.containsKey("btStealth")) appSettings.btStealthMode = doc["btStealth"];
    if(doc.containsKey("btMac"))     appSettings.btPresenceMac = doc["btMac"].as<String>();
    if(doc.containsKey("btTimeout")) appSettings.btPresenceTimeout = doc["btTimeout"];
    if(doc.containsKey("btGain"))    appSettings.btMicGain = doc["btGain"];
    if(doc.containsKey("hwGain"))    appSettings.hwMicGain = doc["hwGain"];
    if(doc.containsKey("audioSrc"))  appSettings.audioSource = (AudioSource)doc["audioSrc"].as<int>();
    
    Serial.println("[INFO] Settings loaded.");
}

void saveSettings() {
    StaticJsonDocument<512> doc;
    doc["btEnabled"] = appSettings.btEnabled;
    doc["btStealth"] = appSettings.btStealthMode;
    doc["btMac"]     = appSettings.btPresenceMac;
    doc["btTimeout"] = appSettings.btPresenceTimeout;
    doc["btGain"]    = appSettings.btMicGain;
    doc["hwGain"]    = appSettings.hwMicGain;
    doc["audioSrc"]  = (int)appSettings.audioSource;

    File file = SPIFFS.open(SETTINGS_FILE, "w");
    if (!file) {
        Serial.println("[ERROR] Failed to write settings file");
        return;
    }

    serializeJson(doc, file);
    file.close();
    Serial.println("[INFO] Settings saved.");
}
