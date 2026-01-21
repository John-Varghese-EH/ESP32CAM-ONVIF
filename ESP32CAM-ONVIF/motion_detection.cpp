#include "config.h"
#if ENABLE_MOTION_DETECTION

#include <Arduino.h>
#include "motion_detection.h"

// Basic frame-difference motion detection stub
static bool motion = false;

void motion_detection_init() {
  // Allocation would happen here
}

void motion_detection_loop() {
  // logic to update 'motion' variable would go here
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
