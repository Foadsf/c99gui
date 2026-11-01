# Changelog

All notable changes to `c99gui` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-11-01 (branch: `vector-suite`)

### Added
- **Comprehensive Pure C99 2D Vector Graphics & Immediate-Mode GUI Framework** (`src/gui.c`):
  - Resolution expanded to 980x640 32-bit ARGB framebuffer ($2,508,800$ bytes) with zero heap allocations (`malloc`-free).
  - Hardware-style **Scissor Clipping Stack** (`vg_push_clip`, `vg_pop_clip`, `vg_reset_clip`) for nested viewports and inputs.
  - **Scanline Winding-Rule Polygon Fill** (`vg_fill`) with sub-pixel edge smoothing for filled graphs and geometry.
  - **Single-Pass Unified SDF Path Stroke** (`vg_stroke`) with clamped line projection distance fields.
  - **Bilinear 4-Corner Gradients** (`vg_fill_gradient`) with analytical SDF rounded box clipping.
  - **Multi-Column Grid Engine** (`ui_columns`, `ui_next_column`, `ui_end_columns`) for responsive layouts.
  - **Scrollable Viewports** (`ui_begin_scroll`, `ui_end_scroll`) with vertical wheel scrolling and draggable vector scrollbars.
  - **Per-Widget Lerp Animation Table** (`ui_anim_step`) providing smooth 60 FPS state transitions.
  - **Deferred Z-Order Overlay Pass**:
    - Dropdown popup menus (`ui_dropdown`)
    - Right-click context menus (`ui.ctx_open`)
    - Modal confirmation dialogs (`ui.modal_open`) with background dimming and input capture
    - Animated toast notification alerts (`ui_toast`)
    - Floating hover tooltips (`ui.tooltip_text`)
- **Suite of 5 Interactive Workspaces**:
  - **Controls Suite**: Single-line text input box (`ui_textbox` with blinking vector caret, typing, backspace, enter, esc), dropdown combo box, animated toggle switches, vector checkboxes, radio buttons, continuous sliders, rotary vector knobs (drag and scroll-wheel), and action buttons.
  - **Node Studio**: Interactive draggable node graph editor with 4 nodes, cubic Bézier patch cables, signal ports, and subtle background dot grid.
  - **Data & Charts**: Real-time filled vector area waveform (`vg_fill` + `vg_stroke`), telemetry, and selectable process data table with status badges.
  - **Color Picker**: Interactive 2D HSV Saturation/Value gradient box, continuous Hue slider, live Hex readout (`#RRGGBB`), and Daylight High-Contrast Light Mode / Dark Mode palette switcher.
  - **Scaling & Modals**: Resolution-independent live UI vector scaling (`0.75x` – `1.30x`), modal confirmation dialog, toast notification spawner, and multi-scale typography specimen.
- **Headless & Regression Testing**:
  - 13-stage automated regression test suite (`--test`).
  - 32-bit uncompressed BMP exporter (`--snapshot <file.bmp>`, 2,508,854 bytes exact).
  - Headless simulation mode (`--headless`).

## [1.0.0] - 2026-11-01 (branch: `vector-gui`)

### Added
- **Monolithic Canonical Pure C99 2D Vector Graphics & Immediate-Mode GUI Engine** (`src/gui.c`):
  - Resolution-independent SDF and Quadratic/Cubic Bézier vector rasterizer operating on an 860x540 32-bit ARGB framebuffer without heap allocation (`malloc`-free).
  - Fast compiler- and libm-agnostic float math (`vg_sqrtf`, `vg_sinf`, `vg_cosf`) requiring zero `<math.h>` or `-lm` dependencies.
  - Proportional SVG-style quadratic Bézier typography (`vg_glyph_path`, `vg_text`, `vg_number`) with sub-pixel anti-aliasing, uppercase, lowercase with true ascenders and descenders, numbers, and punctuation.
  - Unified multi-segment vector path rasterizer (`vg_begin_path`, `vg_move_to`, `vg_line_to`, `vg_quad_to`, `vg_cubic_to`, `vg_arc`, `vg_stroke`) with clamped line projection distance fields.
  - Analytical Euclidean Signed Distance Fields (SDF) for rounded rects (`sdf_rrect`) and multi-pass soft drop shadows (`ui_card`).
  - Resolution-independent dynamic UI Vector DPI Scaling (`0.75x` – `1.30x`) scaling all glyphs, Bézier curves, widgets, and radial gauges in real time.
- **Immediate-Mode Vector Widgets**:
  - `ui_card`: Analytical SDF rounded card with drop shadow and border compositing.
  - `ui_nav_item`: Sidebar navigation item with procedural vector icons (sliders, paths, gauge).
  - `ui_slider`: Draggable progress/value track with thumb and percentage readout.
  - `ui_toggle`: Animated pill toggle switch.
  - `ui_button`: Responsive push button with click detection.
  - `ui_radial_gauge`: Dynamic circular arc meter.
- **Application Workspaces & Controls**:
  - **Dashboard**: Live UI Vector Scale (DPI) slider, Subpixel Anti-Aliasing toggle, Radial Bézier Arcs toggle, Stroke Geometry Quality slider, interactive Radial Telemetry & Bézier Gauge, Benchmark action, and BMP snapshot trigger.
  - **Vector Paths**: Interactive SVG Bézier curve visualizer showing quadratic & cubic curve control points, and live path curvature controls.
  - **Telemetry**: Real-time rendering telemetry, dynamic arc gauge, and execution pulse trigger.
  - **Accent Themes**: Interactive swatches switching palette dynamically (Catppuccin Blue, Green, Purple, Peach).
- **Native OS Windowing Backends**:
  - **Windows**: Win32 GDI (`StretchDIBits` top-down DIB) with mouse tracking, `SetCapture`, and message pump.
  - **Linux / BSD**: Native X11 (`XPutImage`) with event handling.
  - **macOS**: Native Cocoa / CoreGraphics via C Objective-C runtime (`objc_msgSend`).
  - **Headless / CI**: Built-in 32-bit uncompressed BMP exporter (`--snapshot <file.bmp>`, 1,857,654 bytes) and automated regression test suite (`--test`).
- **Universal Multi-Compiler Support**:
  - Warning-free compilation under MSVC (`/W4 /WX`), GCC (`-Wall -Wextra -Wpedantic -Werror`), and Clang.
  - Verified across Visual Studio 2026 Community (x64, x86), Visual Studio 2019 BuildTools (x64, x86), MSYS2 UCRT64 GCC, Cygwin GCC & Clang, and WSL Ubuntu GCC.

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
