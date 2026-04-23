// MyStreamer.cpp
#include "MyStreamer.h"

// The `resolution` array is a standard part of the esp32-camera driver component.
// It maps the framesize enum to width and height.
extern "C" {
    #include "ll_cam.h"
}

MyStreamer::MyStreamer() : CStreamer(NULL, resolution[esp_camera_sensor_get()->status.framesize].width, resolution[esp_camera_sensor_get()->status.framesize].height) {
    // The CStreamer base class constructor needs the image width and height.
    // We get it from the currently configured camera sensor.
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
