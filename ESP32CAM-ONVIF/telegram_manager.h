#pragma once

#include "config.h"
#include "esp_camera.h"

// Send a captured frame as a photo to the configured Telegram chat
// Returns true on success, false on failure
bool telegram_send_photo(camera_fb_t* fb);

// Send a text message to the configured Telegram chat
bool telegram_send_message(const char* message);
