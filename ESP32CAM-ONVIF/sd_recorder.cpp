#include "sd_recorder.h"
#include "FS.h"
#include "SD_MMC.h"
#include "esp_camera.h" // Added for camera functions

#include "config.h"
#include "wifi_manager.h"

  // static internal flag to track state
  static bool _sdMountSuccess = false;

  void sd_recorder_init() {
  _sdMountSuccess = false;
  
  // Optimization: Do not init if recording is disabled
  if (!ENABLE_DAILY_RECORDING) {
      Serial.println("[INFO] Recording disabled. SD Card Init skipped.");
      return;
  }

  bool mountAttempt = false;
  
  if (FLASH_LED_ENABLED) {
    // 1-bit mode to free up GPIO 4 for Flash LED
    mountAttempt = SD_MMC.begin("/sdcard", true);
    if(mountAttempt) Serial.println("[INFO] SD Card mounted in 1-bit mode (Flash enabled).");
  } else {
    // 4-bit mode for higher speed (GPIO 4 used for Data 1)
    mountAttempt = SD_MMC.begin();
    if(mountAttempt) Serial.println("[INFO] SD Card mounted in 4-bit mode (Flash disabled).");
  }

  if(!mountAttempt){
    Serial.println("[WARN] SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("[WARN] No SD Card attached");
    return;
  }
  Serial.println("[INFO] SD Card initialized");
  _sdMountSuccess = true;
}

// --- Recording Globals ---
unsigned long _lastRecordFrame = 0;
unsigned long _currentSegmentStart = 0;
File _recordFile;
bool _isRecording = false;
bool _manualRecording = false; // Flag for manual web trigger
int _segmentCounter = 0;
int _framesSinceFlush = 0;  // Track frames for periodic flush

void manage_storage() {
    float total = SD_MMC.totalBytes();
    float used = SD_MMC.usedBytes();
    float pct = (used / total) * 100.0;
    
    if (pct > MAX_DISK_USAGE_PCT) {
        Serial.printf("[WARN] Disk Usage %.1f%% > %d%%. Cleaning up...\n", pct, MAX_DISK_USAGE_PCT);
        
        File root = SD_MMC.open("/recordings");
        if (!root) return;
        
        // Simple strategy: Delete oldest file
        // Since we name them by timestamp or counter, we could sort.
        // For simplicity, we assume alphabetical is roughly chronological if named properly.
        // Actually, let's just delete the first file we find that is a recording.
        
        File file = root.openNextFile();
        if (file) {
            String path = String("/recordings/") + file.name();
            file.close(); // Close before delete
            Serial.println("[INFO] Deleting old recording: " + path);
            SD_MMC.remove(path);
        }
    }
}

void start_new_segment() {
     if (_recordFile) {
        _recordFile.close();
    }
    
    // Ensure directory exists
    if (!SD_MMC.exists("/recordings")) {
        SD_MMC.mkdir("/recordings");
    }
    
    // Manage storage before creating new file
    manage_storage();
    
    String filename = "/recordings/rec_" + String(millis()) + ".mjpeg";
    _recordFile = SD_MMC.open(filename, FILE_WRITE);
    
    if (_recordFile) {
        Serial.println("[INFO] Started recording segment: " + filename);
        _currentSegmentStart = millis();
        _isRecording = true;
    } else {
        Serial.println("[ERROR] Failed to open recording file");
        _isRecording = false;
    }
}

void sd_recorder_stop_segment() {
    if (_recordFile) {
        _recordFile.close();
        Serial.println("[INFO] Stopped recording segment.");
    }
    _isRecording = false;
}

void sd_recorder_start_manual() {
    if (_manualRecording) return;
    Serial.println("[INFO] Manual Recording START");
    _manualRecording = true;
}

void sd_recorder_stop_manual() {
    if (!_manualRecording) return;
    Serial.println("[INFO] Manual Recording STOP");
    _manualRecording = false;
    // Immediate stop
    sd_recorder_stop_segment();
}

bool sd_recorder_is_recording() {
    return _manualRecording || (ENABLE_DAILY_RECORDING && _sdMountSuccess);
}

bool sd_recorder_is_mounted() {
    return _sdMountSuccess;
}

void sd_recorder_loop() {
    // CRITICAL FIX: Do not attempt to record if SD mount failed.
    if (!_sdMountSuccess) return;

    // Check if we should be recording
    bool shouldRecord = ENABLE_DAILY_RECORDING || _manualRecording;
    
    if (!shouldRecord) {
        // Ensure we aren't leaving a file open if we just stopped
        if (_isRecording) {
            sd_recorder_stop_segment();
        }
        return;
    }

    if (FLASH_LED_ENABLED && !wifiManager.isInAPMode()) { 
        // Optional optimization: disable recording during heavy WiFi use if causing instability?
        // But user wants "even if connection fails".
    }

    unsigned long now = millis();
    
    // 1. Check segment time (only if we are actively recording)
    if (_isRecording && (now - _currentSegmentStart > (RECORD_SEGMENT_SEC * 1000))) {
        start_new_segment();
    }
    
    // 2. Init if needed
    if (!_isRecording) {
        start_new_segment();
        if (!_isRecording) return; // Still failed
    }

    // 3. Record Frame (2 FPS for background recording — saves CPU vs 5 FPS)
    if (now - _lastRecordFrame > 500) { // 500ms = 2 FPS
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) return;
        
        if (_recordFile.write(fb->buf, fb->len) != fb->len) {
            Serial.println("[ERROR] Write failed. Disk full?");
            _recordFile.close();
            _isRecording = false;
        } else {
            // Periodic flush every 10 frames to prevent data loss on power loss
            _framesSinceFlush++;
            if (_framesSinceFlush >= 10) {
                _recordFile.flush();
                _framesSinceFlush = 0;
            }
        }
        
        esp_camera_fb_return(fb);
        _lastRecordFrame = now;
    }
}
