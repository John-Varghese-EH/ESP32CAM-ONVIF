# Web UI Source Files

This folder contains the **source files** for the ESP32-CAM ONVIF web interface.

## Important Notes

⚠️ **These files are NOT directly used by the firmware!**

The actual web interface is embedded in `index_html.h` as a single PROGMEM string for better performance and easier deployment.

## File Structure

| File | Description |
|------|-------------|
| `index.html` | HTML structure |
| `style.css` | Styling |
| `app.js` | JavaScript logic |

## Building index_html.h

To update the embedded web UI after making changes here:

1. Edit the files in this folder
2. Combine and minify them into `index_html.h`
3. Wrap in PROGMEM raw string literal

### Manual Method

```cpp
// index_html.h
#pragma once

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<!-- Paste your combined HTML/CSS/JS here -->
</html>
)rawliteral";
```

### Automated (Optional)

You can use tools like:
- `html-minifier` + custom script
- PlatformIO pre-build script

## Current Status

The `index_html.h` contains the **latest** embedded UI.
These source files may be outdated - use for reference only.
