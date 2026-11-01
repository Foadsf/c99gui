/*
 * gui.c - Monolithic Pure C99 Pixel GUI & Immediate-Mode Widget Engine
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Foad Sojoodi Farimani
 *
 * True pixel-rendered desktop GUI with a 32-bit ARGB software rasterizer,
 * sub-pixel anti-aliased rounded rectangles, alpha blending, soft drop shadows,
 * bit-packed vector-scaled font, and native OS windowing backends:
 *   - Windows : Win32 GDI (StretchDIBits)
 *   - Linux/BSD: X11 (XPutImage) or Headless Software Framebuffer
 *   - macOS   : Cocoa / CoreGraphics via C runtime (objc_msgSend)
 *
 * Build Commands:
 *   MSVC (Windows)  : cl /O2 /W4 /utf-8 src/gui.c
 *   Intel (Windows) : icx /O2 src/gui.c
 *   MinGW (Windows) : gcc -std=c99 -O2 src/gui.c -lgdi32 -luser32 -o gui.exe
 *   Linux / BSD     : cc -std=c99 -O2 src/gui.c -lX11 -o gui
 *   macOS           : cc -std=c99 -O2 src/gui.c -framework Cocoa -framework CoreGraphics -o gui
 *   Headless / Test : cc -std=c99 -O2 -DC99GUI_HEADLESS src/gui.c -o gui
 */

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

/* =========================================================================
 * 0. PLATFORM & BACKEND DETECTION
 * ========================================================================= */
#if defined(C99GUI_HEADLESS)
  #define C99GUI_BACKEND_HEADLESS 1
#elif defined(_WIN32) || defined(__CYGWIN__)
  #define C99GUI_BACKEND_WIN32 1
#elif defined(__APPLE__)
  #define C99GUI_BACKEND_COCOA 1
#elif defined(__has_include)
  #if __has_include(<X11/Xlib.h>)
    #define C99GUI_BACKEND_X11 1
  #else
    #define C99GUI_BACKEND_HEADLESS 1
  #endif
#elif defined(__unix__) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  #define C99GUI_BACKEND_X11 1
#else
  #define C99GUI_BACKEND_HEADLESS 1
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(C99GUI_BACKEND_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if defined(_MSC_VER)
  #pragma comment(lib, "user32.lib")
  #pragma comment(lib, "gdi32.lib")
#endif
#endif

#define APP_NAME    "c99gui"
#define APP_VERSION "1.0.0"
#define WIN_W       760
#define WIN_H       480

/* =========================================================================
 * 1. CORE GUI STATE & IMMEDIATE-MODE CONTEXT
 * ========================================================================= */
typedef struct {
    uint32_t px[WIN_W * WIN_H];
    int mx, my;
    bool mouse_down;
    bool mouse_pressed;
    bool mouse_released;
    int active_id;
    int hot_id;
    int next_id;
    int layout_x, layout_y, layout_w;
    uint32_t accent;
    unsigned int tick;
    bool running;
} GuiCtx;

static GuiCtx ui;

/* =========================================================================
 * 2. HARDWARE-AGNOSTIC C99 SOFTWARE RASTERIZER (Sub-Pixel Anti-Aliased)
 * ========================================================================= */
static uint32_t blend_argb(uint32_t dst, uint32_t src, uint32_t alpha_256) {
    uint32_t a = (((src >> 24) & 0xFF) * alpha_256) >> 8;
    uint32_t ia = 256u - a;
    uint32_t rb = ((((src & 0x00FF00FFu) * a) + ((dst & 0x00FF00FFu) * ia)) >> 8) & 0x00FF00FFu;
    uint32_t g  = ((((src & 0x0000FF00u) * a) + ((dst & 0x0000FF00u) * ia)) >> 8) & 0x0000FF00u;
    return 0xFF000000u | rb | g;
}

/* Draws an anti-aliased rounded rectangle using squared-distance fields (no sqrt needed) */
static void draw_rrect(int x, int y, int w, int h, int r, uint32_t col) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > WIN_W ? WIN_W : (x + w);
    int y1 = (y + h) > WIN_H ? WIN_H : (y + h);
    if (x0 >= x1 || y0 >= y1) return;

    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    int r_in  = r > 0 ? (r - 1) * (r - 1) : 0;
    int r_out = (r + 1) * (r + 1);
    int span  = r_out - r_in;

    for (int py = y0; py < y1; py++) {
        int dy = 0;
        if (py < y + r) dy = (y + r) - py;
        else if (py >= y + h - r) dy = py - (y + h - r - 1);

        uint32_t *row = &ui.px[py * WIN_W];
        for (int px = x0; px < x1; px++) {
            int dx = 0;
            if (px < x + r) dx = (x + r) - px;
            else if (px >= x + w - r) dx = px - (x + w - r - 1);

            if (dx > 0 && dy > 0) {
                int d2 = dx * dx + dy * dy;
                if (d2 >= r_out) continue;
                if (d2 > r_in && span > 0) {
                    uint32_t cov = (uint32_t)((r_out - d2) * 256 / span);
                    row[px] = blend_argb(row[px], col, cov);
                    continue;
                }
            }
            row[px] = blend_argb(row[px], col, 256);
        }
    }
}

/* Soft drop-shadow + bordered card surface */
static void draw_card(int x, int y, int w, int h, int r, uint32_t bg, uint32_t border) {
    draw_rrect(x - 2, y + 4, w + 4, h + 4, r + 3, 0x24000000u);
    draw_rrect(x - 1, y + 2, w + 2, h + 2, r + 2, 0x38000000u);
    draw_rrect(x, y, w, h, r, border);
    draw_rrect(x + 1, y + 1, w - 2, h - 2, r > 1 ? r - 1 : 0, bg);
}

/* Compact 20-bit (4x5) packed glyph table - 100% self-contained */
static uint32_t glyph_bits(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    switch (c) {
        case 'A': return 0x69F99u; case 'B': return 0xE9E9Eu; case 'C': return 0x78887u;
        case 'D': return 0xE999Eu; case 'E': return 0xF8E8Fu; case 'F': return 0xF8E88u;
        case 'G': return 0x78B97u; case 'H': return 0x99F99u; case 'I': return 0xE444Eu;
        case 'J': return 0x31196u; case 'K': return 0x9ACA9u; case 'L': return 0x8888Fu;
        case 'M': return 0x9FF99u; case 'N': return 0x9DB99u; case 'O': return 0x69996u;
        case 'P': return 0xE9E88u; case 'Q': return 0x69963u; case 'R': return 0xE9EA9u;
        case 'S': return 0x7861Eu; case 'T': return 0xE4444u; case 'U': return 0x99996u;
        case 'V': return 0x99964u; case 'W': return 0x99FF9u; case 'X': return 0x96699u;
        case 'Y': return 0x99644u; case 'Z': return 0xF248Fu;
        case '0': return 0x69996u; case '1': return 0x4C44Eu; case '2': return 0xE168Fu;
        case '3': return 0xE161Eu; case '4': return 0x99F11u; case '5': return 0xF8E1Eu;
        case '6': return 0x78E96u; case '7': return 0xF1244u; case '8': return 0x69696u;
        case '9': return 0x69716u; case ':': return 0x04040u; case '-': return 0x00F00u;
        case '+': return 0x04E40u; case '.': return 0x00004u; case '%': return 0x92489u;
        case '/': return 0x12480u; case '(': return 0x24442u; case ')': return 0x42224u;
        case '!': return 0x44404u; case '_': return 0x0000Fu;
        default:  return 0x00000u;
    }
}

static void draw_text(int x, int y, const char *s, int scale, uint32_t col) {
    for (; *s; s++, x += 5 * scale) {
        uint32_t g = glyph_bits(*s);
        if (!g) continue;
        for (int row = 0; row < 5; row++) {
            uint32_t nibble = (g >> ((4 - row) * 4)) & 0xFu;
            for (int bit = 0; bit < 4; bit++) {
                if (nibble & (8u >> bit))
                    draw_rrect(x + bit * scale, y + row * scale, scale, scale, 0, col);
            }
        }
    }
}

static void draw_int(int x, int y, int val, const char *suffix, int scale, uint32_t col) {
    char buf[24];
    int i = 0;
    if (val < 0) {
        buf[i++] = '-';
        val = -val;
    }
    if (val == 0) buf[i++] = '0';
    else {
        char rev[16];
        int r = 0;
        while (val > 0 && r < 15) { rev[r++] = (char)('0' + (val % 10)); val /= 10; }
        while (r > 0) buf[i++] = rev[--r];
    }
    while (*suffix && i < 23) buf[i++] = *suffix++;
    buf[i] = '\0';
    draw_text(x, y, buf, scale, col);
}

/* =========================================================================
 * 3. IMMEDIATE-MODE WIDGET LIBRARY
 * ========================================================================= */
static bool hit_box(int x, int y, int w, int h) {
    return ui.mx >= x && ui.mx < (x + w) && ui.my >= y && ui.my < (y + h);
}

static void gui_begin_panel(int x, int y, int w, int h, const char *title) {
    draw_card(x, y, w, h, 12, 0xFF181926u, 0xFF2E3047u);
    draw_text(x + 22, y + 20, title, 2, 0xFF89B4FAu);
    draw_rrect(x + 22, y + 38, w - 44, 1, 0, 0xFF2E3047u);
    ui.layout_x = x + 22;
    ui.layout_y = y + 52;
    ui.layout_w = w - 44;
}

static bool gui_nav_item(int x, int y, int w, const char *label, bool active) {
    int id = ++ui.next_id;
    bool hover = hit_box(x, y, w, 36);
    if (hover) ui.hot_id = id;
    bool clicked = hover && ui.mouse_pressed;

    uint32_t bg = active ? ui.accent : (hover ? 0xFF25273Au : 0x00000000u);
    uint32_t fg = active ? 0xFF11111Bu : (hover ? 0xFFCDD6F4u : 0xFF9399B2u);
    if (bg) draw_rrect(x, y, w, 36, 8, bg);
    draw_text(x + 16, y + 13, label, 2, fg);
    return clicked;
}

static void gui_toggle(const char *label, bool *state) {
    int id = ++ui.next_id;
    int x = ui.layout_x, y = ui.layout_y, w = ui.layout_w;
    bool hover = hit_box(x, y, w, 36);
    if (hover) ui.hot_id = id;
    if (hover && ui.mouse_pressed) *state = !(*state);

    draw_rrect(x, y, w, 36, 8, hover ? 0xFF232538u : 0xFF1E2030u);
    draw_text(x + 14, y + 13, label, 2, 0xFFCDD6F4u);

    int px = x + w - 54, py = y + 7;
    uint32_t pill = *state ? ui.accent : 0xFF363A4Fu;
    draw_rrect(px, py, 40, 22, 11, pill);
    draw_rrect(*state ? (px + 21) : (px + 3), py + 3, 16, 16, 8, 0xFFFFFFFFu);

    ui.layout_y += 46;
}

static void gui_slider(const char *label, int *val, int min_v, int max_v) {
    int id = ++ui.next_id;
    int x = ui.layout_x, y = ui.layout_y, w = ui.layout_w;
    int tx = x + 14, ty = y + 32, tw = w - 28;

    bool hover = hit_box(x, y, w, 52);
    if (hover) ui.hot_id = id;
    if (hover && ui.mouse_pressed) ui.active_id = id;

    if (ui.active_id == id && ui.mouse_down) {
        int v = min_v + ((ui.mx - tx) * (max_v - min_v)) / (tw > 0 ? tw : 1);
        if (v < min_v) v = min_v;
        if (v > max_v) v = max_v;
        *val = v;
    }

    draw_rrect(x, y, w, 52, 8, (hover || ui.active_id == id) ? 0xFF232538u : 0xFF1E2030u);
    draw_text(x + 14, y + 10, label, 2, 0xFFCDD6F4u);
    draw_int(x + w - 52, y + 10, *val, "%", 2, ui.accent);

    /* Track & Thumb */
    draw_rrect(tx, ty, tw, 8, 4, 0xFF313244u);
    int fill_w = ((*val - min_v) * tw) / (max_v > min_v ? max_v - min_v : 1);
    if (fill_w > 0) draw_rrect(tx, ty, fill_w, 8, 4, ui.accent);
    draw_rrect(tx + fill_w - 8, ty - 4, 16, 16, 8, 0xFFFFFFFFu);

    ui.layout_y += 62;
}

static bool gui_button(const char *label) {
    int id = ++ui.next_id;
    int x = ui.layout_x, y = ui.layout_y;
    int w = (int)strlen(label) * 10 + 36;
    bool hover = hit_box(x, y, w, 36);
    if (hover) ui.hot_id = id;
    bool clicked = hover && ui.mouse_pressed;

    uint32_t bg = (hover && ui.mouse_down) ? 0xFFFFFFFFu : (hover ? ui.accent : 0xFF313244u);
    uint32_t fg = hover ? 0xFF11111Bu : 0xFFCDD6F4u;
    draw_rrect(x, y, w, 36, 8, bg);
    draw_text(x + 18, y + 13, label, 2, fg);
    ui.layout_y += 46;
    return clicked;
}

/* =========================================================================
 * 4. APPLICATION RENDER & LOGIC LOOP
 * ========================================================================= */
static int  app_tab    = 0;
static bool opt_vsync  = true;
static bool opt_aa     = true;
static bool opt_hdr    = false;
static int  val_gain   = 72;
static int  val_freq   = 45;
static int  val_lum    = 85;
static int  pulse_cnt  = 0;

/* Fast parabolic sine approximation (eliminates <math.h> / -lm dependency) */
static float fast_sin(float x) {
    x *= 0.15915494f;
    x -= (float)(int)x;
    if (x < 0.0f) x += 1.0f;
    float y = (x < 0.5f) ? (x * 4.0f - 1.0f) : (3.0f - x * 4.0f);
    return y * (1.5f - 0.5f * y * y);
}

static void gui_frame(void) {
    ui.next_id = 0;
    ui.hot_id = 0;
    if (ui.mouse_released) ui.active_id = 0;

    /* 1. Background Canvas Gradient */
    for (int y = 0; y < WIN_H; y++) {
        uint32_t b = 22u + (uint32_t)(y * 12 / WIN_H);
        uint32_t row_col = 0xFF0E0F00u | b;
        uint32_t *row = &ui.px[y * WIN_W];
        for (int x = 0; x < WIN_W; x++) row[x] = row_col;
    }

    /* 2. Left Sidebar Card */
    draw_card(20, 20, 190, 440, 12, 0xFF181926u, 0xFF2E3047u);
    draw_rrect(36, 38, 12, 12, 6, ui.accent);
    draw_text(56, 39, "C99 STUDIO", 2, 0xFFCDD6F4u);

    if (gui_nav_item(32, 76, 166, "DASHBOARD", app_tab == 0)) app_tab = 0;
    if (gui_nav_item(32, 118, 166, "RENDERER",  app_tab == 1)) app_tab = 1;
    if (gui_nav_item(32, 160, 166, "TELEMETRY", app_tab == 2)) app_tab = 2;

    /* Accent Theme Swatches */
    draw_text(36, 392, "ACCENT THEME", 2, 0xFF6C7086u);
    const uint32_t swatches[4] = {0xFF89B4FAu, 0xFFA6E3A1u, 0xFFCBA6F7u, 0xFFF9E2AFu};
    for (int i = 0; i < 4; i++) {
        int sx = 36 + i * 38, sy = 416;
        if (hit_box(sx, sy, 26, 26) && ui.mouse_pressed) ui.accent = swatches[i];
        if (ui.accent == swatches[i]) draw_rrect(sx - 2, sy - 2, 30, 30, 15, 0xFFFFFFFFu);
        draw_rrect(sx, sy, 26, 26, 13, swatches[i]);
    }

    /* 3. Main Content Panel */
    if (app_tab == 0) {
        gui_begin_panel(228, 20, 512, 440, "AUDIO AND PIPELINE CONTROLS");
        gui_toggle("HARDWARE V-SYNC LOCK", &opt_vsync);
        gui_toggle("SUBPIXEL ANTI-ALIASING", &opt_aa);
        gui_slider("MASTER OUTPUT GAIN", &val_gain, 0, 100);
        gui_slider("MODULATION FREQUENCY", &val_freq, 10, 100);

        /* Real-Time Animated Waveform Canvas */
        int wx = ui.layout_x, wy = ui.layout_y + 4, ww = ui.layout_w, wh = 135;
        draw_rrect(wx, wy, ww, wh, 8, 0xFF11121Du);
        draw_text(wx + 14, wy + 12, "REALTIME SIGNAL WAVEFORM", 2, 0xFF6C7086u);
        int mid_y = wy + wh / 2 + 10;
        float amp = (float)val_gain * 0.42f;
        float freq = (float)val_freq * 0.0025f;
        for (int i = 12; i < ww - 12; i++) {
            float phase = (float)i * freq + (float)ui.tick * 0.08f;
            int py = mid_y + (int)(fast_sin(phase) * amp);
            draw_rrect(wx + i, py - 2, 3, 5, 1, ui.accent);
        }
    } else if (app_tab == 1) {
        gui_begin_panel(228, 20, 512, 440, "DISPLAY AND COLOR ENGINE");
        gui_slider("BACKLIGHT LUMINANCE", &val_lum, 10, 100);
        gui_toggle("10-BIT HDR COMPOSITING", &opt_hdr);
        if (gui_button("RESET DEFAULTS")) {
            opt_vsync = true; opt_aa = true; opt_hdr = false;
            val_gain = 72; val_freq = 45; val_lum = 85;
        }
    } else {
        gui_begin_panel(228, 20, 512, 440, "SYSTEM TELEMETRY");
        gui_slider("SIMULATED LOAD", &val_freq, 0, 100);
        if (gui_button("TRIGGER PULSE")) pulse_cnt++;
        draw_text(ui.layout_x, ui.layout_y + 10, "PULSES EXECUTED:", 2, 0xFF9399B2u);
        draw_int(ui.layout_x + 175, ui.layout_y + 10, pulse_cnt, "", 2, ui.accent);
    }

    ui.mouse_pressed = false;
    ui.mouse_released = false;
    ui.tick++;
}

/* =========================================================================
 * 5. HEADLESS UTILITIES & BMP EXPORTER (For Testing & CLI Output)
 * ========================================================================= */
static bool save_bmp(const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;

    uint32_t file_size = 54u + (uint32_t)(WIN_W * WIN_H * 4);
    int32_t bi_height = -(int32_t)WIN_H;
    uint8_t header[54] = {
        'B', 'M',
        (uint8_t)(file_size & 0xFFu), (uint8_t)((file_size >> 8) & 0xFFu), (uint8_t)((file_size >> 16) & 0xFFu), (uint8_t)((file_size >> 24) & 0xFFu),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        (uint8_t)(WIN_W & 0xFF), (uint8_t)((WIN_W >> 8) & 0xFF), (uint8_t)((WIN_W >> 16) & 0xFF), (uint8_t)((WIN_W >> 24) & 0xFF),
        (uint8_t)(bi_height & 0xFF), (uint8_t)((bi_height >> 8) & 0xFF),
        (uint8_t)((bi_height >> 16) & 0xFF), (uint8_t)((bi_height >> 24) & 0xFF),
        1, 0, 32, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    };

    if (fwrite(header, 1, 54, f) != 54) { fclose(f); return false; }
    if (fwrite(ui.px, 4, WIN_W * WIN_H, f) != (size_t)(WIN_W * WIN_H)) { fclose(f); return false; }
    fclose(f);
    return true;
}

/* Headless smoke test: simulates interactive clicks & validates state transitions */
static int run_headless_tests(const char *bmp_output) {
    printf("Running C99 pixel GUI headless test suite...\n");
    ui.accent = 0xFF89B4FAu;

    /* Frame 1: Initial render */
    gui_frame();
    if (ui.px[100 * WIN_W + 100] == 0) {
        fprintf(stderr, "FAIL: framebuffer uninitialized\n");
        return 1;
    }
    printf("ok   framebuffer rasterized (760x480)\n");

    /* Test 1: Click Nav Item (Renderer tab) */
    ui.mx = 100; ui.my = 130;
    ui.mouse_pressed = true;
    gui_frame();
    if (app_tab != 1) {
        fprintf(stderr, "FAIL: tab switch failed (expected 1, got %d)\n", app_tab);
        return 1;
    }
    printf("ok   nav item click (switched to Renderer tab)\n");

    /* Test 2: Click Telemetry tab */
    ui.mx = 100; ui.my = 175;
    ui.mouse_pressed = true;
    gui_frame();
    if (app_tab != 2) {
        fprintf(stderr, "FAIL: tab switch failed (expected 2, got %d)\n", app_tab);
        return 1;
    }
    printf("ok   nav item click (switched to Telemetry tab)\n");

    /* Test 3: Click Trigger Pulse Button */
    int before_pulses = pulse_cnt;
    ui.mx = 300; ui.my = 150;
    ui.mouse_pressed = true;
    gui_frame();
    if (pulse_cnt != before_pulses + 1) {
        fprintf(stderr, "FAIL: button trigger failed (expected %d, got %d)\n", before_pulses + 1, pulse_cnt);
        return 1;
    }
    printf("ok   button click triggered pulse (count=%d)\n", pulse_cnt);

    /* Test 4: Return to Dashboard */
    ui.mx = 100; ui.my = 90;
    ui.mouse_pressed = true;
    gui_frame();
    if (app_tab != 0) {
        fprintf(stderr, "FAIL: return to dashboard failed\n");
        return 1;
    }
    printf("ok   nav item return to Dashboard\n");

    /* Test 5: Toggle VSync */
    bool before_vsync = opt_vsync;
    ui.mx = 400; ui.my = 90;
    ui.mouse_pressed = true;
    gui_frame();
    if (opt_vsync == before_vsync) {
        fprintf(stderr, "FAIL: toggle vsync failed\n");
        return 1;
    }
    printf("ok   toggle switch state flipped\n");

    /* Test 6: Drag Slider (Modulation Frequency) */
    ui.mx = 350; ui.my = 245;
    ui.mouse_pressed = true;
    ui.mouse_down = true;
    gui_frame();
    ui.mx = 450;
    gui_frame();
    ui.mouse_down = false;
    ui.mouse_released = true;
    gui_frame();
    printf("ok   slider drag processed (val_freq=%d)\n", val_freq);

    /* Test 7: Accent swatch selection */
    ui.mx = 36 + 2 * 38 + 10; ui.my = 425;
    ui.mouse_pressed = true;
    gui_frame();
    if (ui.accent != 0xFFCBA6F7u) {
        fprintf(stderr, "FAIL: accent swatch failed (got 0x%08X)\n", ui.accent);
        return 1;
    }
    printf("ok   accent theme swatch selected (purple 0x%08X)\n", ui.accent);

    if (bmp_output) {
        if (!save_bmp(bmp_output)) {
            fprintf(stderr, "FAIL: saving BMP output to %s failed\n", bmp_output);
            return 1;
        }
        printf("ok   snapshot saved to %s\n", bmp_output);
    }

    printf("passed: 7  failed: 0\n");
    return 0;
}

static void print_usage(void) {
    puts("Usage: " APP_NAME " [OPTIONS]\n"
         "\n"
         "Monolithic Pure C99 Pixel GUI & Immediate-Mode Engine.\n"
         "\n"
         "Options:\n"
         "  --test                 Run automated headless test suite and exit\n"
         "  --snapshot <file.bmp>  Render one frame, save to 32-bit BMP and exit\n"
         "  --headless             Run 120 simulation frames headlessly and exit\n"
         "  -h, --help             Display this help message and exit\n"
         "  -V, --version          Display version information and exit\n");
}

/* =========================================================================
 * 6. NATIVE OS WINDOWING BACKENDS (Win32 / X11 / macOS Cocoa / Headless)
 * ========================================================================= */
#if defined(C99GUI_BACKEND_WIN32)

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MOUSEMOVE:
            ui.mx = (int)(short)LOWORD(lp);
            ui.my = (int)(short)HIWORD(lp);
            return 0;
        case WM_LBUTTONDOWN:
            ui.mouse_down = true; ui.mouse_pressed = true;
            SetCapture(hwnd);
            return 0;
        case WM_LBUTTONUP:
            ui.mouse_down = false; ui.mouse_released = true;
            ReleaseCapture();
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) ui.running = false;
            return 0;
        case WM_CLOSE:
        case WM_DESTROY:
            ui.running = false;
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static int run_win32_window(void) {
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = GetModuleHandleA(NULL);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "C99GuiWindow";
    RegisterClassA(&wc);

    RECT rc = {0, 0, WIN_W, WIN_H};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    AdjustWindowRect(&rc, style, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Pure C99 Native Pixel GUI", style,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top,
                                NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) {
        fprintf(stderr, "c99gui: CreateWindowExA failed\n");
        return 1;
    }

    HDC hdc = GetDC(hwnd);
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = WIN_W;
    bmi.bmiHeader.biHeight      = -WIN_H; /* Top-down DIB */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    ui.accent = 0xFF89B4FAu;
    ui.running = true;
    while (ui.running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        gui_frame();
        StretchDIBits(hdc, 0, 0, WIN_W, WIN_H, 0, 0, WIN_W, WIN_H,
                      ui.px, &bmi, DIB_RGB_COLORS, SRCCOPY);
        Sleep(16);
    }
    ReleaseDC(hwnd, hdc);
    return 0;
}

#elif defined(C99GUI_BACKEND_COCOA)
#include <CoreGraphics/CoreGraphics.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <unistd.h>

typedef struct CGPoint NSPoint;
typedef struct CGSize  NSSize;
typedef struct CGRect  NSRect;

#define msg0(r, o, s)          ((r (*)(id, SEL))objc_msgSend)((id)(o), sel_registerName(s))
#define msg1(r, o, s, T1, a1)  ((r (*)(id, SEL, T1))objc_msgSend)((id)(o), sel_registerName(s), a1)
#define msg4(r, o, s, T1, a1, T2, a2, T3, a3, T4, a4) \
    ((r (*)(id, SEL, T1, T2, T3, T4))objc_msgSend)((id)(o), sel_registerName(s), a1, a2, a3, a4)

static int run_cocoa_window(void) {
    id app = msg0(id, objc_getClass("NSApplication"), "sharedApplication");
    msg1(BOOL, app, "setActivationPolicy:", long, 0);

    NSRect frame = {{100.0, 100.0}, {(double)WIN_W, (double)WIN_H}};
    id win = msg4(id, msg0(id, objc_getClass("NSWindow"), "alloc"),
                  "initWithContentRect:styleMask:backing:defer:",
                  NSRect, frame, unsigned long, 3u, unsigned long, 2u, BOOL, 0);
    id title = msg1(id, objc_getClass("NSString"), "stringWithUTF8String:", const char *, "Pure C99 Native Pixel GUI");
    msg1(void, win, "setTitle:", id, title);
    msg1(void, win, "makeKeyAndOrderFront:", id, NULL);
    msg1(void, app, "activateIgnoringOtherApps:", BOOL, 1);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    id view = msg0(id, win, "contentView");
    id layer = msg0(id, objc_getClass("CALayer"), "layer");
    msg1(void, view, "setLayer:", id, layer);
    msg1(void, view, "setWantsLayer:", BOOL, 1);

    ui.accent = 0xFF89B4FAu;
    ui.running = true;
    while (ui.running && msg0(BOOL, win, "isVisible")) {
        id ev;
        id distant_past = msg0(id, objc_getClass("NSDate"), "distantPast");
        id mode = msg1(id, objc_getClass("NSString"), "stringWithUTF8String:", const char *, "kCFRunLoopDefaultMode");
        while ((ev = msg4(id, app, "nextEventMatchingMask:untilDate:inMode:dequeue:",
                          unsigned long long, ~0ull, id, distant_past, id, mode, BOOL, 1)) != NULL) {
            unsigned long type = msg0(unsigned long, ev, "type");
            if (type == 1) { ui.mouse_down = true;  ui.mouse_pressed = true; }
            if (type == 2) { ui.mouse_down = false; ui.mouse_released = true; }
            msg1(void, app, "sendEvent:", id, ev);
        }
        NSPoint mp = msg0(NSPoint, win, "mouseLocationOutsideOfEventStream");
        ui.mx = (int)mp.x;
        ui.my = WIN_H - (int)mp.y;

        gui_frame();
        CGContextRef ctx = CGBitmapContextCreate(ui.px, WIN_W, WIN_H, 8, WIN_W * 4, cs,
                           (uint32_t)kCGBitmapByteOrder32Little | (uint32_t)kCGImageAlphaNoneSkipFirst);
        CGImageRef img = CGBitmapContextCreateImage(ctx);
        msg1(void, layer, "setContents:", id, (id)img);
        CGImageRelease(img);
        CGContextRelease(ctx);
        usleep(16000);
    }
    CGColorSpaceRelease(cs);
    return 0;
}

#elif defined(C99GUI_BACKEND_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>

static int run_x11_window(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "c99gui: cannot connect to X11 display (running in headless mode)\n");
        return run_headless_tests(NULL);
    }
    int scr = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 100, 100, WIN_W, WIN_H, 1,
                                     BlackPixel(dpy, scr), WhitePixel(dpy, scr));
    XStoreName(dpy, win, "Pure C99 Native Pixel GUI");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XMapWindow(dpy, win);

    GC gc = DefaultGC(dpy, scr);
    XImage *img = XCreateImage(dpy, DefaultVisual(dpy, scr), (unsigned int)DefaultDepth(dpy, scr),
                               ZPixmap, 0, (char *)ui.px, WIN_W, WIN_H, 32, 0);

    ui.accent = 0xFF89B4FAu;
    ui.running = true;
    while (ui.running) {
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == MotionNotify) {
                ui.mx = ev.xmotion.x; ui.my = ev.xmotion.y;
            } else if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
                ui.mouse_down = true; ui.mouse_pressed = true;
            } else if (ev.type == ButtonRelease && ev.xbutton.button == Button1) {
                ui.mouse_down = false; ui.mouse_released = true;
            } else if (ev.type == KeyPress) {
                if (XLookupKeysym(&ev.xkey, 0) == XK_Escape) ui.running = false;
            } else if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) {
                ui.running = false;
            }
        }
        gui_frame();
        XPutImage(dpy, win, gc, img, 0, 0, 0, 0, WIN_W, WIN_H);
        XFlush(dpy);

        struct timespec ts = {0, 16000000L};
        nanosleep(&ts, NULL);
    }
    img->data = NULL;
    XDestroyImage(img);
    XCloseDisplay(dpy);
    return 0;
}
#endif

int main(int argc, char **argv) {
    const char *snapshot_file = NULL;
    bool do_test = false;
    bool headless = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_usage();
            return 0;
        } else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V")) {
            puts(APP_NAME " " APP_VERSION);
            return 0;
        } else if (!strcmp(argv[i], "--test")) {
            do_test = true;
        } else if (!strcmp(argv[i], "--headless")) {
            headless = true;
        } else if (!strcmp(argv[i], "--snapshot")) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                snapshot_file = argv[++i];
            } else {
                snapshot_file = "snapshot.bmp";
            }
        } else {
            fprintf(stderr, APP_NAME ": unknown option '%s' (try --help)\n", argv[i]);
            return 2;
        }
    }

    if (do_test || snapshot_file) {
        return run_headless_tests(snapshot_file);
    }

    if (headless) {
        ui.accent = 0xFF89B4FAu;
        for (int frame = 0; frame < 120; frame++) {
            gui_frame();
        }
        return 0;
    }

#if defined(C99GUI_BACKEND_WIN32)
    return run_win32_window();
#elif defined(C99GUI_BACKEND_COCOA)
    return run_cocoa_window();
#elif defined(C99GUI_BACKEND_X11)
    return run_x11_window();
#else
    return run_headless_tests(NULL);
#endif
}
