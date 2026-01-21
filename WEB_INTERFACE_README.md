# ESP32-CAM ONVIF Web Interface

## 🌟 Overview

A **feature-rich, modern web interface** for controlling and monitoring your ESP32-CAM with ONVIF support. Built with cutting-edge web technologies, this interface provides professional-grade camera control, real-time monitoring, AI-powered object detection, and extensive customization options—all accessible from any browser.

---

## 🚀 Quick Start

1. **Upload firmware** to ESP32-CAM
2. **Connect to WiFi** (credentials in `config.h`)
3. **Access** `http://[ESP32-IP]` in browser
4. **Login** with credentials (default: `admin` / `esp123`)

---

## ✨ Features Overview

### 📹 **Core Video Streaming**
- Real-time MJPEG stream
- Multiple resolution support (QQVGA to UXGA)
- Adjustable quality (4-63)
- Fullscreen mode
- Picture-in-Picture support
- FPS monitoring overlay

### 🎛️ **Camera Controls**
- **Image Settings**: Brightness, contrast, saturation, exposure
- **Advanced Controls**: AWB, AGC, AEC, gain ceiling
- **Quality Presets**: Low, Medium, High, Ultra (one-click)
- **Camera Profiles**: Save/load named configurations
- **Orientation**: Vertical flip, horizontal mirror

### 🎨 **Real-Time Filters** (Client-Side)
- Brightness (0-200%)
- Contrast (0-200%)
- Saturation (0-200%)
- Hue Rotate (0-360°)
- Blur (0-10px)
- Grayscale (0-100%)
- Sepia (0-100%)
- Invert (0-100%)

*All filters applied in browser, saved to localStorage*

### 🤖 **AI Object Detection** (NEW!)
- **TensorFlow.js COCO-SSD** model
- Detects **80+ object classes**:
  - People, vehicles (car, truck, bus, motorcycle, bicycle)
  - Animals (dog, cat, bird, horse, etc.)
  - Household items (phone, laptop, bottle, cup, etc.)
- Real-time bounding boxes
- Confidence scores
- Zero backend load (runs in browser)
- Toggle with **D** key

### 🎥 **Recording**
- **Client-side** recording (WebM/MP4)
- **SD card** recording (server-side)
- Snapshot capture (S key)
- **Snapshot Gallery** (localStorage, 50 max)
- Time-lapse creator (100 frames max)
- Video playback from SD

### ⌨️ **Keyboard Shortcuts**
| Key | Action |
|-----|--------|
| `Space` | Play/Pause stream |
| `S` | Save snapshot |
| `R` | Toggle recording |
| `F` | Toggle flash |
| `K` | Fullscreen |
| `G` | Open gallery |
| `Q` | Quick actions |
| `P` | Picture-in-Picture |
| `M` | Kiosk mode |
| `D` | Toggle AI detection |
| `?` | Show shortcuts help |
| `1-6` | Switch tabs |
| `Esc` | Close modals |

### 📊 **Monitoring & Analytics**
- **Stream Performance**:
  - Live FPS counter
  - Dropped frames tracking
  - Latency display
- **System Status**:
  - Uptime
  - Free heap memory
  - Flash & PSRAM usage
  - WiFi signal strength (RSSI)
- **Connection History**:
  - 24-hour RSSI graph
  - 24-hour FPS graph
  - Automatic data rotation
- **Event Log**:
  - System boot
  - Motion detection
  - Recording start/stop
  - WiFi events
  - Authentication attempts
  - Last 100 events (circular buffer)

### 🔧 **Advanced Features**
- **Quick Actions Panel**: Floating sidebar with common shortcuts
- **Kiosk Mode**: Fullscreen with auto-hide UI (3s timeout)
- **Comparison Tool**: Before/after slider for testing settings
- **🔔 Sound Notifications**: Audio feedback for events
- **Network Diagnostics**: Ping test, latency measurement
- **Settings Export/Import**: Backup and restore configuration

### 📂 **Storage & Playback**
- SD card statistics (total/used/free)
- File count display
- Visual storage meter
- Browse recordings
- Stream recordings directly
- Delete old recordings

### 📡 **Network & Connectivity**
- **WiFi Manager**:
  - Network scanning
  - Signal strength display
  - Connection management
- **ONVIF Support**:
  - Enable/disable toggle
  - RTSP URL display
  - Device service endpoint
- **Bluetooth** (if enabled):
  - Presence detection
  - Audio source selection
  - Mic gain control
  - Device scanning
- **Network Diagnostics**:
  - Ping test
  - Latency measurement
  - RSSI tracking

### 🎯 **PTZ Control** (if hardware supports)
- Pan/tilt servo control
- Home position
- Preset positions

### 🔐 **Security**
- HTTP Basic Authentication
- Configurable credentials
- Session management
- Login timeout

---

## 🗂️ **Tab Structure**

### 1. **Dashboard** 📊
- Live video stream
- Performance overlay (FPS, dropped frames)
- AI detection overlay (when enabled)
- System status cards
- Recording controls

### 2. **Camera** 🎥
- Quality presets
- Camera profiles (save/load/delete)
- Image settings sliders
- Advanced controls
- Filter panel (brightness, contrast, etc.)
- Auto flash toggle
- ONVIF settings

### 3. **Network** 📡
- WiFi manager & scanner
- Network diagnostics (ping test)
- Audio source selection
- Bluetooth settings
- Presence detection

### 4. **Events** 📝
- Event log viewer
- Filter by type
- Clear log button
- Icon-coded events
- Timestamp display

### 5. **Recordings** 🎬
- SD card recordings list
- Video player modal
- Download recordings
- Delete recordings
- File size & date info

### 6. **System** ⚙️
- Firmware update (OTA)
- SD card information
- Settings export/import
- Sync time
- Reboot device

---

## 🎨 **UI & Design**

### **Modern Glassmorphism Theme**
- Dark background with gradient overlays
- Frosted glass panels (backdrop-filter)
- Smooth animations
- Responsive grid layouts
- Mobile-optimized

### **Colors**
- **Primary**: `#6366f1` (indigo)
- **Accent**: `#d946ef` (fuchsia)
- **Success**: `#10b981` (emerald)
- **Danger**: `#ef4444` (red)
- **Background**: `#050510` (dark purple)

### **Typography**
- Font: **Inter** (sans-serif)
- Weights: 400, 500, 600, 700
- Monospace for values/stats

### **Icons**
- Emoji-based for universal support
- SVG for brand logo
- Color-coded status indicators

---

## 💾 **Browser Storage**

### localStorage Usage

| Key | Purpose | Size |
|-----|---------|------|
| `esp32cam_snapshots` | Snapshot gallery (50 max) | ~5MB |
| `esp32cam_history` | Connection timeline (24h) | ~850KB |
| `esp32cam_filters` | Video filter settings | <1KB |
| `esp32cam_preferences` | Sound settings | <1KB |
| **Total** | | **<7MB** |

### Automatic Cleanup
- Snapshots: Auto-delete oldest when >50
- History: Remove data >24 hours old
- Safe within all browser limits

---

## 🌐 **Browser Compatibility**

| Feature | Chrome | Firefox | Safari | Edge |
|---------|:------:|:-------:|:------:|:----:|
| Video Stream | ✅ | ✅ | ✅ | ✅ |
| localStorage | ✅ | ✅ | ✅ | ✅ |
| Canvas | ✅ | ✅ | ✅ | ✅ |
| Keyboard Shortcuts | ✅ | ✅ | ✅ | ✅ |
| CSS Filters | ✅ | ✅ | ✅ | ✅ |
| Fullscreen API | ✅ | ✅ | ✅ | ✅ |
| Web Audio | ✅ | ✅ | ✅ | ✅ |
| MediaRecorder | ✅ | ✅ | ✅ | ✅ |
| PiP | ✅ | ✅ | ⚠️ | ✅ |
| TensorFlow.js | ✅ | ✅ | ✅ | ✅ |

✅ Full support | ⚠️ Limited support

---

## 🔌 **API Endpoints**

### **Stream**
- `GET /stream` - MJPEG video stream
- `GET /snapshot` - Capture single frame

### **Camera Control**
- `POST /api/config` - Update camera settings
- `POST /api/camera/preset` - Apply quality preset
- `POST /api/flash` - Toggle flash LED

### **Profiles**
- `GET /api/profiles` - List saved profiles
- `POST /api/profiles/save` - Save current settings
- `POST /api/profiles/load` - Load profile
- `DELETE /api/profiles/delete` - Delete profile

### **Status & Monitoring**
- `GET /api/status` - System status (RSSI, heap, etc.)
- `GET /api/system/info` - Detailed system info
- `GET /api/events` - Get event log
- `DELETE /api/events` - Clear event log

### **Recording**
- `POST /api/record` - Start/stop SD recording
- `GET /api/recordings` - List SD recordings
- `GET /api/recordings/stream?file=X` - Stream recording
- `DELETE /api/recordings/delete` - Delete recording

### **Storage**
- `GET /api/sd/info` - SD card statistics
- `GET /api/sd/list` - Browse SD files

### **PTZ** (if enabled)
- `POST /api/ptz/control` - Control servos

### **Network**
- `GET /api/wifi/status` - WiFi info
- `GET /api/wifi/scan` - Scan networks
- `GET /api/network/ping` - Ping test
- `POST /api/network/bandwidth-test` - Bandwidth test

### **Settings**
- `GET /api/settings/export` - Export configuration
- `POST /api/settings/import` - Import configuration

### **ONVIF**
- `POST /api/onvif/toggle` - Enable/disable ONVIF
- `POST /api/autoflash` - Toggle auto flash

### **Bluetooth** (if enabled)
- `GET /api/bt/status` - BT status
- `POST /api/bt/config` - Update BT settings
- `GET /api/bt/scan` - Scan BT devices

### **System**
- `POST /api/time` - Sync time
- `POST /reboot` - Reboot device
- `POST /api/update` - OTA firmware update

---

## 🎯 **Performance**

### **Client-Side Processing**
- **AI Detection**: ~30-60 FPS (browser-dependent)
- **Filter Updates**: <5ms (real-time)
- **Snapshot Capture**: <100ms
- **Graph Rendering**: <50ms (every 5s)

### **Memory Usage**
- **Browser localStorage**: <7MB
- **Browser RAM**: ~50-100MB (with AI model)
- **ESP32 Impact**: Zero (all processing in browser)

### **Network**
- **Stream bandwidth**: Depends on resolution/quality
- **API calls**: Minimal (status every 2s)
- **Object detection**: Zero network load

---

## 🎓 **Advanced Usage**

### **Creating Camera Profiles**
1. Adjust camera settings to desired state
2. Go to **Camera** tab → **Camera Profiles**
3. Click **Save** button
4. Enter profile name (e.g., "Night Mode")
5. Load anytime with dropdown + click profile name

### **Using Snapshot Gallery**
1. Press `S` key anytime to capture
2. Press `G` to open gallery
3. Click snapshot to view full-size
4. Download or delete individual snapshots
5. Max 50 snapshots (auto-rotate)

### **Time-Lapse Creation**
1. Set interval (default 1s)
2. Start capture
3. Let run (max 100 frames)
4. Export compiles to WebM video
5. Download to device

### **AI Object Detection**
1. Press `D` to load model (~5MB download, one-time)
2. Wait for "AI model loaded" toast
3. Press `D` again to enable detection
4. Green bounding boxes appear on detected objects
5. Labels show object class and confidence %
6. Detects: person, car, dog, phone, laptop, and 75+ more classes

### **Comparison Tool**
1. Capture "Before" snapshot
2. Change camera settings (e.g., brightness)
3. Capture "After" snapshot
4. View comparison with vertical slider
5. Useful for testing day/night modes

### **Kiosk Mode**
1. Press `M` for fullscreen
2. UI auto-hides after 3 seconds
3. Move mouse to show UI
4. Press `M` or `Esc` to exit
5. Perfect for monitoring displays

---

## 🔧 **Customization**

### **Modifying Colors**
Edit `style.css` variables:
```css
:root {
  --primary: #6366f1;  /* Main accent color */
  --accent: #d946ef;   /* Secondary accent */
  --success: #10b981;  /* Success/online */
  --danger: #ef4444;   /* Error/offline */
}
```

### **Adjusting Storage Limits**
Edit `app.js`:
```javascript
// Snapshot gallery max
snapshotGallery.maxSnapshots = 50; // Change to 100

// Connection history max points
connectionHistory.maxPoints = 43200; // 24h default
```

### **Adding Custom Shortcuts**
Edit `keyboardShortcuts.init()` in `app.js`:
```javascript
case 'c':
    e.preventDefault();
    // Your custom action
    break;
```

---

## 🐛 **Troubleshooting**

### **Stream Not Loading**
- Check WiFi connection
- Verify ESP32 IP address
- Try lower resolution/quality
- Check browser console for errors

### **AI Detection Not Working**
- Ensure TensorFlow.js scripts loaded (check console)
- Model download requires ~5MB (check network tab)
- May not work on very slow connections
- Some browsers block ML models on HTTP (use HTTPS or localhost)

### **Keyboard Shortcuts Not Working**
- Click outside input fields
- Check if modal is open (Esc to close all)
- Keyboard shortcuts disabled in inputs/textareas

### **Gallery Not Saving**
- Check browser localStorage is enabled
- Clear browser data if quota exceeded
- Some browsers limit localStorage in private mode

### **Poor FPS**
- Lower resolution
- Increase quality value (lower quality = higher FPS)
- Disable AI detection if running
- Check WiFi signal strength

---

## 📱 **Mobile Support**

### **Responsive Design**
- Optimized for phones & tablets
- Touch-friendly buttons
- Collapsible panels
- Swipe-friendly modals

### **Mobile Shortcuts**
- Tap controls instead of keyboard
- Quick Actions FAB for common tasks
- Fullscreen mode for better viewing

### **Performance Tips**
- Use lower resolution on mobile
- Disable AI detection on slower devices
- Close unused tabs for better performance

---

## 🔒 **Security Recommendations**

1. **Change default credentials** in `config.h`
2. **Use strong passwords** (12+ characters)
3. **Enable HTTPS** if possible (ESP32 limitations)
4. **Restrict network access** (firewall rules)
5. **Keep firmware updated**
6. **Disable unused features** (Bluetooth, ONVIF if not needed)

---

## 📊 **System Requirements**

### **ESP32 Requirements**
- ESP32-CAM or compatible board
- Min 4MB flash (8MB recommended)
- PSRAM recommended for higher resolutions
- SD card (optional, for recording)

### **Browser Requirements**
- Modern browser (Chrome 90+, Firefox 88+, Safari 14+, Edge 90+)
- JavaScript enabled
- localStorage enabled
- Canvas support
- WebGL (for TensorFlow.js)

### **Network Requirements**
- WiFi 2.4GHz (ESP32 limitation)
- Stable connection for streaming
- ~1-5 Mbps bandwidth (depends on resolution)

---

## 📄 **License**

This web interface is part of the ESP32-CAM ONVIF project.  
Check the main project README for license information.

---

## 🙏 **Credits**

**Libraries & Technologies**:
- TensorFlow.js - Google
- COCO-SSD Model - TensorFlow
- Inter Font - Rasmus Andersson
- ESP32 Arduino Core

**Developed with ❤️ for the ESP32-CAM community**

---

## 📞 **Support**

For issues, feature requests, or contributions:
- Check the main project repository
- Review existing issues
- Submit detailed bug reports
- Contribute improvements via pull requests

---

**Enjoy your feature-rich ESP32-CAM web interface! 🎉**
