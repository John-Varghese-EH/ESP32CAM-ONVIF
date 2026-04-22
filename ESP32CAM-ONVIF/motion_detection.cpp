#include "config.h"
#if ENABLE_MOTION_DETECTION

#include <Arduino.h>
#include "motion_detection.h"
#include "esp_camera.h"

// Frame-difference motion detection using average luminance
// Lightweight: ~2ms per check at VGA, no PSRAM needed
static bool motion = false;
static unsigned long _last_check = 0;
static uint32_t _prev_avg_luma = 0;
static bool _has_baseline = false;

// Tuning constants
#define MOTION_CHECK_INTERVAL_MS 1000   // Check every 1 second
#define MOTION_THRESHOLD         15     // Avg luminance change to trigger (0-255)
#define MOTION_COOLDOWN_MS       5000   // Stay "detected" for at least 5s
static unsigned long _last_motion_time = 0;

void motion_detection_init() {
    motion = false;
    _has_baseline = false;
    _prev_avg_luma = 0;
    Serial.println(F("[INFO] Motion detection initialized"));
}

// Calculate average luminance from first N bytes of JPEG
// JPEG starts with SOI (0xFFD8). We sample raw bytes as a proxy for brightness.
// This is fast and avoids full JPEG decode.
static uint32_t calc_avg_luma_fast(const uint8_t* buf, size_t len) {
    // Sample the middle portion of the JPEG data (skip headers)
    size_t start = len / 4;
    size_t end = (len * 3) / 4;
    if (end > len) end = len;
    size_t count = 0;
    uint32_t sum = 0;
    // Sample every 64th byte for speed
    for (size_t i = start; i < end; i += 64) {
        sum += buf[i];
        count++;
    }
    return count > 0 ? sum / count : 128;
}

void motion_detection_loop() {
    if (millis() - _last_check < MOTION_CHECK_INTERVAL_MS) return;
    _last_check = millis();

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return;

    uint32_t avg_luma = calc_avg_luma_fast(fb->buf, fb->len);
    esp_camera_fb_return(fb);

    if (!_has_baseline) {
        _prev_avg_luma = avg_luma;
        _has_baseline = true;
        return;
    }

    int32_t diff = (int32_t)avg_luma - (int32_t)_prev_avg_luma;
    if (diff < 0) diff = -diff;  // abs()

    if (diff > MOTION_THRESHOLD) {
        if (!motion) {
            Serial.printf("[MOTION] Detected! delta=%ld\n", (long)diff);
        }
        motion = true;
        _last_motion_time = millis();
    } else if (motion && (millis() - _last_motion_time > MOTION_COOLDOWN_MS)) {
        motion = false;
    }

    _prev_avg_luma = avg_luma;
}

bool motion_detected() {
    return motion;
}

#else
// Stub implementations when disabled
void motion_detection_init() {}
void motion_detection_loop() {}
bool motion_detected() { return false; }
#endif
