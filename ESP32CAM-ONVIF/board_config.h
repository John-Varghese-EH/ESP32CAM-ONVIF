#pragma once
// ==============================================================================
//   Board-Specific Camera Pin Definitions
// ==============================================================================
// This file contains pin mappings for various ESP32 camera boards.
// Select your board in config.h using the BOARD_* definitions.
// ==============================================================================

#include "config.h"

// ==============================================================================
//   AI-Thinker ESP32-CAM (Most Common)
// ==============================================================================
#if defined(BOARD_AI_THINKER_ESP32CAM)
    #define PWDN_GPIO_NUM     32
    #define RESET_GPIO_NUM    -1
    #define XCLK_GPIO_NUM      0
    #define SIOD_GPIO_NUM     26
    #define SIOC_GPIO_NUM     27
    #define Y9_GPIO_NUM       35
    #define Y8_GPIO_NUM       34
    #define Y7_GPIO_NUM       39
    #define Y6_GPIO_NUM       36
    #define Y5_GPIO_NUM       21
    #define Y4_GPIO_NUM       19
    #define Y3_GPIO_NUM       18
    #define Y2_GPIO_NUM        5
    #define VSYNC_GPIO_NUM    25
    #define HREF_GPIO_NUM     23
    #define PCLK_GPIO_NUM     22
    #define BOARD_NAME        "AI-Thinker ESP32-CAM"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32"
    // LED and Servo pins for AI-Thinker
    #define FLASH_LED_PIN     4           // Flash LED (warning: conflicts with SD card D1)
    #define STATUS_LED_PIN    33          // Blue LED on module
    #define SERVO_PAN_PIN     12          // Pan servo (safe GPIO)
    #define SERVO_TILT_PIN    13          // Tilt servo (safe GPIO)

// ==============================================================================
//   M5Stack Camera Models
// ==============================================================================
#elif defined(BOARD_M5STACK_CAMERA)
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    15
    #define XCLK_GPIO_NUM     27
    #define SIOD_GPIO_NUM     25
    #define SIOC_GPIO_NUM     23
    #define Y9_GPIO_NUM       19
    #define Y8_GPIO_NUM       36
    #define Y7_GPIO_NUM       18
    #define Y6_GPIO_NUM       39
    #define Y5_GPIO_NUM        5
    #define Y4_GPIO_NUM       34
    #define Y3_GPIO_NUM       35
    #define Y2_GPIO_NUM       32
    #define VSYNC_GPIO_NUM    22
    #define HREF_GPIO_NUM     26
    #define PCLK_GPIO_NUM     21
    #define BOARD_NAME        "M5Stack Camera"
    #define HAS_PSRAM         false
    #define CHIP_TYPE         "ESP32"
    // M5Stack Camera has no external flash LED, status LED on GPIO 14
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    14          // Red LED
    #define SERVO_PAN_PIN     2           // Safe GPIO
    #define SERVO_TILT_PIN    16          // Safe GPIO

#elif defined(BOARD_M5STACK_PSRAM)
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    15
    #define XCLK_GPIO_NUM     27
    #define SIOD_GPIO_NUM     22
    #define SIOC_GPIO_NUM     23
    #define Y9_GPIO_NUM       19
    #define Y8_GPIO_NUM       36
    #define Y7_GPIO_NUM       18
    #define Y6_GPIO_NUM       39
    #define Y5_GPIO_NUM        5
    #define Y4_GPIO_NUM       34
    #define Y3_GPIO_NUM       35
    #define Y2_GPIO_NUM       32
    #define VSYNC_GPIO_NUM    25
    #define HREF_GPIO_NUM     26
    #define PCLK_GPIO_NUM     21
    #define BOARD_NAME        "M5Stack Camera PSRAM"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32"
    // M5Stack PSRAM Camera
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    14          // Red LED
    #define SERVO_PAN_PIN     2           // Safe GPIO
    #define SERVO_TILT_PIN    16          // Safe GPIO

#elif defined(BOARD_M5STACK_WIDE)
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    15
    #define XCLK_GPIO_NUM     27
    #define SIOD_GPIO_NUM     22
    #define SIOC_GPIO_NUM     23
    #define Y9_GPIO_NUM       19
    #define Y8_GPIO_NUM       36
    #define Y7_GPIO_NUM       18
    #define Y6_GPIO_NUM       39
    #define Y5_GPIO_NUM        5
    #define Y4_GPIO_NUM       34
    #define Y3_GPIO_NUM       35
    #define Y2_GPIO_NUM       32
    #define VSYNC_GPIO_NUM    25
    #define HREF_GPIO_NUM     26
    #define PCLK_GPIO_NUM     21
    #define BOARD_NAME        "M5Stack Wide Camera"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32"
    // M5Stack Wide Camera
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    14          // Red LED
    #define SERVO_PAN_PIN     2           // Safe GPIO
    #define SERVO_TILT_PIN    16          // Safe GPIO

#elif defined(BOARD_M5STACK_UNITCAM)
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    15
    #define XCLK_GPIO_NUM     27
    #define SIOD_GPIO_NUM     25
    #define SIOC_GPIO_NUM     23
    #define Y9_GPIO_NUM       19
    #define Y8_GPIO_NUM       36
    #define Y7_GPIO_NUM       18
    #define Y6_GPIO_NUM       39
    #define Y5_GPIO_NUM        5
    #define Y4_GPIO_NUM       34
    #define Y3_GPIO_NUM       35
    #define Y2_GPIO_NUM       32
    #define VSYNC_GPIO_NUM    22
    #define HREF_GPIO_NUM     26
    #define PCLK_GPIO_NUM     21
    #define BOARD_NAME        "M5Stack UnitCam"
    #define HAS_PSRAM         false
    #define CHIP_TYPE         "ESP32"
    // M5Stack UnitCam - minimal GPIO available
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    -1          // No status LED
    #define SERVO_PAN_PIN     -1          // No safe GPIO for servos
    #define SERVO_TILT_PIN    -1          // No safe GPIO for servos

// ==============================================================================
//   TTGO T-Camera Models
// ==============================================================================
#elif defined(BOARD_TTGO_T_CAMERA)
    #define PWDN_GPIO_NUM     26
    #define RESET_GPIO_NUM    -1
    #define XCLK_GPIO_NUM     32
    #define SIOD_GPIO_NUM     13
    #define SIOC_GPIO_NUM     12
    #define Y9_GPIO_NUM       39
    #define Y8_GPIO_NUM       36
    #define Y7_GPIO_NUM       23
    #define Y6_GPIO_NUM       18
    #define Y5_GPIO_NUM       15
    #define Y4_GPIO_NUM        4
    #define Y3_GPIO_NUM       14
    #define Y2_GPIO_NUM        5
    #define VSYNC_GPIO_NUM    27
    #define HREF_GPIO_NUM     25
    #define PCLK_GPIO_NUM     19
    #define BOARD_NAME        "TTGO T-Camera"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32"
    // TTGO T-Camera - GPIO 2 free, GPIO 33/34 input only
    #define FLASH_LED_PIN     -1          // No dedicated flash
    #define STATUS_LED_PIN    2           // Use GPIO 2 for status
    #define SERVO_PAN_PIN     33          // Input-only but can use for output
    #define SERVO_TILT_PIN    -1          // Limited GPIOs available

#elif defined(BOARD_TTGO_T_JOURNAL)
    #define PWDN_GPIO_NUM      0
    #define RESET_GPIO_NUM    15
    #define XCLK_GPIO_NUM     27
    #define SIOD_GPIO_NUM     25
    #define SIOC_GPIO_NUM     23
    #define Y9_GPIO_NUM       19
    #define Y8_GPIO_NUM       36
    #define Y7_GPIO_NUM       18
    #define Y6_GPIO_NUM       39
    #define Y5_GPIO_NUM        5
    #define Y4_GPIO_NUM       34
    #define Y3_GPIO_NUM       35
    #define Y2_GPIO_NUM       17
    #define VSYNC_GPIO_NUM    22
    #define HREF_GPIO_NUM     26
    #define PCLK_GPIO_NUM     21
    #define BOARD_NAME        "TTGO T-Journal"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32"
    // TTGO T-Journal
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    2           // GPIO 2
    #define SERVO_PAN_PIN     12          // Safe GPIO
    #define SERVO_TILT_PIN    13          // Safe GPIO

// ==============================================================================
//   Espressif Development Boards
// ==============================================================================
#elif defined(BOARD_WROVER_KIT)
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    -1
    #define XCLK_GPIO_NUM     21
    #define SIOD_GPIO_NUM     26
    #define SIOC_GPIO_NUM     27
    #define Y9_GPIO_NUM       35
    #define Y8_GPIO_NUM       34
    #define Y7_GPIO_NUM       39
    #define Y6_GPIO_NUM       36
    #define Y5_GPIO_NUM       19
    #define Y4_GPIO_NUM       18
    #define Y3_GPIO_NUM        5
    #define Y2_GPIO_NUM        4
    #define VSYNC_GPIO_NUM    25
    #define HREF_GPIO_NUM     23
    #define PCLK_GPIO_NUM     22
    #define BOARD_NAME        "ESP-WROVER-KIT"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32"
    // WROVER-KIT has RGB LED
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    2           // Red LED on GPIO 2
    #define SERVO_PAN_PIN     12          // Safe GPIO
    #define SERVO_TILT_PIN    13          // Safe GPIO

#elif defined(BOARD_ESP_EYE)
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    -1
    #define XCLK_GPIO_NUM      4
    #define SIOD_GPIO_NUM     18
    #define SIOC_GPIO_NUM     23
    #define Y9_GPIO_NUM       36
    #define Y8_GPIO_NUM       37
    #define Y7_GPIO_NUM       38
    #define Y6_GPIO_NUM       39
    #define Y5_GPIO_NUM       35
    #define Y4_GPIO_NUM       14
    #define Y3_GPIO_NUM       13
    #define Y2_GPIO_NUM       34
    #define VSYNC_GPIO_NUM     5
    #define HREF_GPIO_NUM     27
    #define PCLK_GPIO_NUM     25
    #define BOARD_NAME        "ESP-EYE"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32"
    // ESP-EYE - LED on GPIO 22
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    22          // White LED
    #define SERVO_PAN_PIN     2           // Safe GPIO
    #define SERVO_TILT_PIN    15          // Safe GPIO

// ==============================================================================
//   ESP32-S3 Boards (Support H.264 Software Encoding)
// ==============================================================================
#elif defined(BOARD_FREENOVE_ESP32S3)
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    -1
    #define XCLK_GPIO_NUM     15
    #define SIOD_GPIO_NUM      4
    #define SIOC_GPIO_NUM      5
    #define Y9_GPIO_NUM       16
    #define Y8_GPIO_NUM       17
    #define Y7_GPIO_NUM       18
    #define Y6_GPIO_NUM       12
    #define Y5_GPIO_NUM       10
    #define Y4_GPIO_NUM        8
    #define Y3_GPIO_NUM        9
    #define Y2_GPIO_NUM       11
    #define VSYNC_GPIO_NUM     6
    #define HREF_GPIO_NUM      7
    #define PCLK_GPIO_NUM     13
    #define BOARD_NAME        "Freenove ESP32-S3-WROOM CAM"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32-S3"
    #define H264_CAPABLE      true    // Can use software H.264 encoder
    // Freenove ESP32-S3 - LED on GPIO 2, flash on GPIO 48
    #define FLASH_LED_PIN     48          // Built-in flash LED
    #define STATUS_LED_PIN    2           // Built-in LED
    #define SERVO_PAN_PIN     1           // Safe GPIO
    #define SERVO_TILT_PIN    3           // Safe GPIO

#elif defined(BOARD_SEEED_XIAO_S3)
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    -1
    #define XCLK_GPIO_NUM     10
    #define SIOD_GPIO_NUM     40
    #define SIOC_GPIO_NUM     39
    #define Y9_GPIO_NUM       48
    #define Y8_GPIO_NUM       11
    #define Y7_GPIO_NUM       12
    #define Y6_GPIO_NUM       14
    #define Y5_GPIO_NUM       16
    #define Y4_GPIO_NUM       18
    #define Y3_GPIO_NUM       17
    #define Y2_GPIO_NUM       15
    #define VSYNC_GPIO_NUM    38
    #define HREF_GPIO_NUM     47
    #define PCLK_GPIO_NUM     13
    #define BOARD_NAME        "Seeed XIAO ESP32S3 Sense"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32-S3"
    #define H264_CAPABLE      true
    // XIAO ESP32S3 Sense - very limited GPIOs, LED on GPIO 21
    // WARNING: GPIO 44/43 are console UART, avoid for servos
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    21          // Built-in orange LED
    #define SERVO_PAN_PIN     1           // D0 - safe for PWM
    #define SERVO_TILT_PIN    2           // D1 - safe for PWM

#elif defined(BOARD_ESP32S3_EYE)
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    -1
    #define XCLK_GPIO_NUM     15
    #define SIOD_GPIO_NUM      4
    #define SIOC_GPIO_NUM      5
    #define Y9_GPIO_NUM       16
    #define Y8_GPIO_NUM       17
    #define Y7_GPIO_NUM       18
    #define Y6_GPIO_NUM       12
    #define Y5_GPIO_NUM       10
    #define Y4_GPIO_NUM        8
    #define Y3_GPIO_NUM        9
    #define Y2_GPIO_NUM       11
    #define VSYNC_GPIO_NUM     6
    #define HREF_GPIO_NUM      7
    #define PCLK_GPIO_NUM     13
    #define BOARD_NAME        "ESP32-S3-EYE"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32-S3"
    #define H264_CAPABLE      true
    // ESP32-S3-EYE
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    3           // Status LED
    #define SERVO_PAN_PIN     1           // Safe GPIO
    #define SERVO_TILT_PIN    2           // Safe GPIO

// ==============================================================================
//   ESP32-P4 Boards (Support H.264 Hardware Encoding!)
// ==============================================================================
#elif defined(BOARD_ESP32P4_FUNCTION_EV)
    // ESP32-P4-Function-EV-Board with MIPI-CSI camera
    // Note: This board uses MIPI-CSI, pin definitions differ from DVP cameras
    #define PWDN_GPIO_NUM     -1
    #define RESET_GPIO_NUM    -1
    #define XCLK_GPIO_NUM     -1      // MIPI-CSI doesn't use XCLK
    #define SIOD_GPIO_NUM      8      // I2C SDA for camera control
    #define SIOC_GPIO_NUM      7      // I2C SCL for camera control
    // DVP pins not used - MIPI-CSI is used instead
    #define Y9_GPIO_NUM       -1
    #define Y8_GPIO_NUM       -1
    #define Y7_GPIO_NUM       -1
    #define Y6_GPIO_NUM       -1
    #define Y5_GPIO_NUM       -1
    #define Y4_GPIO_NUM       -1
    #define Y3_GPIO_NUM       -1
    #define Y2_GPIO_NUM       -1
    #define VSYNC_GPIO_NUM    -1
    #define HREF_GPIO_NUM     -1
    #define PCLK_GPIO_NUM     -1
    #define BOARD_NAME        "ESP32-P4-Function-EV-Board"
    #define HAS_PSRAM         true
    #define CHIP_TYPE         "ESP32-P4"
    #define H264_CAPABLE      true
    #define H264_HW_ENCODER   true    // Hardware H.264 encoder available!
    #define USE_MIPI_CSI      true    // Uses MIPI-CSI instead of DVP
    // ESP32-P4 EV Board - many GPIOs available
    #define FLASH_LED_PIN     -1          // No flash LED
    #define STATUS_LED_PIN    27          // Use GPIO 27 for status
    #define SERVO_PAN_PIN     26          // Safe GPIO
    #define SERVO_TILT_PIN    25          // Safe GPIO

// ==============================================================================
//   Custom Board - Define Your Own Pins
// ==============================================================================
#elif defined(BOARD_CUSTOM)
    // Define your custom pin mappings here
    #define PWDN_GPIO_NUM     -1      // Power down control (-1 = not used)
    #define RESET_GPIO_NUM    -1      // Reset pin (-1 = not used)
    #define XCLK_GPIO_NUM      0      // External clock
    #define SIOD_GPIO_NUM     26      // I2C SDA
    #define SIOC_GPIO_NUM     27      // I2C SCL
    #define Y9_GPIO_NUM       35      // Data bit 9
    #define Y8_GPIO_NUM       34      // Data bit 8
    #define Y7_GPIO_NUM       39      // Data bit 7
    #define Y6_GPIO_NUM       36      // Data bit 6
    #define Y5_GPIO_NUM       21      // Data bit 5
    #define Y4_GPIO_NUM       19      // Data bit 4
    #define Y3_GPIO_NUM       18      // Data bit 3
    #define Y2_GPIO_NUM        5      // Data bit 2
    #define VSYNC_GPIO_NUM    25      // Vertical sync
    #define HREF_GPIO_NUM     23      // Horizontal reference
    #define PCLK_GPIO_NUM     22      // Pixel clock
    #define BOARD_NAME        "Custom Board"
    #define HAS_PSRAM         true    // Set to false if no PSRAM
    #define CHIP_TYPE         "ESP32" // Change to "ESP32-S3" or "ESP32-P4" if applicable
    // #define H264_CAPABLE   true    // Uncomment for S3/P4
    // #define H264_HW_ENCODER true   // Uncomment for P4 only
    // Custom LED and Servo pins - MODIFY THESE FOR YOUR BOARD
    #define FLASH_LED_PIN     -1          // Set to your flash LED GPIO (-1 = none)
    #define STATUS_LED_PIN    -1          // Set to your status LED GPIO (-1 = none)
    #define SERVO_PAN_PIN     -1          // Set to your pan servo GPIO (-1 = none)
    #define SERVO_TILT_PIN    -1          // Set to your tilt servo GPIO (-1 = none)

#else
    #error "No board selected! Please define one of the BOARD_* options in config.h"
#endif

// ==============================================================================
//   H.264 Capability Check
// ==============================================================================
#ifdef VIDEO_CODEC_H264
    #ifndef H264_CAPABLE
        #error "H.264 encoding requires ESP32-S3 or ESP32-P4! Your selected board doesn't support H.264. Use VIDEO_CODEC_MJPEG instead."
    #endif
#endif

// Default to MJPEG if no codec specified
#if !defined(VIDEO_CODEC_MJPEG) && !defined(VIDEO_CODEC_H264)
    #define VIDEO_CODEC_MJPEG
#endif
