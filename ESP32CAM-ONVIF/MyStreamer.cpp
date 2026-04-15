// MyStreamer.cpp
#include "MyStreamer.h"

// Safe helper to get camera resolution without relying on external arrays
static int getCamWidth() {
    sensor_t *s = esp_camera_sensor_get();
    if (s && s->status.framesize == FRAMESIZE_VGA) return 640;
    return 640; // Default fallback
}

static int getCamHeight() {
    sensor_t *s = esp_camera_sensor_get();
    if (s && s->status.framesize == FRAMESIZE_VGA) return 480;
    return 480; // Default fallback
}

MyStreamer::MyStreamer() : CStreamer(NULL, getCamWidth(), getCamHeight()) {
    // We initialize CStreamer with the known width/height.
}

void MyStreamer::streamImage(uint32_t curMsec) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera frame buffer could not be acquired");
        return;
    }
    
    if (fb->format == PIXFORMAT_JPEG && fb->len > 0) {
        streamFrame(fb->buf, fb->len, curMsec);
    }
    esp_camera_fb_return(fb);
}
