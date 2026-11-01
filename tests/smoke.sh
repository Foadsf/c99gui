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

# validate_bmp FILE - returns 0 if valid, 1 if invalid
# Reads width/height from BMP header, verifies size matches expected
validate_bmp() {
    bmp_file=$1
    
    if [ ! -f "$bmp_file" ]; then
        printf 'FAIL file not found: %s\n' "$bmp_file"
        return 1
    fi
    
    # Read width and height as unsigned 32-bit little-endian integers
    # od -An -tu4 outputs: width height (two 32-bit unsigned values)
    DIMS=$(od -An -tu4 -j18 -N8 "$bmp_file" 2>/dev/null)
    if [ -z "$DIMS" ]; then
        printf 'FAIL could not read BMP header: %s\n' "$bmp_file"
        return 1
    fi
    
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
    
    if [ "$WIDTH" -le 0 ] || [ "$HEIGHT" -le 0 ]; then
        printf 'FAIL invalid BMP dimensions (got %dx%d): %s\n' "$WIDTH" "$HEIGHT" "$bmp_file"
        return 1
    fi
    
    EXPECTED_SIZE=$((54 + WIDTH * HEIGHT * 4))
    ACTUAL_SIZE=$(wc -c < "$bmp_file" | tr -d ' ')
    
    if [ "$ACTUAL_SIZE" -eq "$EXPECTED_SIZE" ]; then
        return 0
    else
        printf 'FAIL size mismatch (expected %s, got %s for %dx%d): %s\n' "$EXPECTED_SIZE" "$ACTUAL_SIZE" "$WIDTH" "$HEIGHT" "$bmp_file"
        return 1
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

# Validate the real snapshot (must pass)
if [ -f "$TMP_BMP" ]; then
    if validate_bmp "$TMP_BMP"; then
        pass=$((pass + 1))
        printf 'ok   snapshot BMP valid\n'
    else
        fail=$((fail + 1))
    fi
else
    fail=$((fail + 1))
    printf 'FAIL snapshot file not created\n'
fi

# Negative control 1: truncated file (must FAIL)
TMP_TRUNC="/tmp/c99gui_snap_trunc_$$.bmp"
if [ -f "$TMP_BMP" ]; then
    ACTUAL_SIZE=$(wc -c < "$TMP_BMP" | tr -d ' ')
    TRUNC_SIZE=$((ACTUAL_SIZE - 1))
    
    # Portable truncate using dd (works on Linux, macOS, BSD, WSL)
    if dd if="$TMP_BMP" of="$TMP_TRUNC" bs=1 count="$TRUNC_SIZE" 2>/dev/null; then
        if validate_bmp "$TMP_TRUNC"; then
            fail=$((fail + 1))
            printf 'FAIL negative control: truncated BMP should have been rejected\n'
        else
            pass=$((pass + 1))
            printf 'ok   negative control: truncated BMP correctly rejected\n'
        fi
    else
        fail=$((fail + 1))
        printf 'FAIL negative control: could not create truncated file\n'
    fi
    rm -f "$TMP_TRUNC"
fi

# Negative control 2: corrupted width byte (must FAIL)
TMP_CORRUPT="/tmp/c99gui_snap_corrupt_$$.bmp"
if [ -f "$TMP_BMP" ]; then
    # Copy original, then corrupt byte at offset 18 (width LSB)
    cp "$TMP_BMP" "$TMP_CORRUPT"
    # Write a different byte (0x00) at offset 18
    if printf '\000' | dd of="$TMP_CORRUPT" bs=1 seek=18 count=1 conv=notrunc 2>/dev/null; then
        if validate_bmp "$TMP_CORRUPT"; then
            fail=$((fail + 1))
            printf 'FAIL negative control: corrupted-width BMP should have been rejected\n'
        else
            pass=$((pass + 1))
            printf 'ok   negative control: corrupted-width BMP correctly rejected\n'
        fi
    else
        fail=$((fail + 1))
        printf 'FAIL negative control: could not corrupt width byte\n'
    fi
    rm -f "$TMP_CORRUPT"
fi

rm -f "$TMP_BMP"

echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
