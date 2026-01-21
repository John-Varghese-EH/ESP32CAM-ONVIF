#include "config.h"
#ifdef BLUETOOTH_ENABLED

#include "audio_manager.h"
#include "board_config.h"

// Note: Full I2S and HFP Implementation is complex.
// This is a stub to allow the rest of the system to compile and switch modes.
// Actual I2S driver code or HFP handling should be inserted here.

AudioManager audioManager;

AudioManager::AudioManager() {
    _initialized = false;
    _currentSource = AUDIO_SOURCE_NONE;
}

bool AudioManager::begin() {
    _currentSource = appSettings.audioSource;
    
    if (_currentSource == AUDIO_SOURCE_NONE) return true;
    
    Serial.printf("[AUDIO] Initializing Source: %d\n", _currentSource);
    
    if (_currentSource == AUDIO_SOURCE_HARDWARE_I2S) {
        // Init I2S Driver here
        // i2s_driver_install(...)
        // i2s_set_pin(...)
        Serial.println("[AUDIO] I2S Hardware Mic Initialized (Stub)");
    } 
    else if (_currentSource == AUDIO_SOURCE_BLUETOOTH_HFP) {
        // Init HFP here
        // esp_hf_client_init()
        // esp_hf_client_register_callback()
        Serial.println("[AUDIO] Bluetooth HFP Initialized (Stub)");
        
        if (!appSettings.btEnabled) {
            Serial.println("[WARN] Bluetooth is disabled! HFP will not work.");
        }
    }
    
    _initialized = true;
    return true;
}

size_t AudioManager::read(uint8_t* buffer, size_t size) {
    if (!_initialized) return 0;
    
    // Return silence for now
    memset(buffer, 0, size);
    
    // If we had I2S:
    // i2s_read(I2S_NUM_0, buffer, size, &bytes_read, portMAX_DELAY);
    
    return size; // Simulate data flow
}

void AudioManager::end() {
    _initialized = false;
    // i2s_driver_uninstall(I2S_NUM_0);
}

#endif // BLUETOOTH_ENABLED
