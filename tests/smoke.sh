#!/bin/sh
# Headless smoke test for c99gui desktop GUIs (pixel, vector, suite).
# Usage: tests/smoke.sh path/to/gui[.exe]
# Validates BMP export by checking file exists and has reasonable size (>54 bytes header).
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

# Validate BMP: file exists, is > 54 bytes (minimum BMP header), starts with BM
if [ -f "$TMP_BMP" ]; then
    SIZE=$(wc -c < "$TMP_BMP" | tr -d ' ')
    HEADER=$(od -An -tx1 -N2 "$TMP_BMP" 2>/dev/null | tr -d ' \n')
    
    if [ "$SIZE" -gt 54 ] && [ "$HEADER" = "424d" ]; then
        pass=$((pass + 1))
        printf 'ok   snapshot BMP valid (%s bytes, starts with BM)\n' "$SIZE"
    else
        fail=$((fail + 1))
        printf 'FAIL snapshot BMP invalid (size=%s, header=%s)\n' "$SIZE" "${HEADER:-unknown}"
    fi
    rm -f "$TMP_BMP"
else
    fail=$((fail + 1))
    printf 'FAIL snapshot file not created\n'
fi

echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
