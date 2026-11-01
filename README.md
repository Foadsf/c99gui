# c99gui (Desktop Pixel GUI)

A monolithic, zero-allocation, immediate-mode **pixel-rendered desktop GUI in portable C99**.
Uses zero third-party libraries, no GPU shaders, and no `<math.h>` dependency.

Implements a **32-bit ARGB software rasterizer** in standard C99 with:
- Sub-pixel anti-aliased rounded rectangles using squared-distance fields (no sqrt)
- Alpha blending (`blend_argb`) with soft drop shadows
- Bit-packed vector-scaled 4x5 glyph font
- Immediate-mode widget library (panels, sidebar nav items, toggles, draggable sliders, buttons)
- Real-time animated waveform canvas (fast parabolic sine approximation)
- Native OS windowing backends:
  - **Windows**: Win32 GDI (`StretchDIBits`)
  - **Linux / BSD**: X11 (`XPutImage`) or Headless Software Framebuffer
  - **macOS**: Cocoa / CoreGraphics via the pure C runtime (`objc_msgSend`)
  - **Headless**: Full offscreen rasterizer with BMP snapshot export (`--snapshot`) and automated testing (`--test`)

```
 ┌────────────────────────────────────────────────────────────────────────┐
 │ [● C99 STUDIO]   │  AUDIO AND PIPELINE CONTROLS                        │
 │                  │                                                     │
 │ ▸ DASHBOARD      │  HARDWARE V-SYNC LOCK                        [● ON] │
 │   RENDERER       │  SUBPIXEL ANTI-ALIASING                      [● ON] │
 │   TELEMETRY      │  MASTER OUTPUT GAIN         ━━━━━━━───────   72%    │
 │                  │  MODULATION FREQUENCY       ━━━━──────────   45%    │
 │  ACCENT THEME    │  ┌────────────────────────────────────────────────┐ │
 │  (●)(●)(●)(●)    │  │ REALTIME SIGNAL WAVEFORM                       │ │
 │                  │  │       /\    /\    /\    /\    /\    /\         │ │
 │                  │  └────────────────────────────────────────────────┘ │
 └────────────────────────────────────────────────────────────────────────┘
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
./gui --help            # Print command-line options
./gui --version         # Print version (1.0.0)
./gui --test            # Run automated headless widget and rasterizer tests
./gui --snapshot out.bmp # Render one frame, export to 32-bit uncompressed BMP, and exit
./gui --headless        # Run 120 simulation frames headlessly without opening a window
```

## Architecture & Reusable Template

1. **State & Context (`GuiCtx ui`)**: Static 32-bit pixel buffer (`ui.px[760 * 480]`), mouse coordinates, button states, active and hot widget IDs. No heap allocation (`malloc`-free).
2. **Software Rasterizer**:
   - `draw_rrect`: Sub-pixel anti-aliased rounded rectangles. Uses squared distance `d2 = dx*dx + dy*dy` to compute fractional boundary coverage without floating-point `sqrt`.
   - `draw_card`: Multi-layer drop shadow and border compositor.
   - `glyph_bits` & `draw_text`: 4x5 compact bit-packed vector font.
3. **Immediate-Mode Widgets**:
   - `gui_begin_panel`: Structured container with header and separator.
   - `gui_nav_item`: Sidebar button with hover and active selection state.
   - `gui_toggle`: Animated pill switch.
   - `gui_slider`: Draggable progress/value track with thumb and percentage readout.
   - `gui_button`: Clickable button with instant click detection.

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
