#!/bin/sh
# Headless smoke test for c99gui vector engine.
# Usage: tests/smoke.sh path/to/gui[.exe]
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

# Validate BMP: 2508854 bytes (54 header + 980*640*4 bytes)
if [ -f "$TMP_BMP" ]; then
    SIZE=$(wc -c < "$TMP_BMP" | tr -d ' ')
    if [ "$SIZE" -eq 2508854 ]; then
        pass=$((pass + 1))
        printf 'ok   snapshot size exact (2508854 bytes)\n'
    else
        fail=$((fail + 1))
        printf 'FAIL snapshot size mismatch (got %s)\n' "$SIZE"
    fi
    rm -f "$TMP_BMP"
fi

echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]

