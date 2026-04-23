# ESP32-CAM ONVIF/RTSP Surveillance System

### Multi-Board, Multi-Codec, Professional Network Camera Firmware

[![Platform](https://img.shields.io/badge/platform-ESP32%20|%20ESP32--S3%20|%20ESP32--P4-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-Beta-brightblue.svg)]()
[![H.264](https://img.shields.io/badge/H.264-ESP32--P4%20|%20S3-orange.svg)]()

![ESP32-CAM-ONVIF](ESP32-CAM-ONVIF.jpg)

Transform your affordable ESP32 camera module into a **professional-grade ONVIF Security Camera**. This firmware delivers real-time RTSP/ONVIF streaming with AI-powered motion detection, **multiple ESP32 camera boards** from various manufacturers and offers **MJPEG** streaming on all boards plus optional **H.264 encoding** on ESP32-P4 (hardware) and ESP32-S3 (software), automated cloud backups to Google Drive and Telegram, continuous recording, WebDAV file access, MQTT home automation integration, Bluetooth presence detection, and a modern web interface -- all running on a microcontroller with aggressive memory optimization and FreeRTOS task scheduling.

---

## 🎯 Key Capabilities

| Category | Feature | Details |
|----------|---------|---------|
| **Streaming** | ONVIF Profile S | Compatible with Hikvision, Dahua, Unifi Protect, Blue Iris, Synology |
| **Streaming** | RTSP Server | Low-latency MJPEG at 20+ FPS, H.264 on S3/P4 |
| **Intelligence** | Motion Detection | Frame-difference luminance analysis, configurable threshold and cooldown |
| **Intelligence** | AI Object Detection | TensorFlow.js COCO-SSD model (browser-side, zero MCU overhead) |
| **Cloud** | Google Drive Backup | Async JPEG, settings.json, and UI preferences (localStorage) upload on motion via Google Apps Script proxy |
| **Cloud** | Telegram Alerts | Instant photo notifications on motion detection |
| **Integration** | MQTT | Home Assistant / Node-RED integration with dynamic task management |
| **Storage** | Continuous Recording | DashCam-style chunked .mjpeg recording to SD card (configurable 1-60 min chunks) |
| **Storage** | WebDAV Server | Mount SD card as a Windows/macOS/Linux network drive for drag-and-drop file access |
| **Wireless** | Bluetooth Presence | Auto-detect if you are home using your phone's BLE MAC address |
| **Wireless** | Stealth Mode | Disable all LEDs when WiFi is lost and owner is not home |
| **System** | OTA Updates | Over-the-air firmware updates from GitHub Releases or manual upload |
| **System** | Dynamic RAM Management | FreeRTOS tasks are created/destroyed at runtime based on WebUI toggles |
| **System** | NTP Time Sync | POSIX timezone support for accurate recording timestamps |
| **UI** | Modern Web Interface | Responsive SPA with dark theme, live dashboard, camera controls, system diagnostics |

> [!NOTE]
> **🚧 Work in Progress:**  
> ESP32CAM-ONVIF is still evolving! Help make it better and faster-contributions, feedback, and ideas are warmly welcome.  
> *Star the repo and join the project!*

---

### 🎥 Professional Streaming
| Feature | ESP32-CAM | ESP32-S3 | ESP32-P4 |
|---------|-----------|----------|----------|
| **MJPEG Streaming** | ✅ 20 FPS | ✅ 25+ FPS | ✅ 30+ FPS |
| **H.264 Encoding** | ❌ | ✅ Software (~17 FPS) | ✅ Hardware (30 FPS @ 1080p) |
| **ONVIF Compatible** | ✅ | ✅ | ✅ |
| **Memory Required** | 4MB Flash | 8MB Flash + PSRAM | 8MB Flash + PSRAM |

### 📺 NVR/DVR Compatibility
- **Hikvision** - HVR/NVR Series (Tested: DS-7200)
- **Dahua** - XVR/NVR Series  
- **Ubiquiti Unifi Protect** - UDM Pro, Cloud Key Gen2+
- **Blue Iris** - PC-based NVR
- **Synology Surveillance Station**
- **Any ONVIF Profile S compliant recorder**

### 📶 Wireless Features (Beta)
- **PRESENCE DETECTION**: Automatically detects if you are Home using your Phone's Bluetooth MAC.
- **STEALTH MODE**: Turns off all lights if WiFi is lost AND you are not home.
- **BLUETOOTH AUDIO**: Use a Bluetooth Mic/Headset as a wireless microphone (HFP).
- **PRIORITY STREAMING**: Smart coexistence ensures WiFi RTSP streaming never stutters, even with Bluetooth on.

> **ONVIF Zero-Heap Optimizations**: Extreme ONVIF server optimization prevents memory fragmentation during aggressive NVR polling. All SOAP XML templates are strictly embedded into Flash (`PROGMEM`), and responses are dynamically built directly into a single static C-style `char` array buffer, eliminating dangerous dynamic `String` allocations entirely.

---

## 📟 Supported Hardware

### ESP32 (MJPEG Streaming)
| Board | Manufacturer | Camera | Notes |
|-------|-------------|--------|-------|
| **ESP32-CAM** | AI-Thinker | OV2640 | Most common |
| **M5Stack Camera** | M5Stack | OV2640 | Compact form factor |
| **M5Stack Wide** | M5Stack | OV2640 | Fisheye lens |
| **M5Stack UnitCam** | M5Stack | OV2640 | Tiny, no PSRAM |
| **TTGO T-Camera** | LilyGO | OV2640 | With OLED display |
| **TTGO T-Journal** | LilyGO | OV2640 | With mic & OLED |
| **ESP-WROVER-KIT** | Espressif | OV2640 | Dev board |
| **ESP-EYE** | Espressif | OV2640 | With mic |

### ESP32-S3 (MJPEG + H.264 Software)
| Board | Manufacturer | Camera | Notes |
|-------|-------------|--------|-------|
| **Freenove ESP32-S3-WROOM** | Freenove | OV2640 | Best S3 option |
| **Seeed XIAO ESP32S3 Sense** | Seeed Studio | OV2640 | Ultra-compact |
| **ESP32-S3-EYE** | Espressif | OV2640 | Official dev board |

### ESP32-P4 (MJPEG + H.264 Hardware Acceleration)
| Board | Manufacturer | Camera | Performance |
|-------|-------------|--------|-------------|
| **ESP32-P4-Function-EV** | Espressif | MIPI-CSI | 30 FPS @ 1080p, 140KB RAM |

---

## 🛠️ Quick Start

### 1. Clone Repository
```bash
git clone https://github.com/John-Varghese-EH/ESP32CAM-ONVIF.git
cd ESP32CAM-ONVIF
```

### 2. Select Your Board
Edit `ESP32CAM-ONVIF/config.h`:
```cpp
// ==== STEP 1: SELECT YOUR BOARD ====
#define BOARD_AI_THINKER_ESP32CAM     // Most common
// #define BOARD_FREENOVE_ESP32S3     // For S3 boards
// #define BOARD_ESP32P4_FUNCTION_EV  // For P4 boards
```

### 3. Select Video Codec
```cpp
// ==== STEP 2: SELECT VIDEO CODEC ====
#define VIDEO_CODEC_MJPEG             // Works on ALL boards (default)
// #define VIDEO_CODEC_H264           // ESP32-P4/S3 only!
```

### 4. Configure WiFi
```cpp
// ==== WiFi Settings ====
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
```

### 5. Build & Upload

**Arduino IDE:**
1. Install ESP32 Board Manager (v2.0.14+)
2. Select your board (AI Thinker ESP32-CAM, etc.)
3. Upload!

**PlatformIO (Recommended):**
```bash
pio run -t upload
pio device monitor -b 115200
```

**ESP-IDF (Required for H.264):**
```bash
idf.py set-target esp32s3  # or esp32p4
idf.py add-dependency "espressif/esp_h264^1.2.0"  # For H.264
idf.py build flash monitor
```

### 6. Access the Web Interface

Open `http://<ESP32-IP>` in your browser. The dashboard provides live streaming, camera controls, system diagnostics, and full integration configuration.

---

## 🎬 H.264 Encoding Setup

> **Note**: H.264 requires ESP-IDF, not Arduino IDE.

### ESP32-P4 (Hardware Encoder)
```cpp
#define BOARD_ESP32P4_FUNCTION_EV
#define VIDEO_CODEC_H264
#define H264_ENCODER_AUTO   // Hardware encoder
#define H264_GOP       30   // Keyframe interval
#define H264_FPS       30   // 30 FPS at 1080p
#define H264_BITRATE   4000000  // 4 Mbps
```

**Performance**: 30 FPS @ 1920x1080, only 140KB RAM!

### ESP32-S3 (Software Encoder)
```cpp
#define BOARD_FREENOVE_ESP32S3
#define VIDEO_CODEC_H264
#define H264_ENCODER_AUTO   // Software encoder
#define H264_SW_MAX_WIDTH   640
#define H264_SW_MAX_HEIGHT  480
```

**Performance**: ~17 FPS @ 320x192, requires ~1MB RAM

```bash
idf.py set-target esp32s3
idf.py add-dependency "espressif/esp_h264^1.2.0"
idf.py build flash monitor
```

---

## 📹 NVR/HVR Integration

### Hikvision HVR/NVR

| Setting | Value |
|---------|-------|
| **Protocol** | ONVIF |
| **IP Address** | ESP32 IP |
| **Management Port** | 8000 |
| **Username** | admin |
| **Password** | esp123 |
| **Transfer Protocol** | TCP (recommended) |

### Troubleshooting "Offline (Parameter Error)"
- ✅ Ensure time is synced (NVR sends SetSystemDateAndTime)
- ✅ Use TCP transport (more reliable than UDP)
- ✅ Try rebooting both camera and NVR
- ✅ For H.264: Ensure codec is correctly configured

### Hikvision HVR Workaround (go2rtc Transcoder)

> Some Hikvision HVRs only support H.264/H.265 via ONVIF. The ESP32-CAM produces MJPEG which these units cannot decode.
> Use **go2rtc** for real-time transcoding and **onvif-server** for protocol wrapping.

```
ESP32-CAM (MJPEG) → go2rtc (transcode to H.264) → onvif-server → Hikvision NVR
      ↓                         ↓                      ↓
   Port 554              H.264 @ Port 8554         Port 8081
```

See the [go2rtc setup guide](https://github.com/AlexxIT/go2rtc) and [onvif-server](https://github.com/daniela-hase/onvif-server) for full configuration instructions.

<details>
<summary><b>go2rtc Quick Setup</b></summary>
  
### Quick Setup (Windows)

#### Step 1: Install Prerequisites

```powershell
# Install FFmpeg
winget install FFmpeg

# Install Node.js (for onvif-server)
winget install OpenJS.NodeJS.LTS
```

#### Step 2: Download go2rtc

Download from [go2rtc releases](https://github.com/AlexxIT/go2rtc/releases) and extract to a folder.

Create `go2rtc.yaml` in the same folder:

```yaml
streams:
  esp32cam:
    # Transcode MJPEG to H.264 (replace [ESP32-IP] with your camera IP)
    - ffmpeg:rtsp://admin:esp123@[ESP32-IP]:554/mjpeg/1#video=h264

rtsp:
  listen: ":8554"

api:
  listen: ":1984"
```

#### Step 3: Clone and Setup onvif-server

```powershell
git clone https://github.com/daniela-hase/onvif-server.git
cd onvif-server
npm install
```

#### Step 4: Find Your MAC Address

```powershell
Get-NetAdapter | Where-Object {$_.Status -eq 'Up'} | Select-Object Name, MacAddress
```

Use the MAC address of your main network adapter (WiFi or Ethernet).

#### Step 5: Create onvif-server config

Create `config.yaml` in the onvif-server folder:

```yaml
onvif:
  - mac: XX-XX-XX-XX-XX-XX           # Your MAC address from Step 4
    ports:
      server: 8081                    # ONVIF port for Hikvision
      rtsp: 8554                      # go2rtc RTSP port
    name: ESP32CAM
    uuid: 15b21259-77d9-441f-9913-3ccd8a82e430
    highQuality:
      rtsp: /esp32cam                 # Stream name from go2rtc
      width: 640
      height: 480
      framerate: 25
      bitrate: 2048
      quality: 4
    lowQuality:
      rtsp: /esp32cam
      width: 640
      height: 480
      framerate: 25
      bitrate: 1024
      quality: 1
    target:
      hostname: 127.0.0.1             # go2rtc runs locally
      ports:
        rtsp: 8554
```

#### Step 6: Run Both Services

**Terminal 1 - Start go2rtc:**
```powershell
cd path\to\go2rtc
.\go2rtc.exe
```

**Terminal 2 - Start onvif-server:**
```powershell
cd path\to\onvif-server
node main.js ./config.yaml
```

#### Step 7: Add to Hikvision NVR

| Setting | Value |
|---------|-------|
| **Protocol** | `ONVIF` |
| **IP Address** | Your PC's IP |
| **Port** | `8081` |
| **Username** | `admin` |
| **Password** | `admin` |

---

### Linux/Raspberry Pi Setup

```bash
# Install dependencies
sudo apt update
sudo apt install -y ffmpeg nodejs npm

# Download go2rtc
wget https://github.com/AlexxIT/go2rtc/releases/latest/download/go2rtc_linux_amd64
chmod +x go2rtc_linux_amd64
sudo mv go2rtc_linux_amd64 /usr/local/bin/go2rtc

# Clone onvif-server
cd /opt
sudo git clone https://github.com/daniela-hase/onvif-server.git
cd onvif-server
sudo npm install

# Get your MAC address
ip link show | grep ether
```

**Create systemd services:**

`/etc/systemd/system/go2rtc.service`:
```ini
[Unit]
Description=go2rtc streaming server
After=network.target

[Service]
ExecStart=/usr/local/bin/go2rtc -config /etc/go2rtc/go2rtc.yaml
Restart=always
User=pi

[Install]
WantedBy=multi-user.target
```

`/etc/systemd/system/onvif-server.service`:
```ini
[Unit]
Description=Virtual ONVIF Server
After=network.target go2rtc.service

[Service]
ExecStart=/usr/local/bin/go2rtc -config /etc/go2rtc/go2rtc.yaml
Restart=always
User=pi

[Install]
WantedBy=multi-user.target
```

`/etc/systemd/system/onvif-server.service`:
```ini
[Unit]
Description=Virtual ONVIF Server
After=network.target go2rtc.service

[Service]
WorkingDirectory=/opt/onvif-server
ExecStart=/usr/bin/node main.js ./config.yaml
Restart=always
User=pi

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable go2rtc onvif-server
sudo systemctl start go2rtc onvif-server
```

### Troubleshooting

| Issue | Solution |
|-------|----------|
| **"Failed to find IP address for MAC"** | Check MAC with `Get-NetAdapter` (Windows) or `ip link show` (Linux) |
| **Hikvision shows "Offline"** | Ensure both go2rtc and onvif-server are running |
| **No video stream** | Test RTSP in VLC: `rtsp://localhost:8554/esp32cam` |
| **FFmpeg not found** | Restart terminal after installing FFmpeg |

</details>
  
---

## ☁️ Cloud Backup: Google Drive

Motion-triggered JPEG snapshots are uploaded asynchronously to Google Drive using a Google Apps Script proxy. This approach avoids heavy OAuth flows on the microcontroller and uses PSRAM-buffered transfers to prevent blocking the RTSP stream.

### Extended Backup Capabilities
You can optionally bundle your ESP32 configuration (`settings.json`) and your browser UI preferences (`localStorage.json`) directly into the GDrive multipart payload, ensuring your entire ecosystem state is safely archived alongside security footage.

### Setup

1. Open [Google Apps Script](https://script.google.com) and create a new project
2. Paste the following script:

```javascript
function doPost(e) {
  var folder = DriveApp.getFolderById("YOUR_FOLDER_ID");
  var blob = e.postData.contents;
  var contentType = e.postData.type;

  if (contentType.indexOf("multipart") >= 0) {
    var parts = Utilities.parseCsv(blob, "\r\n");
    // Handle multipart form data
    var data = Utilities.newBlob(
      Utilities.base64Decode(blob.split("\r\n\r\n")[1].split("\r\n--")[0]),
      "image/jpeg",
      "motion_" + new Date().toISOString() + ".jpg"
    );
    folder.createFile(data);
  }

  return ContentService.createTextOutput("OK");
}
```

3. Deploy as Web App (Execute as: Me, Access: Anyone)
4. Copy the Web App URL and paste it into the ESP32 WebUI under **System > Integrations > Google Drive**

---

## 🌐 WebUI Feature Overview

The web interface is a responsive single-page application with a dark cyberpunk theme.

**Dashboard Tab**
- Live MJPEG stream with CCTV-style date/time overlay (high-contrast blend modes)
- One-click snapshot, flash control, manual recording toggle
- Live Resource Monitor with dual-plotted real-time Canvas graphs for Heap and PSRAM utilization
- Mobile-optimized swipable bottom navigation bar for zero-congestion ergonomics

**Camera Tab**
- Resolution, quality, brightness, contrast, saturation controls
- Scene presets (Night, Daytime, Indoor, Outdoor)
- Horizontal mirror, vertical flip, auto white balance

**Network Tab**
- WiFi scanner with signal strength visualization
- RTSP/ONVIF connection info and stream URLs

**System Tab**
- Integrations panel: MQTT, Telegram, Google Drive (each with enable/disable toggles)
- Advanced Services: WebDAV, NTP/Timezone, Continuous Recording (DashCam)
- OTA firmware update (manual upload or GitHub auto-update)
- Settings export/import (JSON backup)
- Factory reset and reboot controls

---

## 🔌 Port Configuration

| Port | Service | Description |
|------|---------|-------------|
| 80 | HTTP | Web interface and REST API |
| 554 | RTSP | Real-time video streaming |
| 8000 | ONVIF | Device management and discovery |
| 3702 | WS-Discovery | UDP Multicast auto-detection |

---

## 🏗️ Architecture

```
                                    +------------------+
                                    |   NVR / HVR      |
                                    |  (Hikvision,     |
        +-------+     RTSP/554      |   Dahua, etc.)   |
        | ESP32 | ===============> +------------------+
        |  CAM  |
        |       | ---> ONVIF/8000 ---> WS-Discovery/3702 (Auto-detect)
        |       |
        |       | ---> HTTP/80 -----> WebUI (Dashboard, Camera, Settings)
        |       |                         |
        |       |                         +---> /stream (MJPEG live feed)
        |       |                         +---> /api/settings (REST API)
        |       |                         +---> /webdav/* (Network Drive)
        |       |
        |       | ---> Motion Event ---> Telegram (Async Photo)
        |       |                   ---> Google Drive (Async JPEG Upload)
        |       |                   ---> MQTT (State Publish)
        |       | ---> more:        ---> SD Card (Continuous .mjpeg)
        +-------+
```

**FreeRTOS Task Layout** (6 pinned tasks for deterministic scheduling):

| Task | Core | Priority | Stack | Purpose |
|------|------|----------|-------|---------|
| `RTSP_Task` | 0 | 4 (High) | 6KB | Real-time video streaming (never blocked) |
| `ONVIF_HTTP_Task` | 1 | 3 | 6KB | Web UI + ONVIF SOAP processing |
| `WiFi_Mgmt_Task` | 1 | 6 | 4KB | Connectivity monitoring and reconnection |
| `WDT_Task` | 1 | 7 (Critical) | 2KB | Watchdog, heap audit, dynamic task spawner |
| `Low_Prio_Task` | 1 | 2 | 4KB | Motion detection, SD recording, LED, Bluetooth |
| `MQTT_Task` | 1 | 2 | 4KB | Dynamically spawned/killed based on config |

---

## 📁 Project Structure

```
ESP32CAM-ONVIF/
|-- config.h                  # User configuration (board, codec, WiFi, features)
|-- board_config.h            # Board-specific GPIO pin definitions
|-- ESP32CAM-ONVIF.ino        # Main entry: FreeRTOS task creation and lifecycle
|-- rtsp_server.cpp/h         # RTSP streaming server
|-- onvif_server.cpp/h        # ONVIF Profile S protocol handler
|-- CRtspSession.cpp/h        # RTSP session management (optimized static buffers)
|-- CStreamer.cpp/h            # RTP packetization
|-- web_config.cpp/h           # REST API and WebServer routes
|-- config.cpp                 # Settings persistence (SPIFFS JSON)
|-- motion_detection.cpp/h     # Luminance-based motion detection
|-- sd_recorder.cpp/h          # SD card recording (manual + DashCam continuous)
|-- mqtt_manager.cpp/h         # MQTT client (dynamic FreeRTOS task)
|-- telegram_manager.cpp/h     # Telegram Bot API integration
|-- gdrive_manager.cpp/h       # Google Drive async upload (PSRAM-buffered)
|-- webdav_server.cpp/h        # WebDAV PROPFIND/GET handler
|-- wifi_manager.cpp/h         # WiFi connection manager with AP fallback
|-- camera_control.cpp/h       # Camera sensor parameter control
|-- auto_flash.cpp/h           # Automatic flash LED management
|-- status_led.cpp/h           # Status LED patterns
|-- data/
|   |-- index.html             # WebUI structure
|   |-- app.js                 # WebUI logic (3000+ lines)
|   |-- style.css              # WebUI styling (dark theme)
```

---

## ⚠️ Troubleshooting

| Issue | Solution |
|-------|----------|
| **Purple/Green lines** | Power supply issue - use 5V 2A adapter |
| **Stream disconnects** | Weak WiFi - move closer or use external antenna |
| **"Drop the Loop"** | Memory issue - reduce resolution or disable features |
| **ONVIF not discovered** | Enable multicast on router, allow UDP 3702 |
| **H.264 compile error** | You need ESP-IDF + esp_h264 component |
| **H.264 not supported** | Only ESP32-P4 (HW) and ESP32-S3 (SW) support H.264 |
| **Hikvision: "Stream type not supported"** | ESP32-CAM outputs MJPEG but Hikvision needs H.264. Use [go2rtc transcoder](#-hikvision-hvr-workaround-go2rtc-transcoder) |
| **Hikvision: "Parameter error"** | Check ONVIF port (8000), user/pass (admin/esp123), use TCP transport |
| **Unifi Protect: Auth failed** | Fixed in v1.1+ - update firmware |
| **Stack smashing / random crashes** | Fixed in v1.1+ - update firmware |
| **Bootloop on startup** | LED/PTZ pins may conflict with board. Set `FLASH_LED_ENABLED` and `STATUS_LED_ENABLED` to `false` in config.h |

---

## 🗺️ Roadmap

### ✅ Completed
- [x] Multi-board support (12+ boards)
- [x] ONVIF Profile S compliance
- [x] Hikvision, Dahua, Unifi Protect compatibility
- [x] H.264 infrastructure for P4/S3
- [x] Modern Web Interface with live dashboard
- [x] MQTT, Telegram, and Google Drive integrations
- [x] Continuous DashCam recording to SD
- [x] WebDAV network drive access
- [x] Dynamic FreeRTOS task management (zero-overhead when disabled)
- [x] NTP time sync with POSIX timezone
- [x] Bluetooth presence detection and stealth mode
- [x] OTA updates (manual + GitHub auto-update)

### 🔄 In Progress
- [ ] H.264 RTP NAL unit packetization
- [ ] ONVIF H.264 profile reporting

### 📋 Planned
- [ ] H.265/HEVC support (ESP32-P4)
- [ ] Audio support (G.711/AAC)
- [ ] ONVIF Profile T (Advanced streaming)
- [ ] Multi-stream support (Main + Sub profiles)
- [ ] HomeKit Secure Video integration

---

## 🤝 Contributing

Contributions are welcome. Please fork the repository, create a feature branch, and submit a pull request.

**Areas where help is appreciated:**
- Testing on different NVR brands and models
- New board definitions and pin configurations
- Performance profiling and memory optimization
- Documentation and translations

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Submit a pull request

> [!IMPORTANT]
> **Attention Forkers:** If you are using a fork of this repository, please **sync with the upstream `main` branch** before reporting bugs. This project is under active development, and many early-stage issues have already been patched.
> 
> **Note on Issues:** Please only raise issues or bug reports in the **[Main Upstream Repository](https://github.com/John-Varghese-EH/ESP32CAM-ONVIF/issues)**. I do not receive notifications for issues opened on forks, and they will likely go unnoticed.

---

## ⚠️ Disclaimer

> **This project is currently a proof of concept for testing.**
> 
> Neither the ESP32CAM, nor its SDK was meant or built for proper ONVIF/RTSP support. Bugs can occur!

---

## 📜 License

Apache License 2.0 - See [LICENSE](LICENSE) for details.

---

## Credits

**Developed by John Varghese (J0X)**

Built on:
- [Micro-RTSP](https://github.com/geeksville/Micro-RTSP) - RTSP server foundation
- [ESP32-Camera](https://github.com/espressif/esp32-camera) - Camera driver
- [esp_h264](https://github.com/espressif/esp-h264-component) - H.264 encoder component

---

## 💗 Support the Project

This is an active, evolving project. If it helped you, consider supporting continued development ☺️:

[![Buy me a Coffee](https://img.shields.io/badge/Buy_Me_A_Coffee-FFDD00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/CyberTrinity)
[![Patreon](https://img.shields.io/badge/Patreon-F96854?style=for-the-badge&logo=patreon&logoColor=white)](https://patreon.com/CyberTrinity)
[![Sponsor](https://img.shields.io/badge/sponsor-30363D?style=for-the-badge&logo=GitHub-Sponsors&logoColor=#white)](https://github.com/sponsors/John-Varghese-EH)

---

<div align="center">

**⭐ Star this repo if it helped you.**

[Report Bug](https://github.com/John-Varghese-EH/ESP32CAM-ONVIF/issues) | [Request Feature](https://github.com/John-Varghese-EH/ESP32CAM-ONVIF/issues)

</div>
