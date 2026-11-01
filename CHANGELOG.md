# Changelog

All notable changes to `c99gui` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.1.0] - Unreleased

### Added
- **Four-program consolidation:** All four c99gui variants now ship side-by-side in a single repository:
  - `c99gui-tui`: Terminal UI (zero dependencies, from branch `tui` aaf0c74)
  - `c99gui-pixel`: Pixel desktop GUI (from branch `gui` eff0023)
  - `c99gui-vector`: Vector desktop GUI (from branch `vector-gui` 8e39b21)
  - `c99gui-suite`: Comprehensive vector GUI suite (from branch `vector-suite` 9a70006)
- **CI workflow** (`.github/workflows/ci.yml`): Verifies all four programs on Linux (GCC/Clang, X11/headless), macOS (Clang, Cocoa/headless), Windows MSVC (x64), and Windows MSYS2 (UCRT64 GCC) with strict warnings (`-Werror` / `/WX`).
- **Release workflow** (`.github/workflows/release.yml`): Builds release artifacts for Linux x64, macOS arm64, and Windows x64; supports dry-run mode for testing.
- **Smoke tests:** `tests/smoke.sh` (GUI programs, reads BMP dimensions from header) and `tests/tui_smoke.sh` (TUI, restored from aaf0c74).
- **Build system updates:** Makefile, CMakeLists.txt, `build.cmd`, and `scripts/build-matrix.ps1` now build all four programs with distinct names.

### Changed
- **README.md:** Honest documentation of all four programs with accurate build commands, CLI options, and CI verification status. Removed unverified claims about Cocoa/X11 backends (now CI-verified).
- **BMP size validation:** Smoke test now reads width/height from BMP header instead of hardcoding 2,508,854 bytes (which assumed fixed 980x640).

### Fixed
- CI workflow now references correct source paths (`src/tui/c99gui.c`, `src/pixel/gui.c`, etc.) instead of non-existent `src/gui.c`.
- Release workflow includes all four programs in release archives.
- Documentation no longer claims "13-stage regression suite" or features not present in the actual UI (e.g., "[78%]" readouts on knobs, "ACCENT THEME" in suite).

## [2.0.0] - 2026-11-01 (branch: `vector-suite`)

### Added
- **Comprehensive Pure C99 2D Vector Graphics & Immediate-Mode GUI Framework** (`src/suite/gui.c`):
  - Resolution expanded to 980x640 32-bit ARGB framebuffer with zero heap allocations.
  - Hardware-style **Scissor Clipping Stack** (`vg_push_clip`, `vg_pop_clip`, `vg_reset_clip`) for nested viewports and inputs.
  - **Scanline Winding-Rule Polygon Fill** (`vg_fill`) with sub-pixel edge smoothing.
  - **Single-Pass Unified SDF Path Stroke** (`vg_stroke`) with clamped line projection distance fields.
  - **Bilinear 4-Corner Gradients** (`vg_fill_gradient`) with analytical SDF rounded box clipping.
  - **Multi-Column Grid Engine** (`ui_columns`, `ui_next_column`, `ui_end_columns`) for responsive layouts.
  - **Scrollable Viewports** (`ui_begin_scroll`, `ui_end_scroll`) with vertical wheel scrolling and draggable vector scrollbars.
  - **Per-Widget Lerp Animation Table** (`ui_anim_step`) providing smooth 60 FPS state transitions.
  - **Deferred Z-Order Overlay Pass**: Dropdown popup menus, right-click context menus, modal confirmation dialogs, animated toast notifications, floating hover tooltips.
- **Suite of 5 Interactive Workspaces**:
  - **Controls Suite**: Single-line text input box, dropdown combo box, animated toggle switches, vector checkboxes, radio buttons, continuous sliders, rotary vector knobs, action buttons.
  - **Node Studio**: Interactive draggable node graph editor with 4 nodes, cubic Bézier patch cables, signal ports.
  - **Data & Charts**: Real-time filled vector area waveform, telemetry, selectable process data table with status badges.
  - **Color Picker**: Interactive 2D HSV Saturation/Value gradient box, continuous Hue slider, live Hex readout, Light/Dark mode switcher.
  - **Scaling & Modals**: Resolution-independent live UI vector scaling (0.75x – 1.30x), modal confirmation dialog, toast notification spawner.
- **Headless & Regression Testing**: 13-stage automated regression test suite (`--test`), 32-bit BMP exporter (`--snapshot`), headless simulation mode (`--headless`).

## [1.0.0] - 2026-11-01 (branch: `vector-gui`)

### Added
- **Monolithic Canonical Pure C99 2D Vector Graphics & Immediate-Mode GUI Engine** (`src/vector/gui.c`):
  - Resolution-independent SDF and Quadratic/Cubic Bézier vector rasterizer (860x540 32-bit ARGB).
  - Fast compiler- and libm-agnostic float math (`vg_sqrtf`, `vg_sinf`, `vg_cosf`).
  - Proportional SVG-style quadratic Bézier typography with sub-pixel anti-aliasing.
  - Unified multi-segment vector path rasterizer with clamped line projection distance fields.
  - Analytical Euclidean Signed Distance Fields (SDF) for rounded rects and soft drop shadows.
  - Resolution-independent dynamic UI Vector DPI Scaling (0.75x – 1.30x).
- **Immediate-Mode Vector Widgets**: `ui_card`, `ui_nav_item`, `ui_slider`, `ui_toggle`, `ui_button`, `ui_radial_gauge`.
- **Application Workspaces**: Dashboard, Vector Paths, Telemetry, Accent Themes.
- **Native OS Windowing Backends**: Win32 GDI, X11, Cocoa/CoreGraphics, Headless.

### Added (branch: `gui`)
- **Monolithic Pure C99 Pixel Desktop GUI Engine** (`src/pixel/gui.c`):
  - 32-bit ARGB software rasterizer (760x480 pixels).
  - Sub-pixel anti-aliased rounded rectangles using squared-distance fields.
  - Fixed-point 8.8 alpha blending with translucent soft drop shadows.
  - Bit-packed 4x5 vector-scaled ASCII glyph font.
- **Immediate-Mode Widget Library**: `gui_begin_panel`, `gui_nav_item`, `gui_toggle`, `gui_slider`, `gui_button`.
- **Application Workspaces**: Dashboard, Renderer, Telemetry, Accent Themes.

## [0.1.0] - 2026-11-01 (branch: `tui`)

### Added
- **c99gui TUI**: Single-file, zero-allocation, immediate-mode terminal GUI in C99.
- UTF-8 cell framebuffer with 24-bit foreground/background color.
- VT100/ANSI key decoding, alt screen mode, hidden cursor.
- Four-tab application: Overview, Display, Telemetry, About.
- Smoke test suite (`tests/smoke.sh`) with keyboard scripting via `--snapshot`.
