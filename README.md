# c99gui (Comprehensive 2D Vector Graphics & Immediate-Mode GUI Framework)

A monolithic, zero-allocation, resolution-independent **2D vector graphics immediate-mode desktop GUI framework in portable C99**.
Uses zero third-party libraries, no GPU shaders, zero heap allocations (`malloc`-free), and zero `<math.h>` / `-lm` dependencies (self-contained IEEE-754 fast float math).

Implements an advanced **32-bit ARGB SDF and Bézier vector rasterizer** in standard C99 (`WIN_W 980`, `WIN_H 640`) with:
- **Vector Graphics & Rasterizer Upgrades**:
  - **Hardware-Style Scissor Clipping Stack** (`vg_push_clip` / `vg_pop_clip` / `vg_reset_clip`): Enables nested scrollable viewports, bounded text inputs, and clipped node canvases.
  - **Scanline Polygon Fill** (`vg_fill`) + **Unified SDF Stroke** (`vg_stroke`): Renders filled concave/convex vector polygons (area charts, badges, checkmarks) and sub-pixel anti-aliased Bézier strokes.
  - **Bilinear 4-Corner Gradients** (`vg_fill_gradient`): Powers interactive 2D color pickers, illuminated cards, and styled elements.
  - **Proportional ASCII Vector Typography**: SVG-style quadratic Bézier outlines (`M`, `L`, `Q`) with sub-pixel anti-aliasing, uppercase, lowercase with true ascenders and descenders, numerals, and punctuation. Zero bitmap font tables.
- **Layout & Container Architecture**:
  - **Scrollable Viewports** (`ui_begin_scroll` / `ui_end_scroll`): Vertical scrolling via mouse wheel and draggable vector scrollbars.
  - **Responsive Multi-Column Grid** (`ui_columns` / `ui_next_column` / `ui_end_columns`): Automatic horizontal splitting.
  - **Per-Widget Lerp Animation Table** (`ui_anim_step`): 60 FPS smooth transitions for toggle switches, checkboxes, radios, and hover highlights.
  - **Deferred Z-Order Overlay Pass**: Renders dropdown popups, right-click context menus, modal confirmation dialogs, toast notifications, and hover tooltips on top of the scene.
- **Complete Suite of 5 Interactive Workspaces**:
  - **Tab 1 — Controls Suite**: Single-line text input box (`ui_textbox` with blinking vector caret, typing, and backspace), dropdown combo box, animated toggles, checkboxes, radio buttons, continuous sliders, rotary vector knobs (drag and scroll-wheel), and action buttons.
  - **Tab 2 — Node Studio**: Interactive draggable node graph editor with 4 nodes connected by real-time cubic Bézier patch cables and subtle background dot grid.
  - **Tab 3 — Data & Charts**: Real-time filled vector area waveform (`vg_fill` + `vg_stroke`), telemetry, and selectable process data table with status badges.
  - **Tab 4 — Color Picker**: Interactive 2D HSV Saturation/Value gradient box, continuous Hue slider, live Hex readout (`#RRGGBB`), and Daylight High-Contrast Light Mode / Dark Mode palette switcher.
  - **Tab 5 — Scaling & Modals**: Resolution-independent live UI vector scaling (`0.75x` – `1.30x`), modal confirmation dialog with dimmed backdrop and input capture, toast notification trigger, and multi-scale typography specimen.
- **Native OS Windowing Backends**:
  - **Windows**: Win32 GDI (`StretchDIBits`) with mouse tracking, mouse-wheel, keyboard input, and message pump.
  - **Linux / BSD**: Native X11 (`XPutImage`) with event handling, or Headless fallback.
  - **macOS**: Native Cocoa / CoreGraphics via C Objective-C runtime (`objc_msgSend`).
  - **Headless / CI**: Built-in 32-bit uncompressed BMP exporter (`--snapshot <file.bmp>`, 2,508,854 bytes) and automated regression test suite (`--test`).

```
 ┌────────────────────────────────────────────────────────────────────────────────────────┐
 │ [● VECTOR SUITE]   │  1. CONTROLS SUITE  │  TEXT INPUT & DROPDOWN SELECTOR             │
 │                    │                     │  Project Name: [ VectorStudio_v2 | ]        │
 │  ▸ 1. CONTROLS     │                     │  Audio Routing: [ Low-Latency ASIO  ▾ ]     │
 │    2. NODE STUDIO  │  MULTI-COLUMN       │                                             │
 │    3. DATA & CHARTS│  SWITCHES & CHECKS  │  [●] Sub-Pixel SDF AA   [●] V-Sync Lock     │
 │    4. COLOR PICKER │                     │  [✔] Enable SIMD        (●) Real-Time Mode  │
 │    5. SCALING/MODAL│  ROTARY KNOBS       │                                             │
 │                    │  & SLIDERS          │    (Gain)    (Cutoff)   (Reson)   (Drive)   │
 │   ACCENT THEME     │                     │   [ 78% ]    [ 62Hz ]   [ 45% ]   [ 34% ]   │
 │   (●)(●)(●)(●)     │                     │  Master Gain    ━━━━━━━───────   78%        │
 │                    │                     │  [ SAVE PRESET ]     [ OPEN MODAL DIALOG ]  │
 ├────────────────────┴─────────────────────┴─────────────────────────────────────────────┤
 │ Status: Pure C99 SDF + Bezier Vector Rasterizer  |  Right-Click Anywhere for Menu      │
 └────────────────────────────────────────────────────────────────────────────────────────┘
```

## Quick Build

Pick the command for your compiler; the program is a single file, `src/gui.c`:

| Toolchain | Command |
|---|---|
| MSVC (Windows) | `build.cmd` or `cl /std:c11 /W4 /WX /O2 /MT /utf-8 src/gui.c` |
| MinGW / MSYS2 GCC | `gcc -std=c99 -O2 -Wall -Wextra -Wpedantic src/gui.c -lgdi32 -luser32 -o gui.exe` |
| Cygwin GCC / Clang | `gcc -std=c99 -O2 -Wall -Wextra -Wpedantic src/gui.c -lgdi32 -luser32 -o gui.exe` |
| Linux / BSD (X11) | `cc -std=c99 -O2 -Wall -Wextra -Wpedantic src/gui.c -lX11 -o gui` |
| macOS (Cocoa) | `cc -std=c99 -O2 -Wall -Wextra -Wpedantic src/gui.c -framework Cocoa -framework CoreGraphics -o gui` |
| Headless / Test | `cc -std=c99 -O2 -Wall -Wextra -Wpedantic -DC99GUI_HEADLESS src/gui.c -o gui` |
| CMake | `cmake -B build && cmake --build build` |
| Make | `make` (or `make headless`, `make test`, `make check`) |

## Command-Line Options

```sh
./gui --help             # Print command-line options
./gui --version          # Print version (2.0.0)
./gui --test             # Run 13-stage automated headless vector regression test suite
./gui --snapshot out.bmp # Render one frame, export to 32-bit BMP (2,508,854 bytes), and exit
./gui --headless         # Run 120 simulation frames headlessly without opening a window
```

## Verification

Run the test suite against any binary:
```sh
sh tests/smoke.sh build/gui.exe
```

Run the Windows multi-compiler test matrix:
```powershell
pwsh scripts/build-matrix.ps1
```

## License

MIT License. See [LICENSE](LICENSE).
