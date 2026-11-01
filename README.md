# c99gui

A single-file, zero-allocation, immediate-mode **terminal GUI in portable C99**.
No libraries, no build system required, no heap. Truecolor, rounded windows,
drop shadows, tabs, toggles, sliders, choices, progress bars and buttons, driven
by keyboard (vim keys and arrows). Meant to be copied into new projects as a
starting point.

```
 ╭──●─●─●─────────────────────✦─C99 GUI Template────────────────────────────╮
 │   1 Overview   2 Display   3 Telemetry   4 About                         │
 │  GENERAL ──────────────────────────────────────────────────────────────  │
 │     Notifications                                                ● ON    │
 │     Dark theme                                                   ● ON    │
 │     Volume                                     ━━━━━━━━━━━━──────   70   │
 │   ▸ Language                                               ‹ English ›   │
 │        Apply                                                             │
 │  Ready                       j/k Move  h/l Adjust  Enter Select  q Quit  │
 ╰──────────────────────────────────────────────────────────────────────────╯
```
(plain-text output of `c99gui --snapshot`; the real thing is colored.)

## Build

Pick whichever line matches your toolchain; the program is one file, `src/c99gui.c`.

| Toolchain | Command |
|---|---|
| GCC / Clang / any POSIX `cc` | `cc -std=c99 -O2 -Wall -Wextra -pedantic src/c99gui.c -o c99gui` |
| `make` | `make` (add `CC=clang`, `make strict`, `make static`, `make check`) |
| MSVC | `build.cmd` or `cl /std:c11 /W4 /O2 /MT /utf-8 src\c99gui.c /Fe:c99gui.exe` |
| CMake | `cmake -B build && cmake --build build` (`-DC99GUI_STRICT=ON` for ISO-only) |
| Strict ISO C99 (no OS headers) | `cc -std=c99 -DSTRICT_C99 src/c99gui.c -o c99gui` |

Run `./c99gui` in a UTF-8, ANSI-capable terminal (Windows Terminal, conhost on
Windows 10 1809+, any xterm-like emulator). Keys:

| Key | Action |
|---|---|
| `j` `k` / `↓` `↑` / `Tab` `Shift-Tab` | move focus |
| `h` `l` / `←` `→` | adjust slider, switch toggle/choice |
| `Space` `Enter` | activate |
| `1`–`4`, `PgUp` `PgDn` | switch tab |
| `q` `Esc` `Ctrl-C` | quit |

Options: `--snapshot` (headless, see below), `--256` (256-color palette),
`--help`, `--version`. `C99GUI_COLOR=256|truecolor` overrides detection (256 is
chosen automatically on Apple Terminal and the Linux console).

## Use it as a template

1. Copy `src/c99gui.c` (and `LICENSE`) into your project.
2. Edit the **APPLICATION** section: `struct App` for your state, `app_frame()`
   for your screens. Each widget is a single call per frame:

   ```c
   ui_toggle("Notifications", &a->notifications);
   ui_slider("Volume", &a->volume, 0, 100, 5);
   if (ui_button("Apply")) ui_status("Applied.");
   ```
3. Leave the engine and platform layer alone. All OS-specific code is in the
   **PLATFORM LAYER** section and exports four functions: `sys_init`,
   `sys_shutdown`, `sys_size`, `sys_key`.

Behaviour you get without writing it: centred, resizable layout (re-laid-out on
terminal resize), "terminal too small" message, differential redraw inside a
synchronized-update block (no flicker), alt-screen with guaranteed restore of
the terminal on exit, `Ctrl-C`, `SIGTERM`/`SIGHUP` and console close, dark/light
themes, UTF-8 text clipping, and a one-frame re-render after any state change so
the screen never lags a keystroke.

### Gotchas

- The source is **7-bit ASCII on purpose** (MSVC reads sources in the system
  code page). Write glyphs as `\x` escapes. A hex escape swallows every hex
  digit after it: write `"Fran\xC3\xA7" "ais"`, never `"\xA7ais"`.
- Text is one cell per code point. East-Asian wide glyphs and emoji are not
  measured; keep to single-width characters.
- Every widget reads its label immediately and `ui_status` copies its text, so
  locals and temporaries are safe to pass.
- `FB_MAX_W x FB_MAX_H` (240x80) bounds the framebuffer; larger terminals are
  filled up to that size.

## Test

```sh
make test                      # builds and runs tests/smoke.sh
sh tests/smoke.sh build/c99gui # against any binary
```

`--snapshot` reads keys from stdin, renders an 80x24 frame as plain text and
exits, so the UI is testable with `printf` and `grep` on every platform.
`scripts/build-matrix.ps1` builds and tests every compiler/environment found on
a Windows host (MSVC, MSYS2, Cygwin, WSL) with warnings as errors.

## Platform support

| Platform | Input | Notes |
|---|---|---|
| Windows (MSVC, MinGW, clang) | `ReadConsoleInputW` | VT mode enabled automatically; UTF-8 code page set and restored |
| Linux, macOS, BSD, Cygwin, MSYS2, WSL | termios + `pselect` | raw mode restored on exit and fatal signals |
| anything else, or `-DSTRICT_C99` | `getchar` | one key per line (type key, press Enter) |

See `CHANGELOG.md` for the verification status of each environment.

## License

MIT, see [LICENSE](LICENSE).
