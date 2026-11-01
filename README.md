# c99gui (2D Vector Graphics & Immediate-Mode GUI Engine)

A monolithic, zero-allocation, resolution-independent **2D vector graphics immediate-mode desktop GUI in portable C99**.
Uses zero third-party libraries, no GPU shaders, and zero `<math.h>` / `-lm` dependencies (self-contained IEEE-754 fast float math).

Implements a **32-bit ARGB SDF and Bézier vector rasterizer** in standard C99 (`WIN_W 860`, `WIN_H 540`) with:
- **Proportional Vector Typography**: SVG-path quadratic Bézier curves (`M`, `L`, `Q`) with sub-pixel anti-aliasing, true ascenders, descenders, numerals, and punctuation. Zero bitmap font tables.
- **Unified Vector Path Engine**: `vg_begin_path()`, `vg_move_to()`, `vg_line_to()`, `vg_quad_to()`, `vg_cubic_to()`, `vg_arc()`, and `vg_stroke()` with arbitrary stroke widths and anti-aliased edge coverage.
- **Analytical Signed Distance Fields (SDF)**: Euclidean distance fields for crisp rounded boxes, card surfaces, and smooth multi-pass drop shadows.
- **Real-Time DPI Vector Scaling**: Live UI vector scale slider (`0.75x` – `1.30x`) scaling every glyph, Bézier curve, vector icon, and radial gauge in real time.
- **Immediate-Mode Vector Widgets**: Sidebar navigation items with procedural vector icons, animated toggle switches, interactive draggable sliders, accent theme pickers, radial arc gauges, and buttons.
- **Native OS Windowing Backends**:
  - **Windows**: Win32 GDI (`StretchDIBits`)
  - **Linux / BSD**: X11 (`XPutImage`) or Headless Software Framebuffer
  - **macOS**: Cocoa / CoreGraphics via the pure C runtime (`objc_msgSend`)
  - **Headless**: Full offscreen rasterizer with 32-bit uncompressed BMP snapshot export (`--snapshot`) and automated regression testing (`--test`)

```
 ┌──────────────────────────────────────────────────────────────────────────────┐
 │ [● VECTOR STUDIO] │  VECTOR GRAPHICS & ENGINE CONTROLS                       │
 │                   │                                                          │
 │  ▸ DASHBOARD      │  UI VECTOR SCALE (DPI)      ━━━━━━━──────   1.00x        │
 │    VECTOR PATHS   │  SUBPIXEL ANTI-ALIASING     [● ON]                       │
 │    TELEMETRY      │  RADIAL BEZIER ARCS         [● ON]                       │
 │                   │  STROKE GEOMETRY QUALITY    ━━━━━━━━─────   85%          │
 │   ACCENT THEME    │  ┌─────────────────────────────────────────────────────┐ │
 │   (●)(●)(●)(●)    │  │ RADIAL TELEMETRY & BEZIER GAUGES                    │ │
 │                   │  │       ╭─────╮                                       │ │
 │                   │  │      (  78%  )    Active Vector Path Subdivisions   │ │
 │                   │  │       ╰─────╯                                       │ │
 │                   │  │  [ RUN BENCHMARK ]    [ EXPORT FRAMEBUFFER (BMP) ]   │ │
 │                   │  └─────────────────────────────────────────────────────┘ │
 └──────────────────────────────────────────────────────────────────────────────┘
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
./gui --version          # Print version (1.0.0)
./gui --test             # Run automated headless widget and vector rasterizer regression tests
./gui --snapshot out.bmp  # Render one frame, export to 32-bit uncompressed BMP (1,857,654 bytes), and exit
./gui --headless         # Run 120 simulation frames headlessly without opening a window
```

## Architecture & Reusable Template

1. **State & Context (`GuiCtx ui`)**: Static 32-bit pixel buffer (`ui.px[860 * 540]`), mouse coordinates, button states, active and hot widget IDs. No heap allocation (`malloc`-free).
2. **Self-Contained Float Math**:
   - `vg_sqrtf`: Fast reciprocal square-root approximation with 2 Newton-Raphson iterations.
   - `vg_sinf` & `vg_cosf`: High-precision parabolic trigonometric approximation.
   - Zero dependence on libm (`-lm`).
3. **Vector Path Rasterizer**:
   - Segment accumulator (`VgSeg segs[128]`) with clamped line projection distance fields.
   - `vg_quad_to` and `vg_cubic_to` decomposed into adaptive linear sub-segments.
   - `vg_arc` sweeps smooth circular and elliptical curves.
   - Sub-pixel coverage blending with 32-bit ARGB destination.
4. **Immediate-Mode Vector Widgets**:
   - `ui_card`: Analytical SDF rounded card with smooth drop shadows.
   - `ui_nav_item`: Sidebar navigation with procedural vector icons (sliders, paths, gauge).
   - `ui_slider`: Draggable progress/value track with thumb and percentage readout.
   - `ui_toggle`: Animated pill toggle switch.
   - `ui_button`: Responsive push button with click detection.
   - `ui_radial_gauge`: Dynamic circular arc meter.

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
