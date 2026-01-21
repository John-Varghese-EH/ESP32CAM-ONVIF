#pragma once

// ESP32 Platform Detection - Check multiple possible macros
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM) || defined(_ESP32_HAL_H_)
    #include "platglue-esp32.h"
#elif defined(ARDUINO)
    // If ARDUINO is defined but ESP32 is not, something is wrong with board selection
    #error "ESP32 board not selected! Go to Tools > Board > ESP32 Arduino > AI Thinker ESP32-CAM"
#else
    #include "platglue-posix.h"
#endif
