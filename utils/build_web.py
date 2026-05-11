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


def validate_js(js: str, filename: str = "unknown") -> tuple[bool, str]:
    """Basic JavaScript syntax validation using regex patterns."""
    # Check for common syntax issues
    issues = []
    
    # Check for unmatched braces
    open_braces = js.count('{')
    close_braces = js.count('}')
    if open_braces != close_braces:
        issues.append(f"Unmatched braces: {open_braces} open, {close_braces} close")
    
    # Check for unmatched parentheses  
    open_parens = js.count('(')
    close_parens = js.count(')')
    if open_parens != close_parens:
        issues.append(f"Unmatched parentheses: {open_parens} open, {close_parens} close")
    
    # Check for unmatched brackets
    open_brackets = js.count('[')
    close_brackets = js.count(']')
    if open_brackets != close_brackets:
        issues.append(f"Unmatched brackets: {open_brackets} open, {close_brackets} close")
    
    if issues:
        return False, f"{filename}: " + "; ".join(issues)
    return True, "OK"


def minify_js(js: str) -> str:
    """Safe JS minifier using tokenization to protect string/template content.
    
    The old approach applied regexes to the whole source, corrupting:
    - Multi-line template literals (item.innerHTML = `...`)
    - HTML strings passed to innerHTML containing < > operators
    - CSS selectors in querySelector strings
    """
    placeholders = {}
    counter = [0]

    # Tokenize: find strings, template literals, and comments in source order.
    # Template literals may be nested (e.g. `${items.map(i=>`<li>${i}</li>`)}`),
    # so we use a state machine instead of a single regex.
    result = []
    i = 0
    n = len(js)

    while i < n:
        # Block comment /* ... */
        if js[i:i+2] == '/*':
            end = js.find('*/', i + 2)
            if end == -1:
                end = n - 2
            comment = js[i:end+2]
            # Preserve newlines inside block comment for ASI
            newlines = comment.count('\n')
            result.append('\n' * newlines if newlines else ' ')
            i = end + 2
            continue

        # Line comment // ...
        if js[i:i+2] == '//':
            end = js.find('\n', i + 2)
            if end == -1:
                end = n
            result.append('\n')   # preserve line break
            i = end
            continue

        # Double-quoted string
        if js[i] == '"':
            j = i + 1
            while j < n:
                if js[j] == '\\':
                    j += 2
                elif js[j] == '"':
                    j += 1
                    break
                else:
                    j += 1
            key = f'\x00S{counter[0]}\x00'
            placeholders[key] = js[i:j]
            counter[0] += 1
            result.append(key)
            i = j
            continue

        # Single-quoted string
        if js[i] == "'":
            j = i + 1
            while j < n:
                if js[j] == '\\':
                    j += 2
                elif js[j] == "'":
                    j += 1
                    break
                else:
                    j += 1
            key = f'\x00S{counter[0]}\x00'
            placeholders[key] = js[i:j]
            counter[0] += 1
            result.append(key)
            i = j
            continue

        # Template literal ` ... ` (handles nesting via brace counting)
        if js[i] == '`':
            j = i + 1
            depth = 0
            while j < n:
                if js[j] == '\\':
                    j += 2
                elif js[j:j+2] == '${':
                    depth += 1
                    j += 2
                elif js[j] == '}' and depth > 0:
                    depth -= 1
                    j += 1
                elif js[j] == '`' and depth == 0:
                    j += 1
                    break
                else:
                    j += 1
            key = f'\x00S{counter[0]}\x00'
            placeholders[key] = js[i:j]
            counter[0] += 1
            result.append(key)
            i = j
            continue

        result.append(js[i])
        i += 1

    code = ''.join(result)

    # --- Safely minify the code (no string content remains) ---

    # Remove trailing whitespace per line
    code = re.sub(r'[ \t]+\n', '\n', code)
    # Remove leading whitespace (indentation) per line
    code = re.sub(r'\n[ \t]+', '\n', code)
    # Collapse multiple spaces/tabs (but not newlines)
    code = re.sub(r'[ \t]{2,}', ' ', code)
    # Remove spaces around { } ; ,
    code = re.sub(r'[ \t]*([{};,])[ \t]*', r'\1', code)
    # Remove spaces around common operators (horizontal whitespace only)
    code = re.sub(r'[ \t]*([=+\-*/%&|^~!<>?:])[ \t]*', r'\1', code)
    # Collapse multiple blank lines to one
    code = re.sub(r'\n{3,}', '\n\n', code)
    # Remove blank lines entirely
    code = '\n'.join(line for line in code.split('\n') if line.strip())

    # Restore mandatory keyword spaces
    keywords_needing_space = [
        'return', 'const', 'let', 'var', 'if', 'else', 'for', 'while',
        'function', 'async', 'await', 'new', 'typeof', 'instanceof',
        'throw', 'case', 'catch', 'finally', 'switch', 'do', 'try',
        'delete', 'void', 'in', 'of',
    ]
    for kw in keywords_needing_space:
        code = re.sub(
            rf'\b{kw}\b(?=[a-zA-Z0-9_$(\'"`\x00])',
            rf'{kw} ',
            code
        )

    # else if (must come after keyword restoration)
    code = re.sub(r'\belse\s*if\b', 'else if', code)

    # Restore string/template placeholders
    for key, val in placeholders.items():
        code = code.replace(key, val)

    return code


def minify_html(html: str) -> str:
    """Built-in HTML minifier - preserves <script> and <style> content.
    
    Critical: JS/CSS are already minified at inlining time. Collapsing
    whitespace inside <script> blocks breaks JavaScript ASI (Automatic
    Semicolon Insertion) and multi-line string literals.
    """
    # Step 1: Extract script and style blocks, replace with placeholders
    placeholders = {}
    counter = [0]

    def extract_block(match):
        tag = match.group(0)
        key = f'\x00BLOCK{counter[0]}\x00'
        placeholders[key] = tag
        counter[0] += 1
        return key

    # Extract <script>...</script> blocks (preserve their content exactly)
    html = re.sub(r'<script(?:\s[^>]*)?>.*?</script>', extract_block,
                  html, flags=re.DOTALL | re.IGNORECASE)
    # Extract <style>...</style> blocks
    html = re.sub(r'<style(?:\s[^>]*)?>.*?</style>', extract_block,
                  html, flags=re.DOTALL | re.IGNORECASE)

    # Step 2: Minify the surrounding HTML structure
    html = re.sub(r'<!--(?!\[if).*?-->', '', html, flags=re.DOTALL)
    html = re.sub(r'>\s+<', '><', html)
    html = re.sub(r'\s+', ' ', html)
    html = html.strip()

    # Step 3: Re-inject the preserved blocks
    for key, block in placeholders.items():
        html = html.replace(key, block)

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
    """Generate the C++ header file content using gzip compression."""
    import gzip
    
    # Compress the HTML string
    compressed = gzip.compress(html.encode('utf-8'))
    
    # Format as C++ hex array
    hex_array = ', '.join([f'0x{b:02x}' for b in compressed])
    
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    return f'''#pragma once

// ESP32-CAM ONVIF Web Interface - Auto-generated
// Built: {ts} | Original: {len(html):,} bytes | GZipped: {len(compressed):,} bytes
// Edit files in ESP32CAM-ONVIF/data/ then run: python utils/build_web.py

const uint8_t index_html_gz[] PROGMEM = {{ {hex_array} }};
const size_t index_html_gz_len = {len(compressed)};
'''


def main():
    parser = argparse.ArgumentParser(description="Build ESP32-CAM ONVIF Web UI")
    parser.add_argument('--no-minify', action='store_true', help='Skip minification')
    parser.add_argument('-o', '--output', type=str, help='Output file path')
    parser.add_argument('--check', action='store_true', help='Validate JS without building')
    args = parser.parse_args()
    
    print("\n" + "="*50)
    print("  ESP32-CAM ONVIF Web UI Builder")
    print("="*50)
    
    # Paths
    root = get_project_root()
    data = root / "ESP32CAM-ONVIF" / "data"
    out = Path(args.output) if args.output else root / "ESP32CAM-ONVIF" / "index_html.h"
    html_file = data / "index.html"

    # Check if the expected data directory exists, if not try alternative path
    if not data.exists():
        # Try alternative path structure
        data = root / "data"
        out = Path(args.output) if args.output else root / "index_html.h"
        html_file = data / "index.html"

        if not data.exists():
            print(f"\n[ERROR] Data directory not found at {root / 'ESP32CAM-ONVIF' / 'data'} or {root / 'data'}")
            sys.exit(1)
    
    if not html_file.exists():
        print(f"\n[ERROR] Not found: {html_file}")
        sys.exit(1)
    
    print(f"\n[+] Source: {data}")
    print(f"[+] Output: {out}")
    print(f"[+] Minify: {'NO' if args.no_minify else 'YES'}")
    
    # Validate JS files first
    print("\n[0/4] Validating JavaScript...")
    js_valid = True
    if (data / "app.js").exists():
        js_content = read_file(data / "app.js")
        valid, msg = validate_js(js_content, "app.js")
        if not valid:
            print(f"  [!] {msg}")
            js_valid = False
        else:
            print(f"  [+] app.js: {msg}")
    
    if args.check:
        if js_valid:
            print("\n[+] All validations passed!")
            sys.exit(0)
        else:
            print("\n[!] Validation failed!")
            sys.exit(1)
    
    if not js_valid:
        print("\n[!] Continuing build despite validation warnings...")
    
    # Calculate original size
    orig = len(read_file(html_file))
    if (data / "style.css").exists():
        orig += len(read_file(data / "style.css"))
    if (data / "app.js").exists():
        orig += len(read_file(data / "app.js"))
    
    print("\n[1/4] Reading files...")
    html = read_file(html_file)
    
    print("[2/4] Inlining resources...")
    html = inline_resources(html, data, minify=not args.no_minify)
    
    if not args.no_minify:
        print("[3/4] Minifying HTML...")
        html = minify_html(html)
    else:
        print("[3/4] Skipping minification...")
    
    print("[4/4] Generating header...")
    
    html = escape_raw_literal(html)
    header = generate_header(html)
    
    with open(out, 'w', encoding='utf-8') as f:
        f.write(header)
    
    import gzip
    minified_size = len(html.encode('utf-8'))
    gz_size = len(gzip.compress(html.encode('utf-8')))
    
    print(f"\n{'='*50}")
    print(f"  Original:  {orig:,} bytes")
    print(f"  Minified:  {minified_size:,} bytes")
    print(f"  GZipped:   {gz_size:,} bytes")
    print(f"  Saved:     {orig - gz_size:,} bytes ({(orig - gz_size) / orig * 100:.1f}%)")
    print(f"{'='*50}")
    print("\n[+] Done!\n")


if __name__ == "__main__":
    main()
