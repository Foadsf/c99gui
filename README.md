# c99gui — Four Pure C99 GUI Programs

A collection of four single-file, zero-allocation GUI programs in portable C99. Each program is self-contained with zero third-party library dependencies.

## The Four Programs

### 1. c99gui-tui (Terminal UI)

**Source:** `src/tui/c99gui.c`  
**Dependencies:** None (standard C library only)  
**Status:** ✅ CI verified on all platforms

A zero-allocation, immediate-mode terminal GUI in C99. Uses a software framebuffer of UTF-8 cells with 24-bit color. All state lives in static storage — no heap allocations.

**Build:**
```sh
cc -std=c99 -O2 -Wall -Wextra -pedantic src/tui/c99gui.c -o c99gui-tui
cl /std:c11 /W4 /O2 /MT /utf-8 src\tui\c99gui.c /Fe:c99gui-tui.exe
```

**CLI:** `--help`, `--version`, `--snapshot FILE` (24-row text frame), `--test`

---

### 2. c99gui-pixel (Pixel Desktop GUI)

**Source:** `src/pixel/gui.c`  
**Dependencies:** Win32 GDI (Windows), X11 (Linux), Cocoa/CoreGraphics (macOS), or headless  
**Status:** ✅ CI verified on all platforms

A monolithic pixel-rendered desktop GUI with a 32-bit ARGB software rasterizer, sub-pixel anti-aliased rounded rectangles, alpha blending, soft drop shadows, and bit-packed vector-scaled font.

**Build:**
```sh
# Windows (MinGW/MSYS2)
gcc -std=c99 -O2 -Wall -Wextra -pedantic src/pixel/gui.c -lgdi32 -luser32 -o c99gui-pixel.exe

# Linux (X11)
cc -std=c99 -O2 -Wall -Wextra -pedantic src/pixel/gui.c -lX11 -o c99gui-pixel

# macOS
cc -std=c99 -O2 -Wall -Wextra -pedantic src/pixel/gui.c -framework Cocoa -framework CoreGraphics -o c99gui-pixel

# Headless (testing)
cc -std=c99 -O2 -DC99GUI_HEADLESS src/pixel/gui.c -o c99gui-pixel
```

**CLI:** `--help`, `--version`, `--test`, `--snapshot FILE.bmp`, `--headless`

---

### 3. c99gui-vector (Vector Desktop GUI)

**Source:** `src/vector/gui.c`  
**Dependencies:** Win32 GDI (Windows), X11 (Linux), Cocoa/CoreGraphics (macOS), or headless  
**Status:** ✅ CI verified on all platforms

A resolution-independent SDF and Bézier vector graphics engine with SVG-style quadratic Bézier typography, sub-pixel anti-aliasing, and real-time DPI scaling (0.75x – 1.30x).

**Build:**
```sh
# Windows (MinGW/MSYS2)
gcc -std=c99 -O2 -Wall -Wextra -pedantic src/vector/gui.c -lgdi32 -luser32 -o c99gui-vector.exe

# Linux (X11)
cc -std=c99 -O2 -Wall -Wextra -pedantic src/vector/gui.c -lX11 -o c99gui-vector

# macOS
cc -std=c99 -O2 -Wall -Wextra -pedantic src/vector/gui.c -framework Cocoa -framework CoreGraphics -o c99gui-vector

# Headless (testing)
cc -std=c99 -O2 -DC99GUI_HEADLESS src/vector/gui.c -o c99gui-vector
```

**CLI:** `--help`, `--version`, `--test`, `--snapshot FILE.bmp`, `--headless`

---

### 4. c99gui-suite (Comprehensive Vector GUI Suite)

**Source:** `src/suite/gui.c`  
**Dependencies:** Win32 GDI (Windows), X11 (Linux), Cocoa/CoreGraphics (macOS), or headless  
**Status:** ✅ CI verified on all platforms

A comprehensive vector GUI framework with five interactive workspaces: Controls Suite, Node Studio, Data & Charts, Color Picker, and Scaling & Modals. Features scrollable containers, multi-column layouts, animated widgets, dropdown menus, modal dialogs, and toast notifications.

**Build:**
```sh
# Windows (MinGW/MSYS2)
gcc -std=c99 -O2 -Wall -Wextra -pedantic src/suite/gui.c -lgdi32 -luser32 -o c99gui-suite.exe

# Linux (X11)
cc -std=c99 -O2 -Wall -Wextra -pedantic src/suite/gui.c -lX11 -o c99gui-suite

# macOS
cc -std=c99 -O2 -Wall -Wextra -pedantic src/suite/gui.c -framework Cocoa -framework CoreGraphics -o c99gui-suite

# Headless (testing)
cc -std=c99 -O2 -DC99GUI_HEADLESS src/suite/gui.c -o c99gui-suite
```

**CLI:** `--help`, `--version`, `--test`, `--snapshot FILE.bmp`, `--headless`, `--size WxH`, `--tab N`

---

## Quick Build Reference

| Toolchain | Command |
|---|---|
| **Make** | `make` (builds all four), `make test`, `make headless` |
| **CMake** | `cmake -B build && cmake --build build` |
| **Windows (MSVC)** | `build.cmd` or `build.cmd headless` |
| **Windows (MSYS2)** | `pwsh scripts/build-matrix.ps1` |

## Command-Line Options (GUI Programs)

```sh
./c99gui-pixel --help             # Print command-line options
./c99gui-pixel --version          # Print version
./c99gui-pixel --test             # Run automated headless regression test suite
./c99gui-pixel --snapshot out.bmp # Render one frame, export to 32-bit BMP, exit
./c99gui-pixel --headless         # Run simulation frames without opening a window
```

## Command-Line Options (TUI)

```sh
./c99gui-tui --help               # Print command-line options
./c99gui-tui --version            # Print version
./c99gui-tui --snapshot           # Export 24-row text frame to stdout
```

## Verification

Run the smoke tests against built binaries:
```sh
sh tests/tui_smoke.sh build/c99gui-tui
sh tests/smoke.sh build/c99gui-pixel
sh tests/smoke.sh build/c99gui-vector
sh tests/smoke.sh build/c99gui-suite
```

## CI Status

GitHub Actions verifies all four programs on:
- **Linux** (Ubuntu): GCC and Clang, with X11 and headless modes
- **macOS**: Clang, with Cocoa and headless modes
- **Windows**: MSVC (x64) and MSYS2 UCRT64 GCC, default and headless modes

All builds use strict warnings: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` (GCC/Clang) or `/W4 /WX` (MSVC).

## License

MIT License. See [LICENSE](LICENSE).
