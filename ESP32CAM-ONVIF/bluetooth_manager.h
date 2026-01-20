#pragma once
#include <Arduino.h>
#include "config.h"

class BluetoothManager {
public:
    BluetoothManager();
    void begin();
    void loop();
    
    // Status
    bool isUserPresent();
    bool isStealthActive(); // True if (WiFi Fail && !UserPresent)
    
    // Diagnostics
    String getLastScanResult(); 

private:
    void startScan();
    void onScanComplete(int count);
    
    unsigned long _lastScanTime;
    unsigned long _stealthCheckTime;
    
    bool _isScanning;
    bool _userPresent;
    unsigned long _userLastSeenTime;
    
    String _scanResultsJSON; // Cached JSON for Web UI
};

extern BluetoothManager btManager;
