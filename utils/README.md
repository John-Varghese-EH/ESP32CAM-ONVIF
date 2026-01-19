# ESP32-CAM ONVIF Utilities

Build tools for the ESP32-CAM ONVIF project.

## build_web.py

Converts web source files from `ESP32CAM-ONVIF/data/` into an embedded PROGMEM header file.

### Features

✅ **Built-in minification** - No external packages required!
- CSS minifier (removes whitespace, comments)
- JavaScript minifier (removes comments, compresses)
- HTML minifier (collapses whitespace)

✅ **Inlines resources** - Combines CSS/JS into single HTML file

✅ **Stats output** - Shows size reduction

### Usage

```bash
# From project root directory
python utils/build_web.py

# Skip minification (for debugging)
python utils/build_web.py --no-minify

# Custom output path
python utils/build_web.py -o custom_output.h
```

### Workflow

1. **Edit source files** in `ESP32CAM-ONVIF/data/`:
   - `index.html` - HTML structure
   - `style.css` - CSS styling
   - `app.js` - JavaScript logic

2. **Build**:
   ```bash
   python utils/build_web.py
   ```

3. **Result**: Generates optimized `ESP32CAM-ONVIF/index_html.h`

4. **Compile** and upload firmware

### Example Output

```
==================================================
  ESP32-CAM ONVIF Web UI Builder
==================================================

[+] Source: ESP32CAM-ONVIF/data
[+] Output: ESP32CAM-ONVIF/index_html.h
[+] Minify: YES

[1/3] Reading files...
[2/3] Inlining resources...
  [+] Inlining: style.css
  [+] Inlining: app.js
[3/3] Minifying HTML...

==================================================
  Original: 25,898 bytes
  Final:    19,544 bytes
  Saved:    6,354 bytes (24.5%)
==================================================

[✓] Done!
```

## File Structure

```
utils/
├── README.md       ← This file
└── build_web.py    ← Web UI builder
```
