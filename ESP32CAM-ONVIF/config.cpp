#include "config.h"
#include <Arduino.h>

void printBanner() {
  Serial.println();
  Serial.println("==============================================");
  Serial.println("  ESP32CAM-ONVIF Professional Camera Firmware ");
  Serial.println(
      "         Made with \xE2\x9D\xA4\xEF\xB8\x8F  by J0X           ");
  Serial.println("==============================================");
}

void fatalError(const char *msg) {
  Serial.println();
  Serial.print("[FATAL] ");
  Serial.println(msg);
  Serial.println("[FATAL] System halted.");
  while (1)
    delay(1000);
}

// --- Persistence Implementation ---
#include <ArduinoJson.h>
#include <SPIFFS.h>

AppSettings appSettings;

static const char *SETTINGS_FILE = "/settings.json";

void loadSettings() {
  // defaults
  appSettings.btEnabled = false;
  appSettings.btStealthMode = false;
  appSettings.btPresenceMac[0] = '\0';
  appSettings.btPresenceTimeout = 120;
  appSettings.btMicGain = 50;
  appSettings.hwMicGain = 50;
// Auto-enable Mic for boards with built-in Microphones
#if defined(BOARD_ESP_EYE) || defined(BOARD_ESP32S3_EYE) ||                    \
    defined(BOARD_TTGO_T_CAMERA) || defined(BOARD_TTGO_T_JOURNAL) ||           \
    defined(BOARD_SEEED_XIAO_S3)
  appSettings.audioSource = AUDIO_SOURCE_HARDWARE_I2S;
#else
  appSettings.audioSource = AUDIO_SOURCE_NONE;
#endif

  // MQTT Defaults
  appSettings.mqttEnabled = MQTT_ENABLED;
  strncpy(appSettings.mqttBroker, MQTT_BROKER,
          sizeof(appSettings.mqttBroker) - 1);
  appSettings.mqttPort = MQTT_PORT;
  strncpy(appSettings.mqttUser, MQTT_USER, sizeof(appSettings.mqttUser) - 1);
  strncpy(appSettings.mqttPassword, MQTT_PASS,
          sizeof(appSettings.mqttPassword) - 1);

  // Telegram Defaults
  appSettings.telegramEnabled = false;
  appSettings.telegramBotToken[0] = '\0';
  appSettings.telegramChatId[0] = '\0';

  appSettings.googleDriveEnabled = false;
  appSettings.googleDriveMotion = true;
  appSettings.googleDriveScriptUrl[0] = '\0';

  // Advanced Features Defaults
  appSettings.webDavEnabled = false;
  appSettings.continuousRecordingEnabled = false;
  appSettings.continuousRecordingChunkSize = 5;
  strncpy(appSettings.ntpServer, "pool.ntp.org",
          sizeof(appSettings.ntpServer) - 1);
  strncpy(appSettings.timeZone, "UTC0", sizeof(appSettings.timeZone) - 1);

  if (!SPIFFS.exists(SETTINGS_FILE)) {
    Serial.println("[INFO] No settings file found, using defaults.");
    return;
  }

  File file = SPIFFS.open(SETTINGS_FILE, "r");
  if (!file) {
    Serial.println("[ERROR] Failed to open settings file");
    return;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("[ERROR] Failed to parse settings file");
    return;
  }

  if (doc.containsKey("btEnabled"))
    appSettings.btEnabled = doc["btEnabled"];
  if (doc.containsKey("btStealth"))
    appSettings.btStealthMode = doc["btStealth"];
  if (doc.containsKey("btMac"))
    strncpy(appSettings.btPresenceMac, doc["btMac"] | "",
            sizeof(appSettings.btPresenceMac) - 1);
  if (doc.containsKey("btTimeout"))
    appSettings.btPresenceTimeout = doc["btTimeout"];
  if (doc.containsKey("btGain"))
    appSettings.btMicGain = doc["btGain"];
  if (doc.containsKey("hwGain"))
    appSettings.hwMicGain = doc["hwGain"];
  if (doc.containsKey("audioSrc"))
    appSettings.audioSource = (AudioSource)doc["audioSrc"].as<int>();

  if (doc.containsKey("mqttEnabled"))
    appSettings.mqttEnabled = doc["mqttEnabled"];
  if (doc.containsKey("mqttBroker"))
    strncpy(appSettings.mqttBroker, doc["mqttBroker"] | "",
            sizeof(appSettings.mqttBroker) - 1);
  if (doc.containsKey("mqttPort"))
    appSettings.mqttPort = doc["mqttPort"];
  if (doc.containsKey("mqttUser"))
    strncpy(appSettings.mqttUser, doc["mqttUser"] | "",
            sizeof(appSettings.mqttUser) - 1);
  if (doc.containsKey("mqttPass"))
    strncpy(appSettings.mqttPassword, doc["mqttPass"] | "",
            sizeof(appSettings.mqttPassword) - 1);

  if (doc.containsKey("tgEnabled"))
    appSettings.telegramEnabled = doc["tgEnabled"];
  if (doc.containsKey("tgToken"))
    strncpy(appSettings.telegramBotToken, doc["tgToken"] | "",
            sizeof(appSettings.telegramBotToken) - 1);
  if (doc.containsKey("tgChatId"))
    strncpy(appSettings.telegramChatId, doc["tgChatId"] | "",
            sizeof(appSettings.telegramChatId) - 1);

  if (doc.containsKey("gdEnabled"))
    appSettings.googleDriveEnabled = doc["gdEnabled"];
  if (doc.containsKey("gdMotion"))
    appSettings.googleDriveMotion = doc["gdMotion"];
  if (doc.containsKey("gdUrl"))
    strncpy(appSettings.googleDriveScriptUrl, doc["gdUrl"] | "",
            sizeof(appSettings.googleDriveScriptUrl) - 1);

  if (doc.containsKey("webDav"))
    appSettings.webDavEnabled = doc["webDav"];
  if (doc.containsKey("contRec"))
    appSettings.continuousRecordingEnabled = doc["contRec"];
  if (doc.containsKey("contRecChunk"))
    appSettings.continuousRecordingChunkSize = doc["contRecChunk"];
  if (doc.containsKey("ntp"))
    strncpy(appSettings.ntpServer, doc["ntp"] | "pool.ntp.org",
            sizeof(appSettings.ntpServer) - 1);
  if (doc.containsKey("tz"))
    strncpy(appSettings.timeZone, doc["tz"] | "UTC0",
            sizeof(appSettings.timeZone) - 1);

  Serial.println("[INFO] Settings loaded.");
}

void saveSettings() {
  StaticJsonDocument<1024> doc;
  doc["btEnabled"] = appSettings.btEnabled;
  doc["btStealth"] = appSettings.btStealthMode;
  doc["btMac"] = appSettings.btPresenceMac;
  doc["btTimeout"] = appSettings.btPresenceTimeout;
  doc["btGain"] = appSettings.btMicGain;
  doc["hwGain"] = appSettings.hwMicGain;
  doc["audioSrc"] = (int)appSettings.audioSource;

  doc["mqttEnabled"] = appSettings.mqttEnabled;
  doc["mqttBroker"] = (const char *)appSettings.mqttBroker;
  doc["mqttPort"] = appSettings.mqttPort;
  doc["mqttUser"] = (const char *)appSettings.mqttUser;
  doc["mqttPass"] = (const char *)appSettings.mqttPassword;

  doc["tgEnabled"] = appSettings.telegramEnabled;
  doc["tgToken"] = (const char *)appSettings.telegramBotToken;
  doc["tgChatId"] = (const char *)appSettings.telegramChatId;

  doc["gdEnabled"] = appSettings.googleDriveEnabled;
  doc["gdMotion"] = appSettings.googleDriveMotion;
  doc["gdUrl"] = (const char *)appSettings.googleDriveScriptUrl;

  doc["webDav"] = appSettings.webDavEnabled;
  doc["contRec"] = appSettings.continuousRecordingEnabled;
  doc["contRecChunk"] = appSettings.continuousRecordingChunkSize;
  doc["ntp"] = (const char *)appSettings.ntpServer;
  doc["tz"] = (const char *)appSettings.timeZone;

  File file = SPIFFS.open(SETTINGS_FILE, "w");
  if (!file) {
    Serial.println("[ERROR] Failed to write settings file");
    return;
  }

  serializeJson(doc, file);
  file.close();
  Serial.println("[INFO] Settings saved.");
}
