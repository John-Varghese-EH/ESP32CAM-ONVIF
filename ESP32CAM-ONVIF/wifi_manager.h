#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>

// WiFi credentials structure
struct WiFiCredentials {
  String ssid;
  String password;
};

// WiFi network info structure for scanning
#define MAX_SCAN_NETWORKS 16  // Cap to prevent heap exhaustion in dense WiFi areas
struct WiFiNetwork {
  char ssid[33];       // Max SSID length = 32 + null (was heap-allocated String)
  int32_t rssi;
  uint8_t encType;
};

// WiFi manager class
class WiFiManager {
public:
  WiFiManager();
  
  // Connect using stored credentials or fall back to AP mode
  bool begin();
  
  // Direct connection methods
  bool connectToStoredNetwork();
  bool connectToNetwork(const String& ssid, const String& password);
  
  // AP mode functions
  void startAPMode();
  bool isInAPMode();
  
  // Scanning functions
  int scanNetworks();
  WiFiNetwork* getScannedNetworks();
  int getScannedNetworksCount();
  
  // Credential management
  bool saveCredentials(const String& ssid, const String& password);
  WiFiCredentials loadCredentials();
  
  IPAddress getLocalIP();
  String getSSID();
  // int getScannedNetworksCount(); // Removed duplicate
  
  // Connectivity check helper
  bool checkConnectivity();
  
  void loop(); // Handle reconnection
  
private:
  bool _apMode;
  int _scannedNetworksCount;
  WiFiNetwork _scannedNetworks[MAX_SCAN_NETWORKS]; // Fixed array, no dynamic allocation
  unsigned long _lastConnectAttempt;
  
  // Stability Tracking
  unsigned long _lastConnectedTime; // Last time we had a valid connection
  const unsigned long _wifiTimeoutMs = 300000; // 5 minutes timeout for auto-reboot
  
  // Constants
  const char* _credentialsFile = "/wifi_creds.json";
};

// Global instance
extern WiFiManager wifiManager;