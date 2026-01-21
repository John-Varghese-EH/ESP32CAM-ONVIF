#include <Arduino.h>
#include "camera_control.h"
#define CAM_TASK_STACK_SIZE 16384
#include "esp_camera.h"
#include "config.h"
#include "board_config.h"
#if PTZ_ENABLED
#include <ESP32Servo.h>
Servo servoPan;   // Match names used in web_config.cpp
Servo servoTilt;  // Match names used in web_config.cpp
#endif

bool camera_init() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  
  // Use board-specific pin definitions from board_config.h
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA; // 640x480 - Rock Solid Stability for NVRs
    config.jpeg_quality = 12;          // High quality (lower num)
    config.fb_count = 2;
    Serial.println(F("[INFO] PSRAM found. Using VGA (640x480) and 2 Frame Buffers"));
  } else {
    config.frame_size = FRAMESIZE_VGA; // Non-PSRAM cannot do HD well, fallback to SVGA
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println(F("[WARN] No PSRAM. Using VGA and 1 Frame Buffer"));
  }
  
  // config.frame_size = FRAMESIZE_QVGA; // Reduce frame size to save memory // OLD FRAME SIZE
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init failed: 0x%x\n", err);
    return false;
  }
  Serial.println("[INFO] Camera initialized.");
  
  if (FLASH_LED_ENABLED) {
    init_flash_led();
  }
  
  #if PTZ_ENABLED
    ptz_init();
  #endif

  return true;
}

void init_flash_led() {
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, FLASH_LED_INVERT ? HIGH : LOW); // Off by default
}

void set_flash_led(bool on) {
    if (!FLASH_LED_ENABLED) return;
    int state = on ? HIGH : LOW;
    if (FLASH_LED_INVERT) state = !state;
    digitalWrite(FLASH_LED_PIN, state);
}

#if PTZ_ENABLED
void ptz_init() {
    // Basic Servo init
    servoPan.setPeriodHertz(50); 
    servoPan.attach(SERVO_PAN_PIN, 500, 2400);
    servoTilt.setPeriodHertz(50);
    servoTilt.attach(SERVO_TILT_PIN, 500, 2400);
    
    // Center alignment
    servoPan.write(90);
    servoTilt.write(90);
    Serial.println("[INFO] PTZ Servos initialized.");
}

// x, y are -1.0 to 1.0 (ONVIF standard usually) or 0.0 to 1.0
// We'll assume input 0.0 to 1.0 for absolute move
void ptz_set_absolute(float x, float y) {
    int panAngle = (int)(x * 180.0f);
    int tiltAngle = (int)(y * 180.0f);
    
    // Constrain
    if (panAngle < 0) panAngle = 0; if (panAngle > 180) panAngle = 180;
    if (tiltAngle < 0) tiltAngle = 0; if (tiltAngle > 180) tiltAngle = 180;
    
    servoPan.write(panAngle);
    servoTilt.write(tiltAngle);
}

// Simple step move for continuous simulation
void ptz_move(float x_speed, float y_speed) {
    // Read current? Servo libs don't usually read back position easily.
    // We'll skip continuous move logic for now and stick to Absolute or Presets
    // since we don't track state well without a wrapper class.
}
#endif

