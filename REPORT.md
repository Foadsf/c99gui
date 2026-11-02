# REPORT: src/suite/gui.c Fixes (wt-vector, branch fix/vector)

## Summary of Changes

### 1. Resizable/Maximizable Window Support
- **Win32**: Changed window style from `WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX` to `WS_OVERLAPPEDWINDOW` (includes `WS_THICKFRAME` and `WS_MAXIMIZEBOX`). Added `WM_SIZE` handler to update `ui.w`/`ui.h` and `WM_GETMINMAXINFO` to enforce minimum size (640x400).
- **X11**: Added `StructureNotifyMask` to event mask, handle `ConfigureNotify` to recreate `XImage` at new size.
- **Cocoa**: Changed style mask from `3` (Titled|Closable) to `15` (Titled|Closable|Miniaturizable|Resizable). Read content size from `contentView.frame` each frame.

### 2. Dynamic Framebuffer & Layout Reflow
- Replaced fixed `WIN_W`/`WIN_H` constants with runtime `ui.w`/`ui.h` throughout rendering.
- Framebuffer is now a static max-size buffer `px[MAX_BUF_W * MAX_BUF_H]` (1920×1080) with stride `ui.w`.
- All drawing functions (`vg_fill_rrect`, `vg_fill_gradient`, `vg_stroke_rrect`, `vg_shadow_rrect`, `vg_fill`, `vg_stroke`, `vg_text`, node grid) use `ui.w` for row indexing.
- Layout computes positions from `ui.w`/`ui.h` each frame; sidebar, main card, scroll viewport, knobs, sliders, buttons all reflow.

### 3. Responsive Knob & Column Layout (Tab 0)
- Knobs arrange in 2×2 grid when narrow (< 4×knob_width), 4×1 when wide.
- Sliders/buttons columns stack to 1 column when `ui.lw < 300×scale`.
- Sidebar text shortened from "Quick Presets (Right-Click Menu)" to "Quick Presets" to fit 210px sidebar.

### 4. Honest Test Framework
- **Rect Recording**: Every widget and text label records its rect (name, x, y, w, h, parent_id) during frame via `record_rect()`. Rects outside current clip region are not recorded (except in negative test mode).
- **Hit Testing**: Tests look up rects by name via `find_rect()`; no hard-coded coordinates.
- **State Assertions**: Each test records value before action and asserts change (toggle, checkbox, tab switch, text input, node drag, table selection, color picker, modal, context menu).
- **Resize Honesty Tests**: After main tests, renders Tab 0 at minimum (640×400) and maximum (1920×1080); asserts every recorded rect is inside window and inside its parent; no same-parent rects intersect.
- **Font Glyph Verification**: Renders each printable ASCII 33–126 at 24pt; asserts at least one pixel lit. All 94 glyphs pass. Missing glyphs ($ < > @ \ ^ ` { | } ~) added to `vg_glyph_path`.
- **Negative Test Mode** (`C99GUI_TEST_NEGATIVE=1`): Renders all 5 tabs at 300×200 (below minimum), runs same bounds checks, prints FAIL lines for every violation, exits 1 because failures > 0. Exits 1 with "bounds checking broken" if zero failures found.

### 5. Clean Snapshot CLI
- `--snapshot FILE` now renders exactly ONE clean initial frame at requested `--size WxH` and `--tab N` (default 980×640, tab 0). Does NOT run test suite first.
- BMP header carries actual width/height.
- `--help` first line starts with `Usage:`.
- `--version` prints `c99gui 2.1.0`.

### 6. Version Bump
- `APP_VERSION` updated from `2.0.0` to `2.1.0`.

---

## Verification Results

### GCC Headless Build (strict flags)
```text
=== GCC Headless Build ===
Exit: 0
```
Flags: `-std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -DC99GUI_HEADLESS`

### GCC Win32 Build (strict flags)
```text
=== GCC Win32 Build ===
Exit: 0
```
Flags: `-std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` + `-lgdi32 -luser32`

### Headless Test Suite (normal)
```text
Running C99 comprehensive vector GUI headless test suite...
ok   vector framebuffer rasterized (980x640, 2508800 bytes)
ok   vector text input & caret processing (txt_name='VectorStudio_v2')
ok   animated vector toggle state flipped
ok   vector checkbox state transition
ok   switched to Node Studio tab
ok   interactive node graph drag & bezier wire reroute
ok   switched to Data & Charts tab (filled vector area chart)
ok   interactive data table row selected (row=2)
ok   2D HSV color picker gradient & hex generator (accent=0xFF3F597F)
ok   switched to Scaling & Modals tab
ok   vector modal dialog open & esc-dismiss with toast
ok   right-click context menu open & click-outside dismiss
ok   resize honesty test at 640x400: 23 widgets recorded
ok   resize honesty test at 1920x1080: 27 widgets recorded
ok   all 94 printable ASCII glyphs (33-126) render at least one pixel
passed: 15  failed: 0
```

### Negative Test Mode (`C99GUI_TEST_NEGATIVE=1`)
```text
FAIL: tab 0 widget 'nav_tab' rect [25,202,188,38] outside window 300x200
... (35 total FAIL lines across all 5 tabs) ...
ok   negative test at 300x200 reported 35 bounds failures as expected
Exit: 1
```
Negative mode correctly detects bounds failures at below-minimum size and exits 1.

### Snapshot CLI
```text
$ ./build/tmp/s.exe --snapshot build/logs/shots/suite-min-tab0.bmp --size 640x400 --tab 0
ok   vector snapshot saved to build/logs/shots/suite-min-tab0.bmp (640x400, tab 0)
```
Snapshots generated for all 5 tabs at 3 sizes (640×400, 760×480, 1920×1080), converted to PNG via `magick`.

### Win32 Window Style & Maximize Verification
```text
Window Style: 0x14cf0000
WS_THICKFRAME (0x40000): True
WS_MAXIMIZEBOX (0x10000): True
WS_OVERLAPPEDWINDOW: True
Client Rect Before Maximize: 980 x 640
Client Rect After Maximize: 1536 x 793
PASS: Window has correct styles
```
Win32 window has `WS_THICKFRAME` (resizable) and `WS_MAXIMIZEBOX`; maximizes and client rect changes.

### Font Strip
Generated `build/logs/font-strip-suite.png` — all 94 printable ASCII glyphs (33–126) render visibly.

---

## Unverified / Known Limitations

- **X11 Build**: Not verified locally (WSL Ubuntu lacks `libx11-dev` and apt requires root). Code changes for `ConfigureNotify` and `XSetWMNormalHints` are present but untested.
- **Cocoa Build**: Cannot be built on this machine; style mask 15 and content-view size reading implemented per docs, but unverified.
- **MSVC Build**: `cl.exe` not available on this machine.
- **Knob Value Readout**: Knobs render as rotary arcs with indicator line; no numeric `[78%]` readout drawn (README mock showed readout). This is a visual fidelity gap but not a functional defect.
- **Node Studio at Minimum Size**: Node graph area is very small at 640×400; nodes may overlap. Resize honesty test passes because widgets are inside parent, but usability is limited.
- **Color Picker Tab at Minimum Size**: HSV gradient box (240px) + hue slider fits in 388px card width but is tight.

---

## Defects Found But Not Fixed

1. **Knob numeric readout missing** — would require additional text rendering below each knob.
2. **Node Studio usability at min size** — nodes crowd; could add horizontal scroll or auto-layout.
3. **X11/Cocoa untested** — requires Linux/macOS environment.

---

## Files Changed
- `src/suite/gui.c` — all fixes (single file, ~200 lines changed across ~20 targeted edits)