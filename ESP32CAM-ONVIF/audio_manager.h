#pragma once
#include <Arduino.h>
#include "config.h"

// Abstract audio capture buffer frame size (e.g. 16ms of 16k mono)
#define AUDIO_CHUNK_SIZE 256 

class AudioManager {
public:
    AudioManager();
    
    // Initialize based on appSettings.audioSource
    bool begin();
    
    // Read PCM samples into buffer. 
    // Returns number of bytes read (0 if no audio available)
    size_t read(uint8_t* buffer, size_t size);
    
    // Stop/Deinit
    void end();

private:
    bool _initialized;
    AudioSource _currentSource;
    
    // HFP Ring Buffer (If implemented)
    // I2S Handle (If implemented)
};

extern AudioManager audioManager;
