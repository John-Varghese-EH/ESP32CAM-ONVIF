#include "config.h"
#ifdef BLUETOOTH_ENABLED

#include "bluetooth_manager.h"
#include <WiFi.h>
#include "status_led.h"
#include "wifi_manager.h"

// Include appropriate Bluetooth library based on config
#ifdef USE_NIMBLE
  #include <NimBLEDevice.h>
  #define BT_STACK "NimBLE"
#else
  #include <BLEDevice.h>
  #include <BLEScan.h>
  #include <BLEAdvertisedDevice.h>
  #define BT_STACK "Bluedroid"
#endif

// Tunables
#define SCAN_INTERVAL_MS      60000   // Scan every 60s
#define SCAN_DURATION_SEC     2       // Scan for 2s

BluetoothManager btManager;

BluetoothManager::BluetoothManager() {
    _lastScanTime = 0;
    _stealthCheckTime = 0;
    _isScanning = false;
    _userPresent = false;
    _userLastSeenTime = 0;
    _scanResultsJSON = "[]";
}

void BluetoothManager::begin() {
    if (!appSettings.btEnabled) return;
    
    Serial.print("[INFO] Initializing Bluetooth (");
    Serial.print(BT_STACK);
    Serial.println(")...");
    
    #ifdef USE_NIMBLE
        NimBLEDevice::init(String(DEVICE_HARDWARE_ID).c_str());
        NimBLEDevice::setPower(ESP_PWR_LVL_P7);
        
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(false);
        pScan->setInterval(100);
        pScan->setWindow(99);
    #else
        BLEDevice::init(String(DEVICE_HARDWARE_ID).c_str());
        
        BLEScan* pScan = BLEDevice::getScan();
        pScan->setActiveScan(false);
        pScan->setInterval(100);
        pScan->setWindow(99);
    #endif
}

void BluetoothManager::loop() {
    if (!appSettings.btEnabled) return;
    
    unsigned long now = millis();
    
    // 1. Periodic Scan
    if (!_isScanning && (now - _lastScanTime > SCAN_INTERVAL_MS)) {
        startScan();
    }
    
    // 2. Stealth Logic Update
    if (appSettings.btStealthMode && (now - _stealthCheckTime > 2000)) {
        _stealthCheckTime = now;
        bool wifiDown = (WiFi.status() != WL_CONNECTED);
        
        if (wifiDown && !isUserPresent()) {
            status_led_off(); 
        }
    }
}

void BluetoothManager::startScan() {
    Serial.println("[BLE] Starting scan...");
    
    #ifdef USE_NIMBLE
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->start(SCAN_DURATION_SEC, false);  // Returns bool, not results
        NimBLEScanResults results = pScan->getResults();
        
        Serial.print("[BLE] Scan done. Found: ");
        Serial.println(results.getCount());
        
        _scanResultsJSON = "[";
        bool foundUser = false;
        String target = appSettings.btPresenceMac;
        target.toUpperCase();

        for(int i=0; i<results.getCount(); i++) {
            const NimBLEAdvertisedDevice* device = results.getDevice(i);  // Returns const pointer
            if (i > 0) _scanResultsJSON += ",";
            
            _scanResultsJSON += "{\"mac\":\"";
            _scanResultsJSON += device->getAddress().toString().c_str();
            _scanResultsJSON += "\",\"rssi\":";
            _scanResultsJSON += device->getRSSI();
            _scanResultsJSON += ",\"name\":\"";
            _scanResultsJSON += device->getName().c_str();
            _scanResultsJSON += "\"}";
            
            String mac = device->getAddress().toString().c_str();
            mac.toUpperCase();
            if (target.length() > 0 && mac == target) {
                foundUser = true;
            }
        }
        _scanResultsJSON += "]";
        pScan->clearResults();
        
    #else
        BLEScan* pScan = BLEDevice::getScan();
        BLEScanResults* foundDevices = pScan->start(SCAN_DURATION_SEC, false);
        
        Serial.print("[BLE] Scan done. Found: ");
        Serial.println(foundDevices->getCount());
        
        _scanResultsJSON = "[";
        bool foundUser = false;
        String target = appSettings.btPresenceMac;
        target.toUpperCase();

        for(int i=0; i<foundDevices->getCount(); i++) {
            BLEAdvertisedDevice device = foundDevices->getDevice(i);
            if (i > 0) _scanResultsJSON += ",";
            
            _scanResultsJSON += "{\"mac\":\"";
            _scanResultsJSON += device.getAddress().toString().c_str();
            _scanResultsJSON += "\",\"rssi\":";
            _scanResultsJSON += device.getRSSI();
            _scanResultsJSON += ",\"name\":\"";
            _scanResultsJSON += device.getName().c_str();
            _scanResultsJSON += "\"}";
            
            String mac = device.getAddress().toString().c_str();
            mac.toUpperCase();
            if (target.length() > 0 && mac == target) {
                foundUser = true;
            }
        }
        _scanResultsJSON += "]";
        pScan->clearResults();
    #endif
    
    if (foundUser) {
        _userPresent = true;
        _userLastSeenTime = millis();
        Serial.println("[BLE] User PRESENT");
    } else {
        unsigned long timeoutMs = appSettings.btPresenceTimeout * 1000;
        if (millis() - _userLastSeenTime > timeoutMs) {
            _userPresent = false;
            Serial.println("[BLE] User ABSENT");
        }
    }
    
    _lastScanTime = millis();
}

bool BluetoothManager::isUserPresent() {
    if (!appSettings.btPresenceMac.length()) return true; 
    return _userPresent;
}

bool BluetoothManager::isStealthActive() {
    if (!appSettings.btStealthMode) return false;
    return (WiFi.status() != WL_CONNECTED) && !isUserPresent();
}

String BluetoothManager::getLastScanResult() {
    return _scanResultsJSON;
}

#endif // BLUETOOTH_ENABLED
