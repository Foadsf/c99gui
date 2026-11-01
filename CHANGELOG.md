# Changelog

All notable changes to `c99gui` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-11-01

### Added
- **Monolithic Single-File C99 IMGUI Architecture**: Zero dynamic memory allocation (`malloc`-free, static buffers only).
- **TrueColor 24-bit ANSI Software Framebuffer**:
  - Cell-based differential double-buffering emitting synchronized updates (`\033[?2026h`).
  - Drop shadows, rounded border glyphs (`╭─╮│╰─╯`), star markers, and smooth gradient sliders.
  - Catppuccin Mocha (dark) and Latte (light) color themes.
  - Automatic fallback to 256-color palette via `--256` or `C99GUI_COLOR=256`.
- **Immediate-Mode Widget Library**:
  - `ui_tabs`: Tab bar navigation (switchable via `1`–`4` or `PgUp`/`PgDown`).
  - `ui_section`: Section separator with decorative horizontal rule.
  - `ui_toggle`: On/Off state toggle with pill indicator.
  - `ui_slider`: Numeric value range with gradient fill bar.
  - `ui_choice`: Option cycler with angle brackets.
  - `ui_progress`: Non-interactive percentage progress bar.
  - `ui_button`: Push button triggering immediate callback action.
  - `ui_info`: Read-only property key-value layout row.
- **Cross-Platform Input Layer**:
  - **Windows**: Console API (`ReadConsoleInputW`) supporting arrow keys, PageUp/Down, Tab, and Shift-Tab, with VT mode auto-initialization and clean terminal state restoration on Ctrl-C / exit.
  - **POSIX (Linux, macOS, BSD, Cygwin, MSYS2)**: Raw termios with `pselect` and signal handling (`SIGINT`, `SIGTERM`, `SIGWINCH`).
  - **Strict ISO C99 Mode (`-DSTRICT_C99`)**: Pure standard C99 stdio fallback for bare-metal, embedded, or unsupported hosts.
- **Automated Verification & CI**:
  - Headless `--snapshot` rendering mode for deterministic unit and integration testing.
  - Test suite (`tests/smoke.sh`) covering widgets, keyboard navigation, and alt-screen lifetime.
  - Windows multi-compiler test harness (`scripts/build-matrix.ps1`) verifying MSVC (2026 Community & 2019 BuildTools, x64 and x86), MSYS2 UCRT64 GCC, Cygwin GCC & Clang, and WSL Ubuntu GCC.
  - Multi-platform GitHub Actions workflow (`.github/workflows/ci.yml`).
  - Build scripts: `Makefile`, `CMakeLists.txt`, and `build.cmd`.
