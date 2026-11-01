# Changelog

All notable changes to `c99gui` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-11-01 (branch: `gui`)

### Added
- **Monolithic Pure C99 Pixel Desktop GUI Engine** (`src/gui.c`):
  - 32-bit ARGB software rasterizer operating on a 760x480 pixel framebuffer without dynamic memory allocations (`malloc`-free).
  - Sub-pixel anti-aliased rounded rectangles using squared-distance fields (`draw_rrect`) without floating-point `sqrt` or `<math.h>` dependency.
  - Fixed-point 8.8 alpha blending (`blend_argb`) with translucent soft drop shadows (`draw_card`).
  - Bit-packed 4x5 vector-scaled ASCII glyph font (`glyph_bits`, `draw_text`, `draw_int`).
- **Immediate-Mode Widget Library**:
  - `gui_begin_panel`: Bordered surface with header, title, and separator.
  - `gui_nav_item`: Sidebar navigation button with active, hover, and selection highlights.
  - `gui_toggle`: Animated pill switch with state transitions.
  - `gui_slider`: Draggable progress/value track with thumb and percentage readout.
  - `gui_button`: Push button with hover and active click feedback.
- **Application Workspaces & Controls**:
  - **Dashboard**: Hardware V-Sync Lock, Subpixel Anti-Aliasing, Master Output Gain, Modulation Frequency, and animated real-time signal waveform visualizer using fast parabolic sine approximation.
  - **Renderer**: Backlight Luminance slider, 10-bit HDR Compositing toggle, and Reset Defaults action.
  - **Telemetry**: Simulated compute load slider, Trigger Pulse action, and real-time execution count.
  - **Accent Themes**: Interactive swatches switching palette dynamically (Catppuccin Blue, Green, Purple, Yellow).
- **Native OS Windowing Backends**:
  - **Windows**: Win32 GDI (`StretchDIBits` top-down DIB) with mouse tracking, `SetCapture`, and message pump.
  - **Linux / BSD**: Native X11 (`XPutImage`) with event handling.
  - **macOS**: Native Cocoa / CoreGraphics via C Objective-C runtime (`objc_msgSend`).
  - **Headless / CI**: Built-in 32-bit uncompressed BMP exporter (`--snapshot <file.bmp>`) and automated headless test suite (`--test`).
- **Universal Multi-Compiler Support**:
  - Warning-free compilation under MSVC (`/W4 /WX`), GCC (`-Wall -Wextra -Wpedantic -Werror`), and Clang.
  - Verified across Visual Studio 2026 Community (x64, x86), Visual Studio 2019 BuildTools (x64, x86), MSYS2 UCRT64 GCC, Cygwin GCC & Clang, and WSL Ubuntu GCC.
