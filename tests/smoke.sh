#!/bin/sh
# Headless smoke test for c99gui desktop GUIs (pixel, vector, suite).
# Usage: tests/smoke.sh path/to/gui[.exe]
# Reads BMP dimensions from the header instead of hardcoding size.
BIN=${1:?usage: smoke.sh <binary>}
fail=0
pass=0

check() {
    name=$1; shift
    if "$@" >/dev/null 2>&1; then
        pass=$((pass + 1))
        printf 'ok   %s\n' "$name"
    else
        fail=$((fail + 1))
        printf 'FAIL %s\n' "$name"
    fi
}

check "--version"           sh -c "\"$BIN\" --version | grep -q '^c99gui [0-9]'"
check "--help"              sh -c "\"$BIN\" --help | grep -q '^Usage:'"
check "bad option exit 2"   sh -c "\"$BIN\" --bogus 2>/dev/null; [ \$? -eq 2 ]"
check "headless simulation" "$BIN" --headless
check "interactive engine tests" "$BIN" --test

TMP_BMP="/tmp/c99gui_snap_$$.bmp"
[ -d "/tmp" ] || TMP_BMP="c99gui_snap_$$.bmp"
check "snapshot BMP export" "$BIN" --snapshot "$TMP_BMP"

# Validate BMP: read width and height from header (bytes 18-21 and 22-25, little-endian)
# Expected size = 54 (header) + width * height * 4 (32-bit ARGB)
if [ -f "$TMP_BMP" ]; then
    # Read width (4 bytes LE at offset 18) and height (4 bytes LE at offset 22)
    # Using od to extract bytes and awk to compute
    WIDTH=$(od -An -tu4 -j18 -N4 "$TMP_BMP" | tr -d ' ')
    HEIGHT=$(od -An -tu4 -j22 -N4 "$TMP_BMP" | tr -d ' ')
    
    # Handle signed height (negative means top-down)
    HEIGHT=${HEIGHT#-}
    
    if [ -n "$WIDTH" ] && [ -n "$HEIGHT" ]; then
        EXPECTED_SIZE=$((54 + WIDTH * HEIGHT * 4))
        ACTUAL_SIZE=$(wc -c < "$TMP_BMP" | tr -d ' ')
        
        if [ "$ACTUAL_SIZE" -eq "$EXPECTED_SIZE" ]; then
            pass=$((pass + 1))
            printf 'ok   snapshot size correct (%dx%d = %s bytes)\n' "$WIDTH" "$HEIGHT" "$ACTUAL_SIZE"
        else
            fail=$((fail + 1))
            printf 'FAIL snapshot size mismatch (expected %s, got %s for %dx%d)\n' "$EXPECTED_SIZE" "$ACTUAL_SIZE" "$WIDTH" "$HEIGHT"
        fi
    else
        fail=$((fail + 1))
        printf 'FAIL could not read BMP dimensions\n'
    fi
    rm -f "$TMP_BMP"
else
    fail=$((fail + 1))
    printf 'FAIL snapshot file not created\n'
fi

echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
