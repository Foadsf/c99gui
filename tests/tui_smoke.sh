#!/bin/sh
# Smoke test for c99gui TUI (terminal UI).
# Usage: tests/tui_smoke.sh path/to/c99gui[.exe]
# Based on the original smoke test from aaf0c74, adapted for the consolidated repo.
BIN=${1:?usage: tui_smoke.sh <binary>}
ESC=$(printf '\033')
fail=0
pass=0

check() { # name, command...
    name=$1; shift
    if "$@" >/dev/null 2>&1; then 
        pass=$((pass + 1))
        printf 'ok   %s\n' "$name"
    else 
        fail=$((fail + 1))
        printf 'FAIL %s\n' "$name"
    fi
}

snap() { printf "$1" | "$BIN" --snapshot; }       # keys -> final frame

has() { # keys, regex
    snap "$1" | grep -Eq "$2"
}

check "--version"          sh -c "\"$BIN\" --version | grep -q '^c99gui [0-9]'"
check "--help"             sh -c "\"$BIN\" --help | grep -q '^Usage:'"
check "bad option exit 2"  sh -c "\"$BIN\" --bogus 2>/dev/null; [ \$? -eq 2 ]"
check "initial frame"      has ''            'Overview.*Display.*Telemetry.*About'
check "default toggle ON"  has ''            'Notifications.*ON'
check "space toggles"      has ' '           'Notifications.*OFF'
check "enter toggles"      has '\r'          'Notifications.*OFF'
check "vim j/space"        has 'j '          'Dark theme.*OFF'
check "arrow down"         has "${ESC}[B "   'Dark theme.*OFF'
check "tab key moves"      has '\t '         'Dark theme.*OFF'
check "shift-tab wraps"    has "${ESC}[Z "   'Settings applied'
check "slider right"       has 'jjl'         'Volume.*75'
check "slider left"        has 'jjhh'        'Volume.*60'
check "slider clamps"      has 'jjllllllllllllllllllllllllllllllllllllll' 'Volume.*100'
check "choice vim"         has 'jjjl'        'Language.*Deutsch'
check "choice arrow"       has "${ESC}[B${ESC}[B${ESC}[B${ESC}[C" 'Language.*Deutsch'
check "choice utf-8"       has 'jjjlll'      'Fran.*ais'
check "button + status"    has 'jjjj '       'Settings applied'
check "tab digit 4"        has '4'           'Compiler'
check "tab digit 3"        has '3'           'Memory'
check "pgdn next tab"      has "${ESC}[6~"   'Brightness'
check "focus resets"       has '2j '         'Reduce motion.*ON'
check "quit with q"        sh -c "printf 'q' | \"$BIN\" --snapshot >/dev/null"
check "EOF is quit"        sh -c "\"$BIN\" --snapshot </dev/null >/dev/null"
check "no ESC in snapshot" sh -c "! printf 'jj' | \"$BIN\" --snapshot | grep -q \"$ESC\""
check "snapshot is 24 rows" sh -c "[ \$(printf '' | \"$BIN\" --snapshot | wc -l) -eq 24 ]"

# Real (non-snapshot) mode with piped I/O: must enter and leave the alt screen.
OUT=$(printf 'jj \033q' | "$BIN" | od -An -v -c | tr -d ' \n')
case $OUT in
    *'033[?1049h'*) pass=$((pass + 1)); echo "ok   alt screen entered" ;;
    *) fail=$((fail + 1)); echo "FAIL alt screen entered" ;;
esac
case $OUT in
    *'033[?25h033[?1049l') pass=$((pass + 1)); echo "ok   terminal restored" ;;
    *) fail=$((fail + 1)); echo "FAIL terminal restored" ;;
esac
case $OUT in
    *'033[?2026h'*) pass=$((pass + 1)); echo "ok   frames emitted" ;;
    *) fail=$((fail + 1)); echo "FAIL frames emitted" ;;
esac

echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
