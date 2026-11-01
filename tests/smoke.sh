#!/bin/sh
# Headless smoke test for c99gui desktop GUIs (pixel, vector, suite).
# Usage: tests/smoke.sh path/to/gui[.exe]
# Reads BMP dimensions from header using portable od (works on Linux, macOS, Git bash, MSYS2, WSL).
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

# Validate BMP: read width (offset 18) and height (offset 22) as little-endian uint32
# biHeight is a signed int32; negative means top-down DIB. Take absolute value.
# Expected size = 54 (header) + width * abs(height) * 4 (32-bit ARGB)
if [ -f "$TMP_BMP" ]; then
    # Read width and height as unsigned 32-bit little-endian integers
    # od -An -tu4 outputs: width height (two 32-bit unsigned values)
    DIMS=$(od -An -tu4 -j18 -N8 "$TMP_BMP" 2>/dev/null)
    if [ -n "$DIMS" ]; then
        set -- $DIMS
        WIDTH=$1
        HEIGHT_U=$2
        
        # Convert unsigned height to signed 32-bit
        if [ "$HEIGHT_U" -ge 2147483648 ]; then
            HEIGHT_S=$((HEIGHT_U - 4294967296))
        else
            HEIGHT_S=$HEIGHT_U
        fi
        
        # Take absolute value for top-down DIB (negative height)
        if [ "$HEIGHT_S" -lt 0 ]; then
            HEIGHT=$(( -HEIGHT_S ))
        else
            HEIGHT=$HEIGHT_S
        fi
        
        if [ "$WIDTH" -gt 0 ] && [ "$HEIGHT" -gt 0 ]; then
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
            printf 'FAIL invalid BMP dimensions (got %dx%d)\n' "$WIDTH" "$HEIGHT"
        fi
    else
        fail=$((fail + 1))
        printf 'FAIL could not read BMP header\n'
    fi
    
    # Negative control: truncate the snapshot by 1 byte and verify the check FAILS
    TMP_TRUNC="/tmp/c99gui_snap_trunc_$$.bmp"
    if [ -f "$TMP_BMP" ]; then
        # Copy and truncate by 1 byte
        head -c -1 "$TMP_BMP" > "$TMP_TRUNC" 2>/dev/null
        if [ -f "$TMP_TRUNC" ]; then
            TRUNC_SIZE=$(wc -c < "$TMP_TRUNC" | tr -d ' ')
            # The check should FAIL on the truncated file
            if [ "$TRUNC_SIZE" -eq "$((ACTUAL_SIZE - 1))" ]; then
                # Verify it would fail: expected size won't match
                TRUNC_EXPECTED=$EXPECTED_SIZE
                if [ "$TRUNC_SIZE" -ne "$TRUNC_EXPECTED" ]; then
                    pass=$((pass + 1))
                    printf 'ok   negative control: truncated BMP correctly rejected\n'
                else
                    fail=$((fail + 1))
                    printf 'FAIL negative control: truncated BMP should have been rejected\n'
                fi
            fi
        fi
        rm -f "$TMP_TRUNC"
    fi
    
    rm -f "$TMP_BMP"
else
    fail=$((fail + 1))
    printf 'FAIL snapshot file not created\n'
fi

echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
