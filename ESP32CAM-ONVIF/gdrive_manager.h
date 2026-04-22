#pragma once

#include <Arduino.h>
#include "esp_camera.h"

void initGDrive();
void uploadToGDriveAsync(camera_fb_t* fb);
