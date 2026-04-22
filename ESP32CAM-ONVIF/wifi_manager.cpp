#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "config.h"
#include "status_led.h"
#include "onvif_server.h"
#include <esp_wifi.h> // For esp_wifi_set_ps

// RTC Data for Fast Reconnect
RTC_DATA_ATTR uint8_t  rtc_bssid[6];
RTC_DATA_ATTR uint8_t  rtc_channel;
RTC_DATA_ATTR uint32_t rtc_ip;
RTC_DATA_ATTR uint32_t rtc_gw;
RTC_DATA_ATTR uint32_t rtc_mask;
RTC_DATA_ATTR bool     rtc_valid = false;

// Global instance
WiFiManager wifiManager;

WiFiManager::WiFiManager() : _apMode(false), _scannedNetworksCount(0), _lastConnectAttempt(0), _lastConnectedTime(0) {
}

void WiFiManager::loop() {
    if (_apMode) return;
    
    // Check connectivity
    if (checkConnectivity()) {
        status_led_connected();
        _lastConnectedTime = millis(); // Refresh timestamp
        
        // Handle post-reconnect state
        static bool _wasConnected = true;
        if (!_wasConnected) {
            _wasConnected = true;
            Serial.println("[INFO] Re-broadcasting ONVIF WS-Discovery...");
            onvif_reconnect(); 
        }
    } else {
        status_led_error();
        
        // Track disconnection
        static bool _wasConnected = true;
        if (_wasConnected) {
            _wasConnected = false;
        }

        unsigned long now = millis();
        
        // 1. Aggressive Reconnect Attempt
        if (now - _lastConnectAttempt > 10000) {
            _lastConnectAttempt = now;
            Serial.println(F("[WARN] WiFi Lost. Reconnecting..."));
            status_led_wifi_connecting();
            
            // Force disconnect first to clear stuck states
            WiFi.disconnect();
            WiFi.reconnect();
        }
        
        // 2. Fatal Timeout -> Reboot
        if (now - _lastConnectedTime > _wifiTimeoutMs) {
            Serial.printf("[FATAL] No WiFi for %lu ms. Rebooting for stability...\n", _wifiTimeoutMs);
            delay(1000);
            esp_restart();
        }
    }
}

bool WiFiManager::checkConnectivity() {
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::begin() {
  // Register WiFi Event Handler for immediate reconnect
  WiFi.onEvent([](WiFiEvent_t event) {
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
          Serial.println("[WARN] WiFi lost (Event). Reconnecting...");
          WiFi.reconnect();
      }
  });

  // First try to connect using stored credentials
  if (connectToStoredNetwork()) {
    return true;
  }

  // If stored credentials failed or missing, try hardcoded config.h credentials
  status_led_wifi_connecting();
  String configSSID = WIFI_SSID;
  if(configSSID != "YOUR_WIFI_SSID" && configSSID.length() > 0) {
      Serial.println(F("[INFO] Trying config.h credentials..."));
      if(connectToNetwork(WIFI_SSID, WIFI_PASSWORD)) {
          return true;
      }
  }
  
  // If stored credentials don't work, start AP mode
  startAPMode();
  return false;
}

bool WiFiManager::connectToStoredNetwork() {
  WiFiCredentials creds = loadCredentials();
  
  if (creds.ssid.length() == 0) {
    Serial.println(F("[INFO] No stored WiFi credentials found"));
    return false;
  }
  
  return connectToNetwork(creds.ssid, creds.password);
}

bool WiFiManager::connectToNetwork(const String& ssid, const String& password) {
  _apMode = false;
  WiFi.mode(WIFI_STA);
  // Important for stable streaming: Disable WiFi Power Save
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE); // IDF-level disable
  
  if (rtc_valid) {
      Serial.println("[INFO] Fast Reconnect: Using RTC variables.");
      IPAddress ip(rtc_ip);
      IPAddress gw(rtc_gw);
      IPAddress mask(rtc_mask);
      WiFi.config(ip, gw, mask);
      WiFi.begin(ssid.c_str(), password.c_str(), rtc_channel, rtc_bssid, true);
  } else {
      if (STATIC_IP_ENABLED) {
        IPAddress ip(STATIC_IP_ADDR);
        IPAddress gateway(STATIC_GATEWAY);
        IPAddress subnet(STATIC_SUBNET);
        IPAddress dns(STATIC_DNS);
        
        if (WiFi.config(ip, gateway, subnet, dns)) {
          Serial.println("[INFO] Static IP Configured: " + ip.toString());
        } else {
          Serial.println(F("[WARN] Static IP Configuration Failed. Falling back to DHCP."));
        }
      }
      WiFi.begin(ssid.c_str(), password.c_str());
  }
  
  status_led_wifi_connecting();
  
  Serial.print(F("[INFO] Connecting to WiFi: "));
  Serial.println(ssid);
  
  // Wait for connection
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[INFO] WiFi connected: " + WiFi.localIP().toString());
    status_led_connected();
    
    // Save to RTC for fast reconnect
    rtc_ip = WiFi.localIP();
    rtc_gw = WiFi.gatewayIP();
    rtc_mask = WiFi.subnetMask();
    rtc_channel = WiFi.channel();
    memcpy(rtc_bssid, WiFi.BSSID(), 6);
    rtc_valid = true;
    
    return true;
  } else {
    Serial.println(F("\n[ERROR] WiFi connect failed."));
    status_led_error();
    rtc_valid = false; // Invalidate RTC data
    return false;
  }
}

void WiFiManager::startAPMode() {
  Serial.println(F("[INFO] Starting AP mode"));
  status_led_wifi_connecting(); 
  _apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print(F("[INFO] AP IP address: "));
  Serial.println(IP);
}

bool WiFiManager::isInAPMode() {
  return _apMode;
}

int WiFiManager::scanNetworks() {
  Serial.println("[INFO] Scanning for networks...");
  
  // Scan for networks
  int found = WiFi.scanNetworks();
  
  // Cap at MAX_SCAN_NETWORKS to prevent excessive memory use
  _scannedNetworksCount = (found > MAX_SCAN_NETWORKS) ? MAX_SCAN_NETWORKS : found;
  
  if (_scannedNetworksCount > 0) {
    // Store network details into fixed array (no heap allocation)
    for (int i = 0; i < _scannedNetworksCount; i++) {
      strncpy(_scannedNetworks[i].ssid, WiFi.SSID(i).c_str(), sizeof(_scannedNetworks[i].ssid) - 1);
      _scannedNetworks[i].ssid[sizeof(_scannedNetworks[i].ssid) - 1] = '\0';
      _scannedNetworks[i].rssi = WiFi.RSSI(i);
      _scannedNetworks[i].encType = WiFi.encryptionType(i);
    }
  }
  
  // Free scan results from WiFi driver
  WiFi.scanDelete();
  
  Serial.printf("[INFO] Found %d networks (showing %d)\n", found, _scannedNetworksCount);
  return _scannedNetworksCount;
}

WiFiNetwork* WiFiManager::getScannedNetworks() {
  return _scannedNetworks;
}

int WiFiManager::getScannedNetworksCount() {
  return _scannedNetworksCount;
}

bool WiFiManager::saveCredentials(const String& ssid, const String& password) {
  StaticJsonDocument<256> doc;
  doc["ssid"] = ssid;
  doc["password"] = password;
  
  File file = SPIFFS.open(_credentialsFile, "w");
  if (!file) {
    Serial.println("[ERROR] Failed to open credentials file for writing");
    return false;
  }
  
  if (serializeJson(doc, file) == 0) {
    Serial.println("[ERROR] Failed to write credentials to file");
    file.close();
    return false;
  }
  
  file.close();
  Serial.println("[INFO] WiFi credentials saved");
  return true;
}

WiFiCredentials WiFiManager::loadCredentials() {
  WiFiCredentials creds;
  
  if (!SPIFFS.exists(_credentialsFile)) {
    Serial.println("[INFO] Credentials file does not exist");
    return creds; // Return empty credentials
  }
  
  File file = SPIFFS.open(_credentialsFile, "r");
  if (!file) {
    Serial.println("[ERROR] Failed to open credentials file for reading");
    return creds;
  }
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.print("[ERROR] Failed to parse credentials JSON: ");
    Serial.println(error.c_str());
    return creds;
  }
  
  creds.ssid = doc["ssid"].as<String>();
  creds.password = doc["password"].as<String>();
  
  Serial.println("[INFO] WiFi credentials loaded");
  return creds;
}

IPAddress WiFiManager::getLocalIP() {
  if (_apMode) {
    return WiFi.softAPIP();
  } else {
    return WiFi.localIP();
  }
}

String WiFiManager::getSSID() {
  if (_apMode) {
    return String(AP_SSID);
  } else {
    return WiFi.SSID();
  }
}