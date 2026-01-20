#include "config.h"
#if STATUS_LED_ENABLED

#include <Arduino.h>
#include "status_led.h"
#include "board_config.h"

// Patterns enumerator
enum LedPattern {
    LED_OFF,
    LED_SOLID,
    LED_SLOW_BLINK, // WiFi Connecting
    LED_FAST_BLINK  // Error
};

static LedPattern _current_pattern = LED_OFF;
static unsigned long _last_toggle = 0;
static bool _led_state = false; // Internal state, will be inverted if needed

void status_led_init() {
    pinMode(STATUS_LED_PIN, OUTPUT);
    status_led_off();
}

static void write_led(bool state) {
    // Apply inversion
    bool pinState = STATUS_LED_INVERT ? !state : state;
    digitalWrite(STATUS_LED_PIN, pinState);
}

void status_led_loop() {
    unsigned long now = millis();
    
    switch (_current_pattern) {
        case LED_OFF:
            write_led(false);
            break;
            
        case LED_SOLID:
            write_led(true);
            break;
            
        case LED_SLOW_BLINK:
            if (now - _last_toggle >= 500) { // 500ms on/off
                _last_toggle = now;
                _led_state = !_led_state;
                write_led(_led_state);
            }
            break;
            
        case LED_FAST_BLINK:
            if (now - _last_toggle >= 100) { // 100ms on/off
                _last_toggle = now;
                _led_state = !_led_state;
                write_led(_led_state);
            }
            break;
    }
}

void status_led_wifi_connecting() {
    _current_pattern = LED_SLOW_BLINK;
}

void status_led_connected() {
    _current_pattern = LED_OFF; // User requested OFF when connected
}

void status_led_error() {
    _current_pattern = LED_FAST_BLINK;
}

void status_led_off() {
    _current_pattern = LED_OFF;
    write_led(false);
}

// Blocking flash helper for startup/events
void status_led_flash(int count, int delayMs) {
    // Ensure we start from known state
    write_led(false);
    delay(100);
    
    for (int i = 0; i < count; i++) {
        write_led(true);
        delay(delayMs);
        write_led(false);
        if (i < count - 1) delay(delayMs);
    }
}

#else
// Stub implementations when status LED is disabled
void status_led_init() {}
void status_led_loop() {}
void status_led_wifi_connecting() {}
void status_led_connected() {}
void status_led_error() {}
void status_led_off() {}
void status_led_flash(int count, int delayMs) {}
#endif
