#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32-CAM ONVIF Web UI Builder
==============================
Converts web source files (HTML, CSS, JS) from data/ folder into a single
embedded PROGMEM header file (index_html.h) for the ESP32.

Features:
- Built-in minification (NO external packages required!)
- Inlines CSS and JS into a single HTML file
- Generates a ready-to-use index_html.h file
- Shows compression statistics

Usage:
    python utils/build_web.py
    python utils/build_web.py --no-minify  # Skip minification

Author: John-Varghese-EH ESP32CAM-ONVIF Project
"""

import os
import sys
import re
import argparse
from pathlib import Path
from datetime import datetime


def get_script_dir():
    """Get the directory where this script is located."""
    return Path(__file__).parent.absolute()


def get_project_root():
    """Get the project root directory (parent of utils)."""
    return get_script_dir().parent


def minify_css(css: str) -> str:
    """Built-in CSS minifier - no external dependencies."""
    # Remove comments
    css = re.sub(r'/\*[\s\S]*?\*/', '', css)
    # Remove unnecessary whitespace
    css = re.sub(r'\s+', ' ', css)
    css = re.sub(r'\s*([{};:,>~+])\s*', r'\1', css)
    css = re.sub(r';\s*}', '}', css)
    # Remove leading/trailing whitespace
    css = css.strip()
    return css


def minify_js(js: str) -> str:
    """Built-in JS minifier - no external dependencies."""
    lines = js.split('\n')
    result = []
    in_multiline_comment = False
    
    for line in lines:
        # Handle multi-line comments
        if in_multiline_comment:
            if '*/' in line:
                line = line[line.index('*/') + 2:]
                in_multiline_comment = False
            else:
                continue
        
        # Remove single-line comments (but not URLs with //)
        if '//' in line and 'http' not in line:
            # Find // that's not in a string
            in_string = False
            string_char = None
            for i, char in enumerate(line):
                if char in '"\'`' and (i == 0 or line[i-1] != '\\'):
                    if not in_string:
                        in_string = True
                        string_char = char
                    elif char == string_char:
                        in_string = False
                elif char == '/' and i + 1 < len(line) and line[i+1] == '/' and not in_string:
                    line = line[:i]
                    break
        
        # Start of multi-line comment
        if '/*' in line:
            before = line[:line.index('/*')]
            after = line[line.index('/*') + 2:]
            if '*/' in after:
                line = before + after[after.index('*/') + 2:]
            else:
                line = before
                in_multiline_comment = True
        
        # Trim whitespace
        line = line.strip()
        if line:
            result.append(line)
    
    # Join lines with single space
    js = ' '.join(result)
    
    # Remove excess whitespace but be careful with operators
    # Only remove spaces around specific safe operators
    js = re.sub(r'\s*([{};,])\s*', r'\1', js)
    js = re.sub(r'\s*([=:?<>!+\-*/&|^~%])\s*', r'\1', js)
    
    # Restore needed spaces after keywords (use word boundary)
    keywords = ['return', 'const', 'let', 'var', 'if', 'else', 'for', 'while', 
                'function', 'async', 'await', 'new', 'typeof', 'instanceof', 'throw']
    for kw in keywords:
        # Only add space if keyword is followed by non-punctuation
        js = re.sub(rf'\b{kw}\b(?=[a-zA-Z0-9_$])', rf'{kw} ', js)
    
    return js


def minify_html(html: str) -> str:
    """Built-in HTML minifier - no external dependencies."""
    # Remove HTML comments (but not conditional comments)
    html = re.sub(r'<!--(?!\[if).*?-->', '', html, flags=re.DOTALL)
    # Collapse whitespace between tags
    html = re.sub(r'>\s+<', '><', html)
    # Collapse other whitespace
    html = re.sub(r'\s+', ' ', html)
    # Remove leading/trailing whitespace
    html = html.strip()
    return html


def read_file(filepath: Path) -> str:
    """Read file content with UTF-8 encoding."""
    with open(filepath, 'r', encoding='utf-8') as f:
        return f.read()


def inline_resources(html: str, data_dir: Path, minify: bool = True) -> str:
    """Replace external CSS/JS references with inline content."""
    
    # Inline CSS files
    def replace_css(match):
        css_file = match.group(1)
        css_path = data_dir / css_file
        if css_path.exists():
            print(f"  [+] Inlining: {css_file}")
            css = read_file(css_path)
            if minify:
                css = minify_css(css)
            return f"<style>{css}</style>"
        print(f"  [!] Not found: {css_file}")
        return match.group(0)
    
    html = re.sub(r'<link\s+[^>]*href=["\']([^"\']+\.css)["\'][^>]*>', 
                  replace_css, html, flags=re.IGNORECASE)
    
    # Inline JS files
    def replace_js(match):
        js_file = match.group(1)
        js_path = data_dir / js_file
        if js_path.exists():
            print(f"  [+] Inlining: {js_file}")
            js = read_file(js_path)
            if minify:
                js = minify_js(js)
            return f"<script>{js}</script>"
        print(f"  [!] Not found: {js_file}")
        return match.group(0)
    
    html = re.sub(r'<script\s+[^>]*src=["\']([^"\']+\.js)["\'][^>]*>\s*</script>', 
                  replace_js, html, flags=re.IGNORECASE)
    
    return html


def escape_raw_literal(content: str) -> str:
    """Escape content for C++ raw string literal."""
    return content.replace(')rawliteral', ')raw_literal')


def generate_header(html: str) -> str:
    """Generate the C++ header file content."""
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    return f'''#pragma once

// ESP32-CAM ONVIF Web Interface - Auto-generated
// Built: {ts} | Size: {len(html):,} bytes
// Edit files in ESP32CAM-ONVIF/data/ then run: python utils/build_web.py

const char index_html[] PROGMEM = R"rawliteral({html})rawliteral";
'''


def main():
    parser = argparse.ArgumentParser(description="Build ESP32-CAM ONVIF Web UI")
    parser.add_argument('--no-minify', action='store_true', help='Skip minification')
    parser.add_argument('-o', '--output', type=str, help='Output file path')
    args = parser.parse_args()
    
    print("\n" + "="*50)
    print("  ESP32-CAM ONVIF Web UI Builder")
    print("="*50)
    
    # Paths
    root = get_project_root()
    data = root / "ESP32CAM-ONVIF" / "data"
    out = Path(args.output) if args.output else root / "ESP32CAM-ONVIF" / "index_html.h"
    html_file = data / "index.html"
    
    if not html_file.exists():
        print(f"\n[ERROR] Not found: {html_file}")
        sys.exit(1)
    
    print(f"\n[+] Source: {data}")
    print(f"[+] Output: {out}")
    print(f"[+] Minify: {'NO' if args.no_minify else 'YES'}")
    
    # Calculate original size
    orig = len(read_file(html_file))
    if (data / "style.css").exists():
        orig += len(read_file(data / "style.css"))
    if (data / "app.js").exists():
        orig += len(read_file(data / "app.js"))
    
    print("\n[1/3] Reading files...")
    html = read_file(html_file)
    
    print("[2/3] Inlining resources...")
    html = inline_resources(html, data, minify=not args.no_minify)
    
    if not args.no_minify:
        print("[3/3] Minifying HTML...")
        html = minify_html(html)
    else:
        print("[3/3] Skipping minification...")
    
    html = escape_raw_literal(html)
    header = generate_header(html)
    
    with open(out, 'w', encoding='utf-8') as f:
        f.write(header)
    
    final = len(header)
    saved = orig - final
    pct = (saved / orig * 100) if orig > 0 else 0
    
    print(f"\n{'='*50}")
    print(f"  Original: {orig:,} bytes")
    print(f"  Final:    {final:,} bytes")
    print(f"  Saved:    {saved:,} bytes ({pct:.1f}%)")
    print(f"{'='*50}")
    print("\n[✓] Done!\n")


if __name__ == "__main__":
    main()
