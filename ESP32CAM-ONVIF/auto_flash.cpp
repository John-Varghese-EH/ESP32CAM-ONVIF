#include "config.h"
#if FLASH_LED_ENABLED

#include <Arduino.h>
#include "auto_flash.h"
#include "camera_control.h"
#include "esp_camera.h"

static bool _auto_enabled = DEFAULT_AUTO_FLASH;
static unsigned long _last_check = 0;
static const unsigned long CHECK_INTERVAL = 2000; // Check every 2 seconds

void auto_flash_init() {
    _auto_enabled = DEFAULT_AUTO_FLASH;
}

void auto_flash_set_enabled(bool enabled) {
    _auto_enabled = enabled;
    if (!enabled) {
        set_flash_led(false);
    }
}

bool auto_flash_is_enabled() {
    return _auto_enabled;
}

void auto_flash_loop() {
    if (!_auto_enabled) return;
    
    if (millis() - _last_check < CHECK_INTERVAL) return;
    _last_check = millis();

    sensor_t * s = esp_camera_sensor_get();
    if (!s) return;
    
    int exposure = s->status.aec_value; 
    int gain = s->status.agc_gain;
    
    bool is_dark = (exposure > 600) || (gain > 30);
    
    static bool is_led_on = false;
    
    if (is_dark && !is_led_on) {
        set_flash_led(true);
        is_led_on = true;
    } else if (!is_dark && is_led_on) {
        set_flash_led(false);
        is_led_on = false;
    }
}

#else
// Stub implementations when flash LED is disabled
void auto_flash_init() {}
void auto_flash_set_enabled(bool enabled) {}
bool auto_flash_is_enabled() { return false; }
void auto_flash_loop() {}
#endif
