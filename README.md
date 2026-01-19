# ESP32-CAM ONVIF/RTSP Surveillance Camera

**Multi-Board, Multi-Codec, Professional Network Camera Firmware**

[![Platform](https://img.shields.io/badge/platform-ESP32%20|%20ESP32--S3%20|%20ESP32--P4-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-Beta-blue.svg)]()
[![H.264](https://img.shields.io/badge/H.264-ESP32--P4%20|%20S3-orange.svg)]()

![ESP32-CAM-ONVIF](ESP32-CAM-ONVIF.jpg)

Transform your affordable ESP32 camera module into a **professional-grade ONVIF Security Camera**. This firmware supports **multiple ESP32 camera boards** from various manufacturers and offers **MJPEG** streaming on all boards plus optional **H.264 encoding** on ESP32-P4 (hardware) and ESP32-S3 (software).

---

## ✨ What's New

- 🎯 **Multi-Board Support**: 12+ camera boards from AI-Thinker, M5Stack, TTGO, Freenove, Seeed, and Espressif
- 🎬 **H.264 Encoding**: Hardware acceleration on ESP32-P4 (30fps @ 1080p) and software encoding on ESP32-S3
- 🔧 **Improved Hikvision HVR Compatibility**: Fixed RTSP/SDP parameters and session handling
- 📦 **Board-Specific Pin Configurations**: Automatic pin mapping based on board selection
- ✅ **Unifi Protect Support**: Fixed ONVIF authentication for Ubiquiti NVRs
- 🐛 **Stability Fixes**: Resolved memory leaks, buffer overflows, and stack corruption issues

> [!NOTE]
> **🚧 Work in Progress:**  
> ESP32CAM-ONVIF is still evolving! Help make it better and faster-contributions, feedback, and ideas are warmly welcome.  
> *Star the repo and join the project!*

---

## 🚀 Key Features

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

### 💻 Modern Web Interface
- 🌙 **Cyberpunk/Slate Dark Theme** - Beautiful responsive SPA
- 📊 **Live Dashboard** - Stream, Flash control, Snapshots
- ⚙️ **Camera Settings** - Resolution, Quality, Brightness, Contrast
- 📡 **WiFi Manager** - Scan and connect to networks
- 🔄 **OTA Updates** - Wireless firmware updates

---

## 📋 Supported Boards

### ESP32 (MJPEG Only)
| Board | Manufacturer | Camera | Notes |
|-------|-------------|--------|-------|
| **ESP32-CAM** | AI-Thinker | OV2640 | Most common, ~$5 |
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
| **Freenove ESP32-S3-WROOM** | Freenove | OV2640 | Great S3 option |
| **Seeed XIAO ESP32S3 Sense** | Seeed Studio | OV2640 | Very compact |
| **ESP32-S3-EYE** | Espressif | OV2640 | Official dev board |

### ESP32-P4 (MJPEG + H.264 Hardware) 🚀
| Board | Manufacturer | Camera | Notes |
|-------|-------------|--------|-------|
| **ESP32-P4-Function-EV** | Espressif | MIPI-CSI | Hardware H.264! |

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

---

## 🎬 H.264 Encoding Setup

> **Note**: H.264 requires ESP-IDF, not Arduino IDE.

### For ESP32-P4 (Hardware Encoder)
```cpp
// config.h
#define BOARD_ESP32P4_FUNCTION_EV
#define VIDEO_CODEC_H264
#define H264_ENCODER_AUTO  // Will use hardware

// H.264 Settings
#define H264_GOP       30       // Keyframe every 30 frames
#define H264_FPS       30       // 30 FPS at 1080p!
#define H264_BITRATE   4000000  // 4 Mbps
```

**Performance**: 30 FPS @ 1920x1080, only 140KB RAM!

### For ESP32-S3 (Software Encoder)
```cpp
// config.h  
#define BOARD_FREENOVE_ESP32S3
#define VIDEO_CODEC_H264
#define H264_ENCODER_AUTO  // Will use software

// Limit resolution for performance
#define H264_SW_MAX_WIDTH   640
#define H264_SW_MAX_HEIGHT  480
```

**Performance**: ~17 FPS @ 320x192, requires ~1MB RAM

### Add esp_h264 Component
```bash
idf.py add-dependency "espressif/esp_h264^1.2.0"
```

---

## 📹 NVR Configuration

### Hikvision HVR/NVR
1. **Camera Management** → **IP Camera** → **Add**
2. **Protocol**: `ONVIF`
3. **IP Address**: `[ESP32-IP]`
4. **Management Port**: `8000`
5. **User**: `admin` #default
6. **Password**: `esp123` #default
7. **Transfer Protocol**: `TCP` (Recommended)

### Troubleshooting "Offline (Parameter Error)"
- ✅ Ensure time is synced (NVR sends SetSystemDateAndTime)
- ✅ Use TCP transport (more reliable than UDP)
- ✅ Try rebooting both camera and NVR
- ✅ For H.264: Ensure codec is correctly configured

---

## 🔄 Some Hikvision HVR: Virtual ONVIF Server

> **Problem**: Some Hikvision HVRs only support H.264/H.265 via ONVIF protocol. The ESP32-CAM (original ESP32) produces MJPEG which Hikvision cannot decode.  
> **Solution**: Use **go2rtc** for transcoding + **onvif-server** for proper ONVIF protocol.

### Architecture

```
ESP32-CAM (MJPEG) → go2rtc (transcode) → onvif-server → Hikvision NVR
     ↓                    ↓                   ↓
  Port 554        H.264 @ Port 8554      Port 8081
```

### What You Need
- **[go2rtc](https://github.com/AlexxIT/go2rtc)** - Transcodes MJPEG to H.264 and serves RTSP
- **[onvif-server](https://github.com/daniela-hase/onvif-server)** - Wraps RTSP stream as proper ONVIF device
- **FFmpeg** - Required by go2rtc for transcoding

---

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




## 🔌 Port Configuration

| Port | Service | Description |
|------|---------|-------------|
| `80` | HTTP | Web interface |
| `554` | RTSP | Video streaming |
| `8000` | ONVIF | Device management |
| `3702` | WS-Discovery | UDP Multicast |



## 📁 Project Structure

```
ESP32CAM-ONVIF/
├── config.h              # User configuration
├── board_config.h        # Board-specific pin definitions
├── ESP32CAM-ONVIF.ino    # Main entry point
├── rtsp_server.cpp/h     # RTSP streaming
├── onvif_server.cpp/h    # ONVIF protocol
├── h264_encoder.cpp/h    # H.264 encoding (ESP32-P4/S3)
├── CStreamer.cpp/h       # RTP packetization
├── CRtspSession.cpp/h    # RTSP session handling
├── MyStreamer.cpp/h      # MJPEG streamer
├── web_config.cpp/h      # Web interface
└── index_html.h          # Embedded HTML/CSS/JS
```

---

## 🗺️ Roadmap

### ✅ Completed
- [x] Multi-board support (12+ boards)
- [x] ONVIF Profile S compliance
- [x] Hikvision/Dahua compatibility
- [x] H.264 infrastructure for P4/S3
- [x] Web-based configuration

### 🔄 In Progress
- [ ] H.264 RTP streamer (NAL unit packetization)
- [ ] ONVIF H.264 profile reporting

### 📋 Planned
- [ ] H.265/HEVC support (ESP32-P4)
- [ ] Audio support (G.711/AAC)
- [ ] Motion detection with ONVIF events
- [ ] SD Card recording with playback API
- [ ] Multi-stream support (Main + Sub)
- [ ] ONVIF Profile T (Advanced streaming)

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

## 🤝 Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Submit a pull request

**Areas needing help:**
- Testing on different NVR brands
- New board definitions
- Documentation improvements

---

## ⚠️ Disclaimer

> **This project is currently a proof of concept for testing.**
> 
> Neither the ESP32CAM, nor its SDK was meant or built for proper ONVIF/RTSP support. Bugs can occur!

---

## 📜 License

Apache License 2.0 - See [LICENSE](LICENSE) for details.

---

## 👨‍💻 Credits

**Developed by John Varghese (J0X)**

Built on:
- [Micro-RTSP](https://github.com/geeksville/Micro-RTSP) - RTSP server
- [ESP32-Camera](https://github.com/espressif/esp32-camera) - Camera driver
- [esp_h264](https://github.com/espressif/esp-h264-component) - H.264 encoder

---

## 🚧 Currently a work in progress, but I’d appreciate your support! ☺️

[![Buy me a Coffee](https://img.shields.io/badge/Buy_Me_A_Coffee-FFDD00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/CyberTrinity)
[![Patreon](https://img.shields.io/badge/Patreon-F96854?style=for-the-badge&logo=patreon&logoColor=white)](https://patreon.com/CyberTrinity)
[![Sponsor](https://img.shields.io/badge/sponsor-30363D?style=for-the-badge&logo=GitHub-Sponsors&logoColor=#white)](https://github.com/sponsors/John-Varghese-EH)

---

<div align="center">

**⭐ Star this repo if it helped you! ⭐**

[Report Bug](https://github.com/John-Varghese-EH/ESP32CAM-ONVIF/issues) · [Request Feature](https://github.com/John-Varghese-EH/ESP32CAM-ONVIF/issues)

</div>
