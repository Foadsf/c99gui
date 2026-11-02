# Changelog

All notable changes to `c99gui` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.1.0] - Unreleased

### Added
- **All four programs in one repository**, built side by side under distinct names:
  - `c99gui-tui`: terminal UI (`src/tui/c99gui.c`, from branch `tui` aaf0c74)
  - `c99gui-pixel`: pixel desktop GUI (`src/pixel/gui.c`, from branch `gui` eff0023)
  - `c99gui-vector`: vector desktop GUI (`src/vector/gui.c`, from branch `vector-gui` 8e39b21)
  - `c99gui-suite`: vector GUI suite (`src/suite/gui.c`, from branch `vector-suite` 9a70006)
- **CI** (`.github/workflows/ci.yml`): compiles all four programs with warnings as errors on Linux (GCC and Clang, X11 and headless), macOS (Clang, Cocoa and headless), Windows MSVC x64 and Windows MSYS2 UCRT64 GCC, then runs the headless `--test` and `--snapshot` checks. Uses Node 24 action versions.
- **Release workflow** (`.github/workflows/release.yml`): on a `v*` tag, builds Linux x64, macOS arm64 and Windows x64 archives (each with a top-level folder, README and LICENSE), runs the smoke tests, publishes `SHA256SUMS.txt` and takes the release notes from this file. A manual run rehearses everything except publishing.
- **`scripts/extract-notes.sh`**: extracts one version's section from this file and fails if it is missing or empty. CI runs it on every push.
- **Smoke tests**: `tests/smoke.sh` reads BMP dimensions from the header and includes truncated and corrupted BMP negative controls, and requires each GUI's `C99GUI_TEST_NEGATIVE=1` run to exit 1 with at least 5 real FAIL lines (exit 1 alone can be faked); `tests/tui_smoke.sh` is restored from aaf0c74.

### Changed
- **Pixel GUI**: the window is resizable and maximizable (Win32 `WS_OVERLAPPEDWINDOW`, `WM_SIZE`, `WM_GETMINMAXINFO`; X11 `ConfigureNotify`; Cocoa resizable style). The layout reflows from the live window size with a 640x450 minimum, and the waveform fills the remaining height.
- **Pixel GUI font**: the hand-drawn glyphs (several malformed, e.g. `T`, `%`, `&`, `g`, `y`, `\`) are replaced by the VGA 8x16 font from Debian console-setup (`Lat15-VGA16.psf.gz`), ASCII 32-126, which console-setup states is public domain.
- **Pixel GUI tests**: every widget records its rectangle while drawing. Tests click widgets by name and check every rectangle on every tab against the window and its parent panel at 640x450 and 1920x1080. `C99GUI_TEST_NEGATIVE=1` renders every tab at 300x200 and must fail.
- **Vector GUI**: the window is resizable and maximizable (Win32 `WM_SIZE`/`WM_GETMINMAXINFO`; X11 `ConfigureNotify` with min/max size hints; Cocoa reads the live content size each frame, so the layout reflows instead of stretching). Long text fits its card at 640x400; the Synthesizer knobs are drawn as arcs with their values and labels clear of them; Bezier tessellation scales with curve length.
- **Vector GUI punctuation**: one glyph set for `` ! " # $ & ' * , ; < = > ? @ [ \ ] ^ _ ` { | } ~ ``; the comma was missing and `#`, `$`, `&`, `?`, `@`, brackets, braces and `~` were malformed.
- **Vector GUI tests**: every widget and every text string records its rectangle (text measured from the glyph box). Tests click by name, assert each state change, check every rectangle against the window and its parent and every pair of siblings for overlap on every tab at 640x400 and 1920x1080, check `vg_sinf` against literal values, and check that every printable ASCII glyph lights pixels. `C99GUI_TEST_NEGATIVE=1` renders every tab at 300x200 and fails on the real bounds failures.
- **Suite GUI**: the window is resizable and maximizable on Win32, X11 and Cocoa, and all five workspaces reflow from the live size: the node graph lays its nodes out from the canvas size, the Data table and Color Picker scroll at small sizes, long labels are shortened, and "Reset 100% DPI" has its own row.
- **Suite GUI tests**: every widget and text string records its rectangle; scroll-viewport children are checked against the content area without the scrollbar. Tests click by name, assert each state change, check bounds and sibling overlap on all five tabs at 640x400 and 1920x1080, check `vg_sinf`/`vg_cosf` against literal values and every printable glyph. `C99GUI_TEST_NEGATIVE=1` renders every tab at 300x200, counts each bounds failure and exits by the failure count.
- **`--snapshot FILE --size WxH --tab N`** renders exactly one frame of the requested tab and size; sizes are clamped to the 640x400 (vector, suite) or 640x450 (pixel) minimum and the 3840x2160 framebuffer.
- **README**: build commands, CLI options and verification status now match the code.

### Fixed
- CI referenced a non-existent `src/gui.c`; it now builds the four real sources.
- Release notes were extracted empty because of a broken awk regex; extraction now uses a fixed-string match and refuses empty output.
- A failing MSVC `cl.exe` compile could pass the Windows release job; every compile's exit code is now checked.
- CI's push trigger only covered `main` and one work branch, so the merged pixel and vector window code had never been compiled for X11 or Cocoa. Running it exposed compile errors (an `NSSize` read as a rect, a test runner called with the wrong arity, a `Rect` type clashing with macOS `MacTypes.h`, signedness, a missing final newline), all fixed.
- `vg_sinf` (vector) returned sin(0) = 1 and sin(pi/2) = 0, which drew the knobs as hooks and the spline with a kink; the pixel GUI's `fast_sin` returned -cos. Both are now within 0.0011 of sin.
- Vector X11: resize events were never requested (`StructureNotifyMask`), and resizing would have freed the static framebuffer through `XDestroyImage`.
- No backend limited the window to the framebuffer, so a larger window overflowed it (the suite's buffer was only 1920x1080, so maximizing on any screen above 1080p overflowed); all three GUIs now use a 3840x2160 buffer and clamp on every path.
- Suite: X11 freed the static framebuffer through `XDestroyImage`, and its negative test exited 1 unconditionally, so a broken bounds checker looked the same as a working one.
- Removed documentation claims that the code did not support, such as a "13-stage regression suite", "[78%]" knob readouts and "ACCENT THEME" in the suite. The 2.0.0 and earlier entries below are kept as originally written and contain such claims.

### Verification limits
- Cocoa and X11 window behaviour (resize, maximize, input) is compile-checked in CI only (GCC and Clang on Linux, Clang on macOS); CI runners cannot open windows, so it has not been exercised on a real X11 or macOS desktop. Win32 resize and maximize were checked on Windows 11 (window style includes `WS_THICKFRAME` and `WS_MAXIMIZEBOX`; maximize grew the client area from 760x480 to 1536x793).

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
