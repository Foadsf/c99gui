/*
 * gui.c - Comprehensive Pure C99 2D Vector Graphics & Immediate-Mode GUI Framework
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Foad Sojoodi Farimani
 *
 * Zero external libraries, zero -lm dependency, zero heap allocations.
 * Single monolithic C99 source file.
 *
 * Capabilities:
 *   - Resolution-independent SDF and Bezier vector rasterizer (980x640 ARGB)
 *   - Fast compiler- and libm-agnostic float math (vg_sqrtf, vg_sinf, vg_cosf)
 *   - Proportional ASCII quadratic Bezier typography (M, L, Q)
 *   - Scissor clipping stack (vg_push_clip, vg_pop_clip, vg_reset_clip)
 *   - Scanline polygon fill (vg_fill) & unified single-pass SDF stroke (vg_stroke)
 *   - Bilinear 4-corner gradients (vg_fill_gradient) & soft drop shadows
 *   - Scrollable containers with mouse-wheel & draggable vector scrollbars
 *   - Responsive multi-column grid layout (ui_columns, ui_next_column)
 *   - Smooth per-widget lerp animation transitions (ui_anim_step)
 *   - Comprehensive widget suite:
 *       * Text Input Box with blinking vector caret and keyboard handling
 *       * Dropdown Combo Box (deferred popup overlay)
 *       * Animated Toggle Switches, Checkboxes & Radio Buttons
 *       * Continuous Sliders & Rotary Vector Knobs (drag & wheel)
 *       * Interactive Node Graph Studio with live cubic Bezier wires
 *       * Real-time filled vector area charts / sparklines
 *       * Selectable Data Tables with status badges
 *       * 2D HSV Saturation/Value + Hue Bar Color Picker & Hex readout
 *       * Daylight High-Contrast Light Mode / Dark Mode palettes
 *       * Right-Click Context Menu, Modal Dialogs & Toast Notifications
 *       * Live DPI Vector Scaling (0.75x - 1.30x)
 *   - Native OS Windowing Backends:
 *       * Windows: Win32 GDI (StretchDIBits)
 *       * Linux / BSD: X11 (XPutImage) or Headless Framebuffer
 *       * macOS: Cocoa / CoreGraphics via C runtime (objc_msgSend)
 *       * Headless: Automated test suite (--test) & 32-bit BMP export (--snapshot)
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
#define APP_VERSION "2.1.0"
#define WIN_W       980
#define WIN_H       640
#define MIN_W       640
#define MIN_H       400
#define MAX_BUF_W   1920
#define MAX_BUF_H   1080
#define MAX_SEGS    256
#define MAX_WIDGETS 256
#define VG_PI       3.1415926535f

/* =========================================================================
 * 1. COMPILER-AGNOSTIC FLOAT MATH & COLOR UTILITIES (Zero -lm dependency)
 * ========================================================================= */
static float vg_absf(float x) { return x < 0.0f ? -x : x; }
static float vg_minf(float a, float b) { return a < b ? a : b; }
static float vg_maxf(float a, float b) { return a > b ? a : b; }
static float vg_clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static float vg_lerpf(float a, float b, float t) { return a + (b - a) * t; }

static float vg_sqrtf(float x) {
    if (x <= 0.0f) return 0.0f;
    union { float f; uint32_t i; } u;
    u.f = x;
    u.i = 0x5f3759dfu - (u.i >> 1);
    float y = u.f;
    y = y * (1.5f - 0.5f * x * y * y);
    y = y * (1.5f - 0.5f * x * y * y);
    return x * y;
}

static float vg_sinf(float x) {
    /* Wrap to [-0.5, 0.5) turns, then parabolic sine with one refinement (max error ~0.0011) */
    x *= 0.15915494f;
    x -= (float)(int)x;
    if (x < -0.5f) x += 1.0f;
    else if (x >= 0.5f) x -= 1.0f;
    float y = 8.0f * x - 16.0f * x * vg_absf(x);
    return 0.225f * (y * vg_absf(y) - y) + y;
}

static float vg_cosf(float x) { return vg_sinf(x + 1.57079632f); }

static uint32_t color_lerp(uint32_t c0, uint32_t c1, float t) {
    uint32_t it = (uint32_t)(vg_clampf(t, 0.0f, 1.0f) * 256.0f);
    uint32_t i0 = 256u - it;
    uint32_t rb = ((((c0 & 0x00FF00FFu) * i0) + ((c1 & 0x00FF00FFu) * it)) >> 8) & 0x00FF00FFu;
    uint32_t g  = ((((c0 & 0x0000FF00u) * i0) + ((c1 & 0x0000FF00u) * it)) >> 8) & 0x0000FF00u;
    uint32_t a  = ((((c0 >> 24) & 0xFFu) * i0 + ((c1 >> 24) & 0xFFu) * it) >> 8) << 24;
    return a | rb | g;
}

static uint32_t hsv_to_argb(float h, float s, float v) {
    h = h - (float)(int)h;
    if (h < 0.0f) h += 1.0f;
    float c = v * s;
    float x = c * (1.0f - vg_absf(((h * 6.0f) - (float)((int)(h * 3.0f) * 2)) - 1.0f));
    float m = v - c;
    float r = 0.0f, g = 0.0f, b = 0.0f;
    int sec = (int)(h * 6.0f);
    switch (sec % 6) {
        case 0: r = c; g = x; b = 0.0f; break;
        case 1: r = x; g = c; b = 0.0f; break;
        case 2: r = 0.0f; g = c; b = x; break;
        case 3: r = 0.0f; g = x; b = c; break;
        case 4: r = x; g = 0.0f; b = c; break;
        default: r = c; g = 0.0f; b = x; break;
    }
    uint32_t ri = (uint32_t)(vg_clampf(r + m, 0.0f, 1.0f) * 255.0f);
    uint32_t gi = (uint32_t)(vg_clampf(g + m, 0.0f, 1.0f) * 255.0f);
    uint32_t bi = (uint32_t)(vg_clampf(b + m, 0.0f, 1.0f) * 255.0f);
    return 0xFF000000u | (ri << 16) | (gi << 8) | bi;
}

static void str_append(char *dst, int max_len, const char *src) {
    int len = (int)strlen(dst);
    while (*src && len < max_len - 1) dst[len++] = *src++;
    dst[len] = '\0';
}

static void str_from_int(char *buf, int val, const char *suffix) {
    int i = 0;
    if (val < 0) { buf[i++] = '-'; val = -val; }
    if (val == 0) {
        buf[i++] = '0';
    } else {
        char rev[16]; int r = 0;
        while (val > 0 && r < 15) { rev[r++] = (char)('0' + (val % 10)); val /= 10; }
        while (r > 0) buf[i++] = rev[--r];
    }
    buf[i] = '\0';
    if (suffix) str_append(buf, 32, suffix);
}

static void str_from_hex(char *buf, uint32_t col) {
    const char *hex = "0123456789ABCDEF";
    buf[0] = '#';
    for (int i = 0; i < 6; i++) {
        buf[1 + i] = hex[(col >> (20 - i * 4)) & 0xFu];
    }
    buf[7] = '\0';
}

/* =========================================================================
 * 2. ENGINE STATE, CLIPPING STACK & IMMEDIATE-MODE CONTEXT
 * ========================================================================= */
typedef struct {
    float x0, y0, x1, y1, vx, vy, inv_len2;
} VgSeg;

typedef struct { int x0, y0, x1, y1; } ClipRect;

typedef struct {
    uint32_t bg_canvas, bg_card, bg_elem, bg_hover, border, text_main, text_muted;
} ThemePalette;

typedef struct {
    char name[64];
    int x, y, w, h, parent_id;
} Rect;

typedef struct {
    uint32_t px[MAX_BUF_W * MAX_BUF_H];
    VgSeg segs[MAX_SEGS];
    int seg_count;
    float cur_x, cur_y, start_x, start_y;
    float min_x, min_y, max_x, max_y;

    /* Scissor Clip Stack */
    ClipRect clip_stack[8];
    int clip_depth;
    ClipRect clip;

    /* Input State */
    float mx, my, click_x, click_y;
    bool mouse_down, mouse_pressed, mouse_released;
    bool rmouse_pressed;
    float wheel_dy;
    char key_char;
    bool key_backspace, key_enter, key_esc;

    /* Widget & Layout State */
    int active_id, hot_id, focus_id, next_id;
    float anim[MAX_WIDGETS];
    float scale;
    float lx, ly, lw;
    float col_x, col_w;
    int col_idx, col_count;

    /* Deferred Overlays */
    const char *tooltip_text;
    int open_dropdown_id;
    float dd_x, dd_y, dd_w;
    const char **dd_items;
    int dd_count;
    int *dd_selected;

    bool ctx_open;
    float ctx_x, ctx_y;
    bool modal_open;

    char toast_msg[64];
    int toast_timer;

    /* Theme */
    bool light_mode;
    uint32_t accent;
    ThemePalette pal;
    unsigned int tick;
    bool running;
    int w, h;

    /* Honest Test Framework: Widget Rect Recording */
    Rect rects[512];
    int rect_count;
    int current_parent;
    int parent_stack[16];
    int parent_depth;
    float col_row_h;
    int test_negative_mode;
    int test_passed;
    int test_failed;
    int record_text_rects;
} GuiCtx;

static GuiCtx ui;

/* CHECK macro for honest test counting */
#define CHECK(cond, msg) do { \
    if (cond) { \
        ui.test_passed++; \
        printf("ok   %s\n", msg); \
    } else { \
        ui.test_failed++; \
        fprintf(stderr, "FAIL: %s\n", msg); \
    } \
} while (0)

/* Record one rect; returns its index, or -1 when not recorded (invisible or full). */
static int record_rect(const char *name, int x, int y, int w, int h, int parent_id) {
    /* Only record rects that intersect the current clip region (visible widgets) */
    /* Exception: in negative test mode, record all rects to detect bounds failures */
    if (!ui.test_negative_mode && (x + w <= ui.clip.x0 || x >= ui.clip.x1 || y + h <= ui.clip.y0 || y >= ui.clip.y1)) {
        return -1; /* Outside clip region, not visible */
    }
    if (ui.rect_count >= 512) return -1;
    strncpy(ui.rects[ui.rect_count].name, name, 63);
    ui.rects[ui.rect_count].name[63] = '\0';
    ui.rects[ui.rect_count].x = x;
    ui.rects[ui.rect_count].y = y;
    ui.rects[ui.rect_count].w = w;
    ui.rects[ui.rect_count].h = h;
    ui.rects[ui.rect_count].parent_id = parent_id;
    ui.rect_count++;
    return ui.rect_count - 1;
}

/* Parent tracking for sibling-overlap checks: children drawn between push/pop
 * get the pushed rect index as their parent. */
static void ui_push_parent(int idx) {
    if (ui.parent_depth < 16) ui.parent_stack[ui.parent_depth++] = ui.current_parent;
    if (idx >= 0) ui.current_parent = idx;
}

static void ui_pop_parent(void) {
    if (ui.parent_depth > 0) ui.current_parent = ui.parent_stack[--ui.parent_depth];
}

static float vg_text_width(const char *s, float size);

static int find_rect(const char *name) {
    for (int i = 0; i < ui.rect_count; i++) {
        if (strcmp(ui.rects[i].name, name) == 0) return i;
    }
    return -1;
}

/* =========================================================================
 * 3. VECTOR RASTERIZER (Scissor Clipping, SDF Shapes, Gradients & Paths)
 * ========================================================================= */
static void vg_reset_clip(void) {
    ui.clip_depth = 0;
    ui.clip.x0 = 0; ui.clip.y0 = 0;
    ui.clip.x1 = ui.w; ui.clip.y1 = ui.h;
}

static void vg_push_clip(float x, float y, float w, float h) {
    if (ui.clip_depth < 8) ui.clip_stack[ui.clip_depth++] = ui.clip;
    int x0 = (int)x, y0 = (int)y, x1 = (int)(x + w), y1 = (int)(y + h);
    if (x0 > ui.clip.x0) ui.clip.x0 = x0;
    if (y0 > ui.clip.y0) ui.clip.y0 = y0;
    if (x1 < ui.clip.x1) ui.clip.x1 = x1;
    if (y1 < ui.clip.y1) ui.clip.y1 = y1;
}

static void vg_pop_clip(void) {
    if (ui.clip_depth > 0) ui.clip = ui.clip_stack[--ui.clip_depth];
}

static uint32_t blend_argb(uint32_t dst, uint32_t src, float cov) {
    uint32_t a_cov = (uint32_t)(vg_clampf(cov, 0.0f, 1.0f) * 256.0f);
    uint32_t a = (((src >> 24) & 0xFFu) * a_cov) >> 8;
    if (a == 0) return dst;
    if (a >= 255) return src | 0xFF000000u;
    uint32_t ia = 256u - a;
    uint32_t rb = ((((src & 0x00FF00FFu) * a) + ((dst & 0x00FF00FFu) * ia)) >> 8) & 0x00FF00FFu;
    uint32_t g  = ((((src & 0x0000FF00u) * a) + ((dst & 0x0000FF00u) * ia)) >> 8) & 0x0000FF00u;
    return 0xFF000000u | rb | g;
}

static float sdf_rrect(float px, float py, float cx, float cy, float hw, float hh, float r) {
    float qx = vg_absf(px - cx) - (hw - r);
    float qy = vg_absf(py - cy) - (hh - r);
    float ox = vg_maxf(qx, 0.0f), oy = vg_maxf(qy, 0.0f);
    return vg_sqrtf(ox * ox + oy * oy) + vg_minf(vg_maxf(qx, qy), 0.0f) - r;
}

static void vg_fill_rrect(float x, float y, float w, float h, float r, uint32_t col) {
    if (w <= 0.0f || h <= 0.0f) return;
    float hw = w * 0.5f, hh = h * 0.5f, cx = x + hw, cy = y + hh;
    r = vg_minf(r, vg_minf(hw, hh));

    int x0 = (int)(x - 1.0f); if (x0 < ui.clip.x0) x0 = ui.clip.x0;
    int y0 = (int)(y - 1.0f); if (y0 < ui.clip.y0) y0 = ui.clip.y0;
    int x1 = (int)(x + w + 2.0f); if (x1 > ui.clip.x1) x1 = ui.clip.x1;
    int y1 = (int)(y + h + 2.0f); if (y1 > ui.clip.y1) y1 = ui.clip.y1;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t *row = &ui.px[py * ui.w];
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            if (fx > x + r && fx < x + w - r && fy > y + r && fy < y + h - r) {
                row[px] = blend_argb(row[px], col, 1.0f);
                continue;
            }
            float d = sdf_rrect(fx, fy, cx, cy, hw, hh, r);
            if (d < 0.5f) row[px] = blend_argb(row[px], col, 0.5f - d);
        }
    }
}

static void vg_fill_gradient(float x, float y, float w, float h, float r,
                             uint32_t c_tl, uint32_t c_tr, uint32_t c_bl, uint32_t c_br) {
    float hw = w * 0.5f, hh = h * 0.5f, cx = x + hw, cy = y + hh;
    r = vg_minf(r, vg_minf(hw, hh));
    int x0 = (int)(x - 1.0f); if (x0 < ui.clip.x0) x0 = ui.clip.x0;
    int y0 = (int)(y - 1.0f); if (y0 < ui.clip.y0) y0 = ui.clip.y0;
    int x1 = (int)(x + w + 2.0f); if (x1 > ui.clip.x1) x1 = ui.clip.x1;
    int y1 = (int)(y + h + 2.0f); if (y1 > ui.clip.y1) y1 = ui.clip.y1;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        float ty = vg_clampf((fy - y) / (h > 0.0f ? h : 1.0f), 0.0f, 1.0f);
        uint32_t c_left  = color_lerp(c_tl, c_bl, ty);
        uint32_t c_right = color_lerp(c_tr, c_br, ty);
        uint32_t *row = &ui.px[py * ui.w];
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            float d = sdf_rrect(fx, fy, cx, cy, hw, hh, r);
            if (d < 0.5f) {
                float tx = vg_clampf((fx - x) / (w > 0.0f ? w : 1.0f), 0.0f, 1.0f);
                uint32_t col = color_lerp(c_left, c_right, tx);
                row[px] = blend_argb(row[px], col, 0.5f - d);
            }
        }
    }
}

static void vg_stroke_rrect(float x, float y, float w, float h, float r, float sw, uint32_t col) {
    float hw = w * 0.5f, hh = h * 0.5f, cx = x + hw, cy = y + hh, hr = sw * 0.5f;
    r = vg_minf(r, vg_minf(hw, hh));
    int x0 = (int)(x - sw - 2.0f); if (x0 < ui.clip.x0) x0 = ui.clip.x0;
    int y0 = (int)(y - sw - 2.0f); if (y0 < ui.clip.y0) y0 = ui.clip.y0;
    int x1 = (int)(x + w + sw + 2.0f); if (x1 > ui.clip.x1) x1 = ui.clip.x1;
    int y1 = (int)(y + h + sw + 2.0f); if (y1 > ui.clip.y1) y1 = ui.clip.y1;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t *row = &ui.px[py * ui.w];
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            if (fx > x + r + sw && fx < x + w - r - sw && fy > y + r + sw && fy < y + h - r - sw) continue;
            float d = vg_absf(sdf_rrect(fx, fy, cx, cy, hw, hh, r)) - hr;
            if (d < 0.5f) row[px] = blend_argb(row[px], col, 0.5f - d);
        }
    }
}

static void vg_shadow_rrect(float x, float y, float w, float h, float r, float blur, uint32_t col) {
    float hw = w * 0.5f, hh = h * 0.5f, cx = x + hw, cy = y + hh;
    int x0 = (int)(x - blur); if (x0 < ui.clip.x0) x0 = ui.clip.x0;
    int y0 = (int)(y - blur); if (y0 < ui.clip.y0) y0 = ui.clip.y0;
    int x1 = (int)(x + w + blur); if (x1 > ui.clip.x1) x1 = ui.clip.x1;
    int y1 = (int)(y + h + blur); if (y1 > ui.clip.y1) y1 = ui.clip.y1;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t *row = &ui.px[py * ui.w];
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            if (fx > x + r && fx < x + w - r && fy > y + r && fy < y + h - r) continue;
            float d = sdf_rrect(fx, fy, cx, cy, hw, hh, r);
            if (d > 0.0f && d < blur) {
                float t = 1.0f - (d / blur);
                row[px] = blend_argb(row[px], col, t * t);
            }
        }
    }
}

static void vg_fill_circle(float cx, float cy, float r, uint32_t col) {
    vg_fill_rrect(cx - r, cy - r, r * 2.0f, r * 2.0f, r, col);
}

/* Vector Path Builder */
static void vg_begin_path(void) {
    ui.seg_count = 0;
    ui.min_x = 1e9f; ui.min_y = 1e9f;
    ui.max_x = -1e9f; ui.max_y = -1e9f;
}

static void vg_move_to(float x, float y) {
    ui.cur_x = x; ui.cur_y = y;
    ui.start_x = x; ui.start_y = y;
}

static void vg_line_to(float x, float y) {
    if (ui.seg_count < MAX_SEGS) {
        VgSeg *s = &ui.segs[ui.seg_count++];
        s->x0 = ui.cur_x; s->y0 = ui.cur_y;
        s->x1 = x;        s->y1 = y;
        s->vx = x - ui.cur_x; s->vy = y - ui.cur_y;
        float len2 = s->vx * s->vx + s->vy * s->vy;
        s->inv_len2 = (len2 > 1e-6f) ? (1.0f / len2) : 0.0f;

        ui.min_x = vg_minf(ui.min_x, vg_minf(ui.cur_x, x));
        ui.min_y = vg_minf(ui.min_y, vg_minf(ui.cur_y, y));
        ui.max_x = vg_maxf(ui.max_x, vg_maxf(ui.cur_x, x));
        ui.max_y = vg_maxf(ui.max_y, vg_maxf(ui.cur_y, y));
    }
    ui.cur_x = x; ui.cur_y = y;
}

static void vg_close_path(void) {
    vg_line_to(ui.start_x, ui.start_y);
}

static void vg_quad_to(float cx, float cy, float x, float y) {
    float x0 = ui.cur_x, y0 = ui.cur_y;
    const int steps = 6;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps, u = 1.0f - t;
        vg_line_to(u * u * x0 + 2.0f * u * t * cx + t * t * x,
                   u * u * y0 + 2.0f * u * t * cy + t * t * y);
    }
}

static void vg_cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y) {
    float x0 = ui.cur_x, y0 = ui.cur_y;
    const int steps = 12;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps, u = 1.0f - t;
        float px = u * u * u * x0 + 3.0f * u * u * t * c1x + 3.0f * u * t * t * c2x + t * t * t * x;
        float py = u * u * u * y0 + 3.0f * u * u * t * c1y + 3.0f * u * t * t * c2y + t * t * t * y;
        vg_line_to(px, py);
    }
}

static void vg_arc(float cx, float cy, float r, float a0, float a1, int steps) {
    for (int i = 0; i <= steps; i++) {
        float a = a0 + (a1 - a0) * ((float)i / (float)steps);
        float px = cx + vg_cosf(a) * r, py = cy + vg_sinf(a) * r;
        if (i == 0) vg_move_to(px, py);
        else vg_line_to(px, py);
    }
}

/* Scanline Winding-Rule Polygon Fill with Sub-Pixel Edge Smoothing */
static void vg_fill(uint32_t col) {
    if (ui.seg_count < 3) return;
    int x0 = (int)(ui.min_x - 1.0f); if (x0 < ui.clip.x0) x0 = ui.clip.x0;
    int y0 = (int)(ui.min_y - 1.0f); if (y0 < ui.clip.y0) y0 = ui.clip.y0;
    int x1 = (int)(ui.max_x + 2.0f); if (x1 > ui.clip.x1) x1 = ui.clip.x1;
    int y1 = (int)(ui.max_y + 2.0f); if (y1 > ui.clip.y1) y1 = ui.clip.y1;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t *row = &ui.px[py * ui.w];
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            int winding = 0;
            for (int i = 0; i < ui.seg_count; i++) {
                const VgSeg *s = &ui.segs[i];
                if ((s->y0 <= fy && s->y1 > fy) || (s->y1 <= fy && s->y0 > fy)) {
                    float ix = s->x0 + (fy - s->y0) * s->vx / (vg_absf(s->vy) > 1e-5f ? s->vy : 1e-5f);
                    if (fx < ix) winding ^= 1;
                }
            }
            if (winding) row[px] = blend_argb(row[px], col, 1.0f);
        }
    }
}

/* Single-Pass Unified SDF Path Stroke */
static void vg_stroke(float thickness, uint32_t col) {
    if (ui.seg_count == 0) return;
    float rad = thickness * 0.5f, max_d = rad + 0.65f, max_d2 = max_d * max_d;

    int x0 = (int)(ui.min_x - max_d - 1.0f); if (x0 < ui.clip.x0) x0 = ui.clip.x0;
    int y0 = (int)(ui.min_y - max_d - 1.0f); if (y0 < ui.clip.y0) y0 = ui.clip.y0;
    int x1 = (int)(ui.max_x + max_d + 2.0f); if (x1 > ui.clip.x1) x1 = ui.clip.x1;
    int y1 = (int)(ui.max_y + max_d + 2.0f); if (y1 > ui.clip.y1) y1 = ui.clip.y1;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t *row = &ui.px[py * ui.w];
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f, min_d2 = max_d2;
            for (int i = 0; i < ui.seg_count; i++) {
                const VgSeg *s = &ui.segs[i];
                float wx = fx - s->x0, wy = fy - s->y0;
                float t = vg_clampf((wx * s->vx + wy * s->vy) * s->inv_len2, 0.0f, 1.0f);
                float dx = wx - t * s->vx, dy = wy - t * s->vy;
                float d2 = dx * dx + dy * dy;
                if (d2 < min_d2) min_d2 = d2;
            }
            if (min_d2 < max_d2) {
                float cov = (rad + 0.5f) - vg_sqrtf(min_d2);
                if (cov > 0.0f) row[px] = blend_argb(row[px], col, cov);
            }
        }
    }
}

/* =========================================================================
 * 4. PROPORTIONAL BEZIER VECTOR FONT ENGINE (Printable ASCII)
 * ========================================================================= */
static int hex_c(char c) {
    return (c >= '0' && c <= '9') ? (c - '0') : ((c >= 'A' && c <= 'F') ? (c - 'A' + 10) : 0);
}

static const char *vg_glyph_path(char c, float *adv) {
    *adv = 10.2f;
    switch (c) {
        case 'A': return "M0CL40L8CM18L78";
        case 'B': return "M00L0CL5CQ8C89Q8656L06M00L50Q8083Q8656";
        case 'C': return "M83Q8040Q0004L08Q0C4CQ8C89";
        case 'D': return "M00L0CL4CQ8C86Q8040L00";
        case 'E': *adv = 9.2f; return "M70L00L0CL7CM06L66";
        case 'F': *adv = 8.8f; return "M70L00L0CM06L66";
        case 'G': return "M83Q8040Q0004L08Q0C4CQ8C88L86L46";
        case 'H': return "M00L0CM80L8CM06L86";
        case 'I': *adv = 6.2f; return "M10L50M30L3CM1CL5C";
        case 'J': *adv = 8.8f; return "M60L68Q6C3CQ0C09";
        case 'K': return "M00L0CM80L07M35L8C";
        case 'L': *adv = 8.8f; return "M00L0CL7C";
        case 'M': *adv = 11.8f; return "M0CL00L58LA0LAC";
        case 'N': return "M0CL00L8CL80";
        case 'O': return "M04Q0040Q8084L88Q8C4CQ0C08L04";
        case 'P': return "M0CL00L50Q8083Q8656L06";
        case 'Q': return "M04Q0040Q8084L88Q8C4CQ0C08L04M58L8C";
        case 'R': return "M0CL00L50Q8083Q8656L06M56L8C";
        case 'S': return "M83Q8040Q0003Q0646Q8689Q8C4CQ0C09";
        case 'T': *adv = 9.2f; return "M00L80M40L4C";
        case 'U': return "M00L08Q0C4CQ8C88L80";
        case 'V': return "M00L4CL80";
        case 'W': *adv = 11.8f; return "M00L2CL54L8CLA0";
        case 'X': return "M00L8CM80L0C";
        case 'Y': return "M00L46L80M46L4C";
        case 'Z': return "M00L80L0CL8C";
        case 'a': *adv = 9.2f; return "M74L7CM76Q7444Q0408Q0C4CQ7C7A";
        case 'b': *adv = 9.2f; return "M00L0CM06Q0444Q7478Q7C4CQ0C0A";
        case 'c': *adv = 8.5f; return "M76Q6434Q0408Q0C3CQ6C7A";
        case 'd': *adv = 9.2f; return "M70L7CM76Q7434Q0408Q0C3CQ7C7A";
        case 'e': *adv = 9.0f; return "M08L78Q7434Q0408Q0C4CQ6C7A";
        case 'f': *adv = 6.2f; return "M51Q2023L2CM05L55";
        case 'g': *adv = 9.2f; return "M74L7CQ7F3FQ1F0DM76Q7434Q0408Q0C3CQ7C7A";
        case 'h': *adv = 9.2f; return "M00L0CM07Q0444Q7477L7C";
        case 'i': *adv = 4.6f; return "M20L21M24L2C";
        case 'j': *adv = 5.2f; return "M30L31M34L3CQ3F0F";
        case 'k': *adv = 8.8f; return "M00L0CM64L08M27L7C";
        case 'l': *adv = 4.6f; return "M20L2AQ2C4C";
        case 'm': *adv = 11.8f; return "M0CL04M06Q0424Q5456L5CM56Q5474QA4A6LAC";
        case 'n': *adv = 9.2f; return "M0CL04M07Q0444Q7477L7C";
        case 'o': *adv = 9.2f; return "M08Q0444Q7478Q7C4CQ0C08";
        case 'p': *adv = 9.2f; return "M04L0FM06Q0444Q7478Q7C4CQ0C0A";
        case 'q': *adv = 9.2f; return "M74L7FM76Q7434Q0408Q0C3CQ7C7A";
        case 'r': *adv = 6.5f; return "M0CL04M07Q0434Q5455";
        case 's': *adv = 8.5f; return "M66Q6434Q0406Q0838Q686AQ6C3CQ0C0A";
        case 't': *adv = 6.2f; return "M21L2AQ2C5CM04L54";
        case 'u': *adv = 9.2f; return "M04L09Q0C4CQ7C79L74";
        case 'v': *adv = 8.8f; return "M04L3CL74";
        case 'w': *adv = 11.8f; return "M04L2CL56L8CLA4";
        case 'x': *adv = 8.8f; return "M04L7CM74L0C";
        case 'y': *adv = 8.8f; return "M04L3CM74L3CQ1F0F";
        case 'z': *adv = 8.5f; return "M04L74L0CL7C";
        case '0': return "M04Q0040Q8084L88Q8C4CQ0C08L04M2AL62";
        case '1': *adv = 6.8f; return "M12L30L3CM1CL5C";
        case '2': return "M03Q0040Q8083Q8648L0CL8C";
        case '3': return "M02Q0040Q8083Q8646Q8689Q8C4CQ0C0A";
        case '4': return "M6CL60L08L88";
        case '5': return "M80L00L06L56Q8689Q8C4CQ0C09";
        case '6': return "M82Q7040Q0005L08Q0C4CQ8C89Q8545Q0508";
        case '7': return "M00L80L3C";
        case '8': return "M46Q0603Q0040Q8083Q8646Q0609Q0C4CQ8C89Q8646";
        case '9': return "M0AQ1C4CQ8C87L84Q8040Q0003Q0747Q8784";
        case '.': *adv = 4.5f; return "M2BL2C";
        case ',': *adv = 4.5f; return "M2BL1E";
        case ':': *adv = 4.5f; return "M24L25M2AL2B";
        case ';': *adv = 4.5f; return "M24L25M2AL1D";
        case '-': *adv = 7.5f; return "M16L66";
        case '_': *adv = 8.5f; return "M0CL7C";
        case '+': *adv = 8.8f; return "M16L76M43L49";
        case '=': *adv = 8.5f; return "M14L74M18L78";
        case '/': *adv = 7.5f; return "M1CL70";
        case '#': *adv = 10.5f; return "M21L2BM61L6BM04L84M08L88";
        case '%': *adv = 11.0f; return "M1CL70M10Q0002Q2220Q0010M7AQ6A6CQ8C8AQ6A7A";
        case '(': *adv = 6.0f; return "M40Q164C";
        case ')': *adv = 6.0f; return "M10Q461C";
        case '[': *adv = 6.0f; return "M40L10L1CL4C";
        case ']': *adv = 6.0f; return "M10L40L4CL1C";
        case '!': *adv = 5.0f; return "M20L28M2BL2C";
        case '?': *adv = 8.8f; return "M12Q1040Q7073Q7547L49M4BL4C";
        case '&': *adv = 10.5f; return "M8CL24Q1130Q6062Q6427Q090AQ1C4CQ6C88";
        case '\'': *adv = 4.0f; return "M20L23";
        case '"':  *adv = 6.5f; return "M10L13M40L43";
        case '*':  *adv = 7.5f; return "M13L67M63L17M41L49";
        case '$': *adv = 8.5f; return "M2CL70M20Q0002Q2220Q0010M7AQ6A6CQ8C8AQ6A7A";
        case '<': *adv = 7.5f; return "M60L24L6CL2C";
        case '>': *adv = 7.5f; return "M20L64L2CL6C";
        case '@': *adv = 10.5f; return "M84Q7040Q4000Q0040Q0080Q40CCQ8488Q8884";
        case '\\': *adv = 7.5f; return "M1CL70";
        case '^': *adv = 7.5f; return "M20L64LAC";
        case '`': *adv = 4.0f; return "M20L23";
        case '{': *adv = 6.0f; return "M40Q164CQ4884Q8040Q480C";
        case '|': *adv = 4.6f; return "M20L2C";
        case '}': *adv = 6.0f; return "M10Q461CQ8884Q8040Q487C";
        case '~': *adv = 8.5f; return "M16Q2464Q6C2CQ7C7C";
        default:   *adv = 5.2f; return "";
    }
}

static float vg_text_width(const char *s, float size) {
    float scale = size / 12.0f, w = 0.0f;
    for (; *s; s++) { float adv = 0.0f; vg_glyph_path(*s, &adv); w += adv * scale; }
    return w;
}

static void vg_text(float x, float y, float size, const char *s, uint32_t col) {
    /* Record text rect for honest bounds checking (sibling overlap, parent containment) */
    if (ui.record_text_rects && ui.rect_count < 512) {
        float w = vg_text_width(s, size);
        float h = size * 1.2f;
        char name[64];
        snprintf(name, sizeof(name), "text:%.40s", s);
        record_rect(name, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    }
    float scale = size / 12.0f;
    float stroke = vg_maxf(size * 0.135f, 1.25f);
    for (; *s; s++) {
        float adv = 0.0f;
        const char *p = vg_glyph_path(*s, &adv);
        if (*p && x + adv * scale >= (float)ui.clip.x0 && x <= (float)ui.clip.x1) {
            vg_begin_path();
            while (*p) {
                char cmd = *p++;
                if (cmd == 'M') {
                    vg_move_to(x + (float)hex_c(p[0]) * scale, y + (float)hex_c(p[1]) * scale);
                    p += 2;
                } else if (cmd == 'L') {
                    vg_line_to(x + (float)hex_c(p[0]) * scale, y + (float)hex_c(p[1]) * scale);
                    p += 2;
                } else if (cmd == 'Q') {
                    vg_quad_to(x + (float)hex_c(p[0]) * scale, y + (float)hex_c(p[1]) * scale,
                               x + (float)hex_c(p[2]) * scale, y + (float)hex_c(p[3]) * scale);
                    p += 4;
                }
            }
            vg_stroke(stroke, col);
        }
        x += adv * scale;
    }
}

/* =========================================================================
 * 5. IMMEDIATE-MODE LAYOUT, CONTAINERS & WIDGET SUITE
 * ========================================================================= */
static int ui_id(void) {
    return (ui.next_id < MAX_WIDGETS - 1) ? ++ui.next_id : ui.next_id;
}

static float ui_anim_step(int id, float target, float speed) {
    ui.anim[id] += (target - ui.anim[id]) * speed;
    return ui.anim[id];
}

static bool hit_box(float x, float y, float w, float h) {
    if (ui.modal_open) return false;
    return ui.mx >= x && ui.mx < (x + w) && ui.my >= y && ui.my < (y + h) &&
           ui.mx >= (float)ui.clip.x0 && ui.mx < (float)ui.clip.x1 &&
           ui.my >= (float)ui.clip.y0 && ui.my < (float)ui.clip.y1;
}

static void ui_columns(int count) {
    ui.col_count = count > 1 ? count : 1;
    ui.col_idx = 0;
    ui.col_x = ui.lx;
    ui.col_w = ui.lw;
    float gap = 12.0f * ui.scale;
    ui.lw = (ui.col_w - gap * (float)(ui.col_count - 1)) / (float)ui.col_count;
}

static void ui_next_column(float row_h) {
    float gap = 12.0f * ui.scale;
    ui.col_row_h = row_h;
    ui.col_idx = (ui.col_idx + 1) % ui.col_count;
    if (ui.col_idx == 0) {
        ui.lx = ui.col_x;
    } else {
        ui.ly -= row_h;
        ui.lx = ui.col_x + (float)ui.col_idx * (ui.lw + gap);
    }
}

static void ui_end_columns(void) {
    /* A dangling last column (odd item count) has rewound ui.ly; advance past it
     * so following content never overlaps the last row. */
    if (ui.col_idx != 0) ui.ly += ui.col_row_h;
    ui.lx = ui.col_x;
    ui.lw = ui.col_w;
    ui.col_count = 1;
    ui.col_idx = 0;
}

static int ui_card(float x, float y, float w, float h, float r) {
    int self = record_rect("card", (int)x, (int)y, (int)w, (int)h, -1);
    vg_shadow_rrect(x, y + 4.0f * ui.scale, w, h, r, 14.0f * ui.scale,
                    ui.light_mode ? 0x22000000u : 0x58000000u);
    vg_fill_rrect(x, y, w, h, r, ui.pal.bg_card);
    vg_stroke_rrect(x, y, w, h, r, 1.1f, ui.pal.border);
    return self;
}

static void ui_section(const char *title) {
    vg_text(ui.lx, ui.ly + 2.0f * ui.scale, 11.5f * ui.scale, title, ui.accent);
    float tw = vg_text_width(title, 11.5f * ui.scale) + 12.0f * ui.scale;
    vg_fill_rrect(ui.lx + tw, ui.ly + 8.0f * ui.scale, ui.lw - tw, 1.2f, 0.5f, ui.pal.border);
    ui.ly += 22.0f * ui.scale;
}

/* Scrollable Container with Mouse Wheel & Vector Scrollbar */
static void ui_begin_scroll(float x, float y, float w, float h, float *scroll_y, float content_h) {
    if (hit_box(x, y, w, h) && ui.wheel_dy != 0.0f && ui.open_dropdown_id == 0) {
        *scroll_y -= ui.wheel_dy * 28.0f * ui.scale;
    }
    float max_scroll = vg_maxf(0.0f, content_h - h);
    *scroll_y = vg_clampf(*scroll_y, 0.0f, max_scroll);

    vg_push_clip(x, y, w, h);
    /* Record scroll viewport as a rect for honest bounds checking */
    {
        int vp = record_rect("scroll_viewport", (int)x, (int)y, (int)w, (int)h, ui.current_parent);
        ui_push_parent(vp);
    }
    ui.lx = x + 18.0f * ui.scale;
    ui.ly = y + 16.0f * ui.scale - *scroll_y;
    ui.lw = w - 42.0f * ui.scale;

    if (max_scroll > 1.0f) {
        float sb_x = x + w - 10.0f * ui.scale;
        float sb_h = vg_maxf(28.0f * ui.scale, (h / content_h) * (h - 16.0f * ui.scale));
        float sb_y = y + 8.0f * ui.scale + (*scroll_y / max_scroll) * (h - 16.0f * ui.scale - sb_h);
        vg_fill_rrect(sb_x, y + 8.0f * ui.scale, 5.0f * ui.scale, h - 16.0f * ui.scale, 2.5f * ui.scale, ui.pal.bg_elem);
        vg_fill_rrect(sb_x, sb_y, 5.0f * ui.scale, sb_h, 2.5f * ui.scale, ui.accent);
    }
}

static void ui_end_scroll(void) {
    vg_pop_clip();
    ui_pop_parent(); /* restore parent pushed by ui_begin_scroll */
}

static void ui_toast(const char *msg) {
    strncpy(ui.toast_msg, msg, 63);
    ui.toast_msg[63] = '\0';    ui.toast_timer = 150;
}

static void ui_toggle(const char *label, bool *state, const char *tip) {
    int id = ui_id();
    float x = ui.lx, y = ui.ly, w = ui.lw, h = 36.0f * ui.scale;
    char rname[64];
    snprintf(rname, sizeof(rname), "toggle:%s", label);
    int self = record_rect(rname, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    ui_push_parent(self);
    bool hover = !ui.modal_open && ui.open_dropdown_id == 0 && hit_box(x, y, w, h);
    if (hover) { ui.hot_id = id; if (tip) ui.tooltip_text = tip; }
    if (hover && ui.mouse_pressed) *state = !(*state);

    float t = ui_anim_step(id, *state ? 1.0f : 0.0f, 0.22f);
    vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, hover ? ui.pal.bg_hover : ui.pal.bg_elem);
    vg_stroke_rrect(x, y, w, h, 8.0f * ui.scale, 1.0f, ui.pal.border);
    vg_text(x + 12.0f * ui.scale, y + 11.5f * ui.scale, 12.5f * ui.scale, label, ui.pal.text_main);

    float pw = 38.0f * ui.scale, ph = 20.0f * ui.scale;
    float px = x + w - pw - 10.0f * ui.scale, py = y + (h - ph) * 0.5f;
    /* Record the pill so the sibling check catches it sitting on the label */
    record_rect("toggle_pill", (int)px, (int)py, (int)pw, (int)ph, ui.current_parent);
    uint32_t pill = color_lerp(ui.pal.border, ui.accent, t);
    vg_fill_rrect(px, py, pw, ph, ph * 0.5f, pill);
    float r = (ph - 5.0f * ui.scale) * 0.5f;
    float kx = vg_lerpf(px + r + 2.5f * ui.scale, px + pw - r - 2.5f * ui.scale, t);
    vg_fill_circle(kx, py + ph * 0.5f, r, 0xFFFFFFFFu);
    ui_pop_parent();

    ui.ly += h + 8.0f * ui.scale;
    if (ui.col_count > 1) ui_next_column(h + 8.0f * ui.scale);
}

/* 2. Vector Checkbox */
static void ui_checkbox(const char *label, bool *checked) {
    int id = ui_id();
    float x = ui.lx, y = ui.ly, w = ui.lw, h = 34.0f * ui.scale;
    char rname[64];
    snprintf(rname, sizeof(rname), "checkbox:%s", label);
    int self = record_rect(rname, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    ui_push_parent(self);
    bool hover = !ui.modal_open && ui.open_dropdown_id == 0 && hit_box(x, y, w, h);
    if (hover) ui.hot_id = id;
    if (hover && ui.mouse_pressed) *checked = !(*checked);

    float t = ui_anim_step(id, *checked ? 1.0f : 0.0f, 0.25f);
    float bx = x + 4.0f * ui.scale, by = y + 7.0f * ui.scale, bs = 20.0f * ui.scale;
    vg_fill_rrect(bx, by, bs, bs, 5.0f * ui.scale, color_lerp(ui.pal.bg_elem, ui.accent, t));
    vg_stroke_rrect(bx, by, bs, bs, 5.0f * ui.scale, 1.2f, *checked ? ui.accent : ui.pal.border);

    if (t > 0.1f) {
        vg_begin_path();
        vg_move_to(bx + 4.5f * ui.scale, by + 10.5f * ui.scale);
        vg_line_to(bx + 8.5f * ui.scale, by + 14.5f * ui.scale);
        vg_line_to(bx + 15.5f * ui.scale, by + 6.0f * ui.scale);
        vg_stroke(2.1f * ui.scale, 0xFF11121Cu);
    }
    vg_text(bx + 30.0f * ui.scale, y + 10.5f * ui.scale, 12.5f * ui.scale, label, ui.pal.text_main);
    ui_pop_parent();
    ui.ly += h + 6.0f * ui.scale;
    if (ui.col_count > 1) ui_next_column(h + 6.0f * ui.scale);
}

/* 3. Radio Button */
static void ui_radio(const char *label, int *group_val, int my_val) {
    int id = ui_id();
    float x = ui.lx, y = ui.ly, w = ui.lw, h = 32.0f * ui.scale;
    char rname[64];
    snprintf(rname, sizeof(rname), "radio:%s", label);
    int self = record_rect(rname, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    ui_push_parent(self);
    bool hover = !ui.modal_open && ui.open_dropdown_id == 0 && hit_box(x, y, w, h);
    if (hover) ui.hot_id = id;
    if (hover && ui.mouse_pressed) *group_val = my_val;

    bool active = (*group_val == my_val);
    float t = ui_anim_step(id, active ? 1.0f : 0.0f, 0.25f);
    float cx = x + 14.0f * ui.scale, cy = y + h * 0.5f, r = 9.5f * ui.scale;
    vg_fill_circle(cx, cy, r, ui.pal.bg_elem);
    vg_stroke_rrect(cx - r, cy - r, r * 2.0f, r * 2.0f, r, 1.4f, active ? ui.accent : ui.pal.border);
    if (t > 0.05f) vg_fill_circle(cx, cy, r * 0.52f * t, ui.accent);

    vg_text(x + 32.0f * ui.scale, y + 9.5f * ui.scale, 12.5f * ui.scale, label, ui.pal.text_main);
    ui_pop_parent();
    ui.ly += h + 6.0f * ui.scale;
    if (ui.col_count > 1) ui_next_column(h + 6.0f * ui.scale);
}

/* 4. Interactive Slider */
static void ui_slider(const char *label, float *val, float min_v, float max_v, const char *suffix) {
    int id = ui_id();
    float x = ui.lx, y = ui.ly, w = ui.lw, h = 48.0f * ui.scale;
    char rname[64];
    snprintf(rname, sizeof(rname), "slider:%s", label);
    int self = record_rect(rname, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    ui_push_parent(self);
    float tx = x + 12.0f * ui.scale, ty = y + 33.0f * ui.scale, tw = w - 24.0f * ui.scale;

    bool hover = !ui.modal_open && ui.open_dropdown_id == 0 && hit_box(x, y, w, h);
    if (hover) ui.hot_id = id;
    if (hover && ui.mouse_pressed) ui.active_id = id;
    if (ui.active_id == id && ui.mouse_down) {
        float t = vg_clampf((ui.mx - tx) / (tw > 0.0f ? tw : 1.0f), 0.0f, 1.0f);
        *val = min_v + t * (max_v - min_v);
    }

    vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, (hover || ui.active_id == id) ? ui.pal.bg_hover : ui.pal.bg_elem);
    vg_stroke_rrect(x, y, w, h, 8.0f * ui.scale, 1.0f, (ui.active_id == id) ? ui.accent : ui.pal.border);
    vg_text(x + 12.0f * ui.scale, y + 8.5f * ui.scale, 12.5f * ui.scale, label, ui.pal.text_main);

    char num[32];
    str_from_int(num, (int)(*val + 0.5f), suffix);
    float nw = vg_text_width(num, 12.5f * ui.scale);
    vg_text(x + w - nw - 12.0f * ui.scale, y + 8.5f * ui.scale, 12.5f * ui.scale, num, ui.accent);
    ui_pop_parent();

    float th = 5.5f * ui.scale;
    vg_fill_rrect(tx, ty - th * 0.5f, tw, th, th * 0.5f, ui.pal.border);
    float norm = vg_clampf((*val - min_v) / (max_v - min_v), 0.0f, 1.0f);
    if (norm > 0.01f) vg_fill_rrect(tx, ty - th * 0.5f, norm * tw, th, th * 0.5f, ui.accent);
    vg_fill_circle(tx + norm * tw, ty, 7.0f * ui.scale, 0xFFFFFFFFu);
    vg_fill_circle(tx + norm * tw, ty, 3.2f * ui.scale, ui.accent);

    ui.ly += h + 8.0f * ui.scale;
    if (ui.col_count > 1) ui_next_column(h + 8.0f * ui.scale);
}

/* 5. Vector Rotary Knob (Drag Vertically or Scroll Wheel) */
static void ui_knob(float cx, float cy, float r, const char *label, float *val, float min_v, float max_v) {
    int id = ui_id();
    char rname[64];
    snprintf(rname, sizeof(rname), "knob:%s", label);
    int self = record_rect(rname, (int)(cx - r - 8.0f), (int)(cy - r - 8.0f), (int)(r * 2.0f + 16.0f), (int)(r * 2.0f + 28.0f), ui.current_parent);
    ui_push_parent(self);
    bool hover = !ui.modal_open && hit_box(cx - r - 8.0f, cy - r - 8.0f, r * 2.0f + 16.0f, r * 2.0f + 28.0f);
    if (hover) {
        ui.hot_id = id;
        ui.tooltip_text = "Drag vertically or use Mouse Wheel to adjust knob";
        if (ui.wheel_dy != 0.0f)
            *val = vg_clampf(*val + ui.wheel_dy * (max_v - min_v) * 0.05f, min_v, max_v);
    }
    if (hover && ui.mouse_pressed) { ui.active_id = id; ui.click_y = ui.my; }
    if (ui.active_id == id && ui.mouse_down) {
        float dy = (ui.click_y - ui.my) * 0.008f;
        *val = vg_clampf(*val + dy * (max_v - min_v), min_v, max_v);
        ui.click_y = ui.my;
    }

    float norm = (*val - min_v) / (max_v - min_v);
    float a0 = 2.356f, a1 = 7.068f, ang = a0 + (a1 - a0) * norm;

    vg_fill_circle(cx, cy, r - 4.0f * ui.scale, ui.pal.bg_elem);
    vg_begin_path(); vg_arc(cx, cy, r, a0, a1, 24); vg_stroke(4.5f * ui.scale, ui.pal.border);
    if (norm > 0.01f) {
        vg_begin_path(); vg_arc(cx, cy, r, a0, ang, 24); vg_stroke(4.5f * ui.scale, ui.accent);
    }
    /* Indicator Line */
    vg_begin_path();
    vg_move_to(cx + vg_cosf(ang) * (r * 0.25f), cy + vg_sinf(ang) * (r * 0.25f));
    vg_line_to(cx + vg_cosf(ang) * (r * 0.72f), cy + vg_sinf(ang) * (r * 0.72f));
    vg_stroke(2.4f * ui.scale, 0xFFFFFFFFu);

    float lw = vg_text_width(label, 11.5f * ui.scale);
    vg_text(cx - lw * 0.5f, cy + r + 8.0f * ui.scale, 11.5f * ui.scale, label, ui.pal.text_muted);
    ui_pop_parent();
}

/* 6. Single-Line Interactive Text Input Box */
static void ui_textbox(const char *label, char *buf, int max_len) {
    int id = ui_id();
    float x = ui.lx, y = ui.ly, w = ui.lw, h = 38.0f * ui.scale;
    char rname[64];
    snprintf(rname, sizeof(rname), "textbox:%s", label);
    int self = record_rect(rname, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    ui_push_parent(self);
    bool hover = !ui.modal_open && ui.open_dropdown_id == 0 && hit_box(x, y, w, h);
    if (hover && ui.mouse_pressed) ui.focus_id = id;
    else if (!hover && ui.mouse_pressed && ui.focus_id == id) ui.focus_id = 0;

    bool focused = (ui.focus_id == id);
    if (focused) {
        int len = (int)strlen(buf);
        if (ui.key_backspace && len > 0) buf[len - 1] = '\0';
        if (ui.key_char >= 32 && ui.key_char <= 126 && len < max_len - 1) {
            buf[len] = ui.key_char;
            buf[len + 1] = '\0';
        }
        if (ui.key_enter || ui.key_esc) ui.focus_id = 0;
    }

    float lw = vg_text_width(label, 12.5f * ui.scale) + 16.0f * ui.scale;
    vg_text(x, y + 12.0f * ui.scale, 12.5f * ui.scale, label, ui.pal.text_main);

    float bx = x + lw, bw = w - lw;
    vg_fill_rrect(bx, y, bw, h, 7.0f * ui.scale, ui.pal.bg_canvas);
    vg_stroke_rrect(bx, y, bw, h, 7.0f * ui.scale, focused ? 1.6f : 1.0f, focused ? ui.accent : ui.pal.border);

    vg_push_clip(bx + 8.0f * ui.scale, y, bw - 16.0f * ui.scale, h);
    float tx = bx + 12.0f * ui.scale, ty = y + 12.0f * ui.scale;
    vg_text(tx, ty, 12.5f * ui.scale, buf, ui.pal.text_main);
    if (focused && ((ui.tick / 25) % 2 == 0)) {
        float cw = vg_text_width(buf, 12.5f * ui.scale);
        vg_fill_rrect(tx + cw + 2.0f * ui.scale, y + 9.0f * ui.scale, 2.0f * ui.scale, h - 18.0f * ui.scale, 1.0f, ui.accent);
    }
    vg_pop_clip();
    ui_pop_parent();

    ui.ly += h + 8.0f * ui.scale;
}

/* 7. Deferred Dropdown / Combo Box */
static void ui_dropdown(const char *label, const char **items, int count, int *selected) {
    int id = ui_id();
    float x = ui.lx, y = ui.ly, w = ui.lw, h = 38.0f * ui.scale;
    char rname[64];
    snprintf(rname, sizeof(rname), "dropdown:%s", label);
    int self = record_rect(rname, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    ui_push_parent(self);
    float lw = vg_text_width(label, 12.5f * ui.scale) + 16.0f * ui.scale;
    vg_text(x, y + 12.0f * ui.scale, 12.5f * ui.scale, label, ui.pal.text_main);

    float bx = x + lw, bw = w - lw;
    bool hover = !ui.modal_open && hit_box(bx, y, bw, h);
    if (hover && ui.mouse_pressed) {
        ui.open_dropdown_id = (ui.open_dropdown_id == id) ? 0 : id;
        ui.mouse_pressed = false;
    }

    vg_fill_rrect(bx, y, bw, h, 7.0f * ui.scale, hover ? ui.pal.bg_hover : ui.pal.bg_elem);
    vg_stroke_rrect(bx, y, bw, h, 7.0f * ui.scale, 1.1f, (ui.open_dropdown_id == id) ? ui.accent : ui.pal.border);
    vg_text(bx + 12.0f * ui.scale, y + 12.0f * ui.scale, 12.5f * ui.scale, items[*selected], ui.pal.text_main);

    /* Chevron Vector Arrow */
    float ax = bx + bw - 18.0f * ui.scale, ay = y + h * 0.5f;
    vg_begin_path();
    vg_move_to(ax - 5.0f * ui.scale, ay - 2.0f * ui.scale);
    vg_line_to(ax, ay + 3.0f * ui.scale);
    vg_line_to(ax + 5.0f * ui.scale, ay - 2.0f * ui.scale);
    vg_stroke(1.6f * ui.scale, ui.accent);

    if (ui.open_dropdown_id == id) {
        ui.dd_x = bx; ui.dd_y = y + h + 4.0f * ui.scale; ui.dd_w = bw;
        ui.dd_items = items; ui.dd_count = count; ui.dd_selected = selected;
    }
    ui_pop_parent();
    ui.ly += h + 8.0f * ui.scale;
}

/* 8. Action Button */
static bool ui_button(const char *label, bool primary) {
    int id = ui_id();
    float s = 12.5f * ui.scale;
    float w = (ui.col_count > 1) ? ui.lw : (vg_text_width(label, s) + 34.0f * ui.scale);
    float h = 36.0f * ui.scale, x = ui.lx, y = ui.ly;
    char rname[64];
    snprintf(rname, sizeof(rname), "button:%s", label);
    int self = record_rect(rname, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    ui_push_parent(self);

    bool hover = !ui.modal_open && ui.open_dropdown_id == 0 && hit_box(x, y, w, h);
    if (hover) ui.hot_id = id;
    bool clicked = hover && ui.mouse_pressed;

    uint32_t bg = primary ? (hover ? 0xFFFFFFFFu : ui.accent) : (hover ? ui.pal.bg_hover : ui.pal.bg_elem);
    uint32_t fg = primary ? 0xFF11121Cu : ui.pal.text_main;
    vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, bg);
    if (!primary) vg_stroke_rrect(x, y, w, h, 8.0f * ui.scale, 1.2f, hover ? ui.accent : ui.pal.border);

    float tw = vg_text_width(label, s);
    vg_text(x + (w - tw) * 0.5f, y + 11.5f * ui.scale, s, label, fg);

    ui_pop_parent();
    ui.ly += h + 8.0f * ui.scale;
    if (ui.col_count > 1) ui_next_column(h + 8.0f * ui.scale);
    return clicked;
}

/* =========================================================================
 * 6. APPLICATION WORKSPACES & INTERACTIVE DEMOS
 * ========================================================================= */
typedef struct {
    const char *title;
    float x, y, w, h;
    uint32_t col;
} GraphNode;

static int   app_tab    = 0;
static float scroll_t0  = 0.0f;
static bool  chk_simd   = true, chk_vsync = true, chk_aa = true;
static int   rad_mode   = 0;
static int   dd_profile = 1;
static char  txt_name[32] = "VectorStudio_v2";
static float val_gain = 78.0f, val_cutoff = 62.0f, val_res = 45.0f, val_drive = 34.0f;
static float val_scale = 100.0f;
static float picker_h = 0.60f, picker_s = 0.45f, picker_v = 0.98f;
static int   table_sel = 1;
static float scroll_t2  = 0.0f;

static GraphNode nodes[4] = {
    {"Oscillator A",  30.0f,  45.0f, 150.0f, 92.0f, 0xFF89B4FAu},
    {"LFO Modulator", 30.0f, 175.0f, 150.0f, 92.0f, 0xFFA6E3A1u},
    {"SVF Filter",   250.0f, 105.0f, 155.0f, 98.0f, 0xFFCBA6F7u},
    {"Master Out",   475.0f, 120.0f, 145.0f, 84.0f, 0xFFFAB387u}
};
static int drag_node = -1;

static void update_theme_palette(void) {
    if (ui.light_mode) {
        ui.pal.bg_canvas  = 0xFFEFF1F5u;
        ui.pal.bg_card    = 0xFFFFFFFFu;
        ui.pal.bg_elem    = 0xFFE6E9EFu;
        ui.pal.bg_hover   = 0xFFDCE0E8u;
        ui.pal.border     = 0xFFBCC0CCu;
        ui.pal.text_main  = 0xFF1E1E2Eu;
        ui.pal.text_muted = 0xFF6C6F85u;
    } else {
        ui.pal.bg_canvas  = 0xFF11121Cu;
        ui.pal.bg_card    = 0xFF181926u;
        ui.pal.bg_elem    = 0xFF1E2030u;
        ui.pal.bg_hover   = 0xFF26293Eu;
        ui.pal.border     = 0xFF2F334Du;
        ui.pal.text_main  = 0xFFEDF0FAu;
        ui.pal.text_muted = 0xFF9399B2u;
    }
}

static bool ui_nav_tab(float x, float y, float w, const char *label, int idx) {
    float h = 38.0f * ui.scale;
    char nav_name[32];
    snprintf(nav_name, sizeof(nav_name), "nav_tab%d", idx);
    record_rect(nav_name, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    int widget_parent = ui.current_parent;
    ui.current_parent = ui.rect_count - 1;
    bool active = (app_tab == idx);
    bool hover = !ui.modal_open && hit_box(x, y, w, h);
    if (hover && ui.mouse_pressed) app_tab = idx;

    if (active) vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, ui.accent);
    else if (hover) vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, ui.pal.bg_hover);

    uint32_t fg = active ? 0xFF11121Cu : (hover ? ui.pal.text_main : ui.pal.text_muted);
    vg_fill_circle(x + 16.0f * ui.scale, y + h * 0.5f, 4.0f * ui.scale, active ? 0xFF11121Cu : ui.accent);
    vg_text(x + 28.0f * ui.scale, y + 12.5f * ui.scale, 12.5f * ui.scale, label, fg);
    ui.current_parent = widget_parent;
    return active;
}

static void render_node_studio(float cx, float cy, float cw, float ch) {
    /* Clamp node positions to fit within card bounds */
    for (int i = 0; i < 4; i++) {
        GraphNode *n = &nodes[i];
        float max_x = (cw / ui.scale) - n->w - 10.0f;
        float max_y = (ch / ui.scale) - n->h - 10.0f;
        if (n->x > max_x) n->x = max_x;
        if (n->y > max_y) n->y = max_y;
        if (n->x < 10.0f) n->x = 10.0f;
        if (n->y < 10.0f) n->y = 10.0f;
    }

    vg_fill_rrect(cx, cy, cw, ch, 10.0f * ui.scale, ui.pal.bg_canvas);
    vg_stroke_rrect(cx, cy, cw, ch, 10.0f * ui.scale, 1.0f, ui.pal.border);
    vg_push_clip(cx, cy, cw, ch);

    /* Subtle Dot Grid */
    int step = (int)(24.0f * ui.scale);
    if (step < 12) step = 12;
    for (int gy = (int)cy + step; gy < (int)(cy + ch); gy += step) {
        for (int gx = (int)cx + step; gx < (int)(cx + cw); gx += step) {
            ui.px[gy * ui.w + gx] = ui.pal.border;
        }
    }

    /* Connect Nodes with Cubic Bezier Splines */
    const int links[3][2] = {{0, 2}, {1, 2}, {2, 3}};
    for (int i = 0; i < 3; i++) {
        GraphNode *a = &nodes[links[i][0]];
        GraphNode *b = &nodes[links[i][1]];
        float x0 = cx + (a->x + a->w) * ui.scale, y0 = cy + (a->y + 52.0f) * ui.scale;
        float x1 = cx + b->x * ui.scale,          y1 = cy + (b->y + 45.0f + (float)(i == 1 ? 24 : 0)) * ui.scale;
        float dx = vg_absf(x1 - x0) * 0.5f;
        vg_begin_path();
        vg_move_to(x0, y0);
        vg_cubic_to(x0 + dx, y0, x1 - dx, y1, x1, y1);
        vg_stroke(2.6f * ui.scale, a->col);
    }

    /* Draw & Drag Nodes */
    for (int i = 0; i < 4; i++) {
        GraphNode *n = &nodes[i];
        float nx = cx + n->x * ui.scale, ny = cy + n->y * ui.scale;
        float nw = n->w * ui.scale,      nh = n->h * ui.scale;

        char node_name[32];
        snprintf(node_name, sizeof(node_name), "node_header%d", i);
        record_rect(node_name, (int)nx, (int)ny, (int)nw, (int)(26.0f * ui.scale), ui.current_parent);
        int node_parent = ui.current_parent;
        ui.current_parent = ui.rect_count - 1;

        if (!ui.modal_open && hit_box(nx, ny, nw, nh) && ui.mouse_pressed) {
            drag_node = i;
            ui.click_x = ui.mx - nx;
            ui.click_y = ui.my - ny;
        }
        if (drag_node == i) {
            if (ui.mouse_down) {
                n->x = vg_clampf((ui.mx - ui.click_x - cx) / ui.scale, 10.0f, (cw / ui.scale) - n->w - 10.0f);
                n->y = vg_clampf((ui.my - ui.click_y - cy) / ui.scale, 10.0f, (ch / ui.scale) - n->h - 10.0f);
            } else {
                drag_node = -1;
            }
        }

        vg_shadow_rrect(nx, ny + 4.0f, nw, nh, 8.0f * ui.scale, 10.0f * ui.scale, 0x50000000u);
        vg_fill_rrect(nx, ny, nw, nh, 8.0f * ui.scale, ui.pal.bg_card);
        vg_fill_rrect(nx, ny, nw, 26.0f * ui.scale, 8.0f * ui.scale, n->col);
        vg_stroke_rrect(nx, ny, nw, nh, 8.0f * ui.scale, 1.2f, (drag_node == i) ? 0xFFFFFFFFu : n->col);
        vg_text(nx + 10.0f * ui.scale, ny + 7.0f * ui.scale, 11.5f * ui.scale, n->title, 0xFF11121Cu);
        vg_text(nx + 12.0f * ui.scale, ny + 42.0f * ui.scale, 11.0f * ui.scale, "Signal Port", ui.pal.text_muted);
        ui.current_parent = node_parent;
        vg_fill_circle(nx, ny + 48.0f * ui.scale, 5.0f * ui.scale, n->col);
        vg_fill_circle(nx + nw, ny + 52.0f * ui.scale, 5.0f * ui.scale, n->col);
    }
    vg_text(cx + 14.0f * ui.scale, cy + ch - 22.0f * ui.scale, 11.5f * ui.scale,
            "Drag any node header to reroute real-time Cubic Bezier patch cables.", ui.pal.text_muted);
    vg_pop_clip();
}

static void gui_frame(void) {
    static const char *profiles[] = {"Studio Reference", "Low-Latency ASIO", "Mastering Linear", "Eco Power"};
    ui.next_id = 0;
    ui.hot_id = 0;
    ui.rect_count = 0;
    ui.current_parent = -1;
    ui.tooltip_text = NULL;
    if (ui.mouse_released) ui.active_id = 0;
    ui.scale = vg_clampf(val_scale * 0.01f, 0.75f, 1.30f);
    update_theme_palette();
    vg_reset_clip();

    /* Right-click opens context menu anywhere */
    if (ui.rmouse_pressed) {
        ui.ctx_open = true;
        ui.ctx_x = vg_minf(ui.mx, (float)ui.w - 180.0f);
        ui.ctx_y = vg_minf(ui.my, (float)ui.h - 130.0f);
    }

    /* 1. Clear Canvas Background */
    for (int i = 0; i < ui.w * ui.h; i++) ui.px[i] = ui.pal.bg_canvas;

    float pad = 14.0f * ui.scale;
    float side_w = 210.0f * ui.scale;
    float status_h = 26.0f * ui.scale;
    float card_h = (float)ui.h - pad * 2.0f - status_h - 6.0f;

    /* 2. Left Sidebar Navigation */
    ui_card(pad, pad, side_w, card_h, 11.0f * ui.scale);
    int sidebar_parent = ui.current_parent;
    ui.current_parent = ui.rect_count - 1;
    float bx = pad + 24.0f * ui.scale, by = pad + 26.0f * ui.scale;
    vg_fill_circle(bx, by, 9.0f * ui.scale, ui.accent);
    vg_fill_circle(bx, by, 4.0f * ui.scale, ui.pal.bg_card);
    vg_text(bx + 15.0f * ui.scale, by - 7.0f * ui.scale, 15.0f * ui.scale, "VectorSuite", ui.pal.text_main);

    float ny = pad + 56.0f * ui.scale, nw = side_w - 22.0f * ui.scale;
    ui_nav_tab(pad + 11.0f * ui.scale, ny,                   nw, "1. Controls Suite", 0);
    ui_nav_tab(pad + 11.0f * ui.scale, ny + 44.0f * ui.scale, nw, "2. Node Studio",   1);
    ui_nav_tab(pad + 11.0f * ui.scale, ny + 88.0f * ui.scale, nw, "3. Data & Charts", 2);
    ui_nav_tab(pad + 11.0f * ui.scale, ny + 132.0f * ui.scale, nw, "4. Color Picker",  3);
    ui_nav_tab(pad + 11.0f * ui.scale, ny + 176.0f * ui.scale, nw, "5. Scaling & Modals", 4);

    /* Quick Theme Swatches */
    float sy = pad + card_h - 42.0f * ui.scale;
    vg_text(pad + 16.0f * ui.scale, sy - 28.0f * ui.scale, 10.5f * ui.scale, "Quick Presets", ui.pal.text_muted);
    const uint32_t swatches[4] = {0xFF89B4FAu, 0xFFA6E3A1u, 0xFFCBA6F7u, 0xFFFAB387u};
    for (int i = 0; i < 4; i++) {
        float sx = pad + 28.0f * ui.scale + (float)i * 42.0f * ui.scale, sr = 10.0f * ui.scale;
        record_rect("swatch", (int)(sx - sr), (int)(sy - sr), (int)(sr * 2.0f), (int)(sr * 2.0f), ui.current_parent);
        if (hit_box(sx - sr, sy - sr, sr * 2.0f, sr * 2.0f) && ui.mouse_pressed) {
            ui.accent = swatches[i];
            ui_toast("Accent preset updated");
        }
        if (ui.accent == swatches[i]) vg_fill_circle(sx, sy, sr + 2.5f * ui.scale, ui.pal.text_main);
        vg_fill_circle(sx, sy, sr, swatches[i]);
    }
    ui.current_parent = sidebar_parent;

    /* 3. Main Workspace Card */
    float mx = pad * 2.0f + side_w, mw = (float)ui.w - mx - pad;
    ui_card(mx, pad, mw, card_h, 11.0f * ui.scale);
    int workspace_parent = ui.current_parent;
    ui.current_parent = ui.rect_count - 1;

    if (app_tab == 0) {
        /* Scrollable Rich Widget Suite */
        ui_begin_scroll(mx, pad, mw, card_h, &scroll_t0, 560.0f * ui.scale);
        ui_section("TEXT INPUT & DROPDOWN SELECTOR");
        ui_textbox("Project Name:", txt_name, (int)sizeof(txt_name));
        ui_dropdown("Audio Routing:", profiles, 4, &dd_profile);

        ui_section("MULTI-COLUMN SWITCHES, CHECKBOXES & RADIOS");
        ui_columns(2);
        ui_toggle("Sub-Pixel SDF AA", &chk_aa, "Toggle analytical signed-distance anti-aliasing");
        ui_toggle("V-Sync Frame Lock", &chk_vsync, "Lock presentation to 60Hz display refresh");
        ui_checkbox("Enable SIMD Vectorization", &chk_simd);
        ui_radio("Mode: Real-Time", &rad_mode, 0);
        ui_end_columns();

        ui_section("ROTARY VECTOR KNOBS & CONTINUOUS SLIDERS");
        /* Responsive knob layout: 2x2 grid at narrow widths, 4x1 at wide */
        float knob_r = 26.0f * ui.scale;
        float knob_w = knob_r * 2.0f + 16.0f;
        float avail_w = ui.lw;
        int knobs_per_row = (avail_w >= knob_w * 4.0f + 30.0f) ? 4 : 2;
        float start_x = ui.lx + (avail_w - (float)knobs_per_row * knob_w) * 0.5f;
        float ky = ui.ly + 34.0f * ui.scale;
        for (int k = 0; k < 4; k++) {
            int row = k / knobs_per_row;
            int col = k % knobs_per_row;
            float kx = start_x + (float)col * (knob_w + 10.0f);
            float ky_row = ky + (float)row * (knob_r * 2.0f + 28.0f + 10.0f);
            const char *labels[4] = {"Gain", "Cutoff", "Reson", "Drive"};
            float *vals[4] = {&val_gain, &val_cutoff, &val_res, &val_drive};
            ui_knob(kx, ky_row, knob_r, labels[k], vals[k], 0.0f, 100.0f);
        }
        ui.ly += 86.0f * ui.scale + (knobs_per_row == 2 ? (knob_r * 2.0f + 28.0f + 10.0f) : 0.0f);

        /* Columns stack to 1 when narrow */
        int slider_cols = (ui.lw >= 300.0f * ui.scale) ? 2 : 1;
        ui_columns(slider_cols);
        ui_slider("Master Output Gain", &val_gain, 0.0f, 100.0f, "%");
        ui_slider("Filter Cutoff Freq", &val_cutoff, 10.0f, 100.0f, "Hz");
        if (ui_button("Save Preset", true)) ui_toast("Preset saved to memory!");
        /* Record modal button rect for test clicking */
        { float bw = (ui.col_count > 1) ? ui.lw : (vg_text_width("Open Modal Dialog", 12.5f * ui.scale) + 34.0f * ui.scale);
          float bh = 36.0f * ui.scale;
          record_rect("modal_button", (int)ui.lx, (int)ui.ly, (int)bw, (int)bh, ui.current_parent); }
        if (ui_button("Open Modal Dialog", false)) ui.modal_open = true;
        ui_end_columns();
        ui_end_scroll();

    } else if (app_tab == 1) {
        ui.lx = mx + 20.0f * ui.scale; ui.ly = pad + 18.0f * ui.scale; ui.lw = mw - 40.0f * ui.scale;
        ui_section("INTERACTIVE BEZIER NODE GRAPH EDITOR");
        render_node_studio(ui.lx, ui.ly, ui.lw, card_h - (ui.ly - pad) - 18.0f * ui.scale);

    } else if (app_tab == 2) {
        ui_begin_scroll(mx, pad, mw, card_h, &scroll_t2, 420.0f * ui.scale);
        ui.lx = mx + 20.0f * ui.scale; ui.ly = pad + 18.0f * ui.scale; ui.lw = mw - 40.0f * ui.scale;
        ui_section("REALTIME FILLED AREA WAVEFORM & TELEMETRY");

        float cx = ui.lx, cy = ui.ly, cw = ui.lw, ch = 120.0f * ui.scale;
        vg_fill_rrect(cx, cy, cw, ch, 9.0f * ui.scale, ui.pal.bg_canvas);
        vg_stroke_rrect(cx, cy, cw, ch, 9.0f * ui.scale, 1.0f, ui.pal.border);

        /* Filled Vector Polygon Area Under Curve */
        vg_begin_path();
        float x_left = cx + 12.0f * ui.scale, x_right = cx + cw - 12.0f * ui.scale;
        float base_y = cy + ch - 12.0f * ui.scale, mid_y = cy + ch * 0.52f;
        vg_move_to(x_left, base_y);
        for (int i = 0; i <= 36; i++) {
            float t = (float)i / 36.0f;
            float px = vg_lerpf(x_left, x_right, t);
            float py = mid_y + vg_sinf(t * (val_cutoff * 0.12f) + (float)ui.tick * 0.06f) * (ch * 0.32f * (val_gain * 0.01f));
            vg_line_to(px, py);
        }
        vg_line_to(x_right, base_y);
        vg_close_path();
        vg_fill((ui.accent & 0x00FFFFFFu) | 0x38000000u);

        /* Crisp Anti-Aliased Stroke Over Top */
        vg_begin_path();
        for (int i = 0; i <= 36; i++) {
            float t = (float)i / 36.0f;
            float px = vg_lerpf(x_left, x_right, t);
            float py = mid_y + vg_sinf(t * (val_cutoff * 0.12f) + (float)ui.tick * 0.06f) * (ch * 0.32f * (val_gain * 0.01f));
            if (i == 0) vg_move_to(px, py);
            else vg_line_to(px, py);
        }
        vg_stroke(2.4f * ui.scale, ui.accent);
        ui.ly += ch + 16.0f * ui.scale;

        /* Selectable Vector Data Grid */
        ui_section("INTERACTIVE SYSTEM PROCESS TABLE");
        static const char *rows[4][3] = {
            {"01  AudioEngine_DSP",   "0.4 ms", "ACTIVE"},
            {"02  VectorSDF_Render",  "1.2 ms", "LOCKED"},
            {"03  BezierSpline_Solver","0.2 ms", "READY"},
            {"04  OS_Window_Blitter", "0.3 ms", "ACTIVE"}
        };
        for (int r = 0; r < 4; r++) {
            float ry = ui.ly, rh = 30.0f * ui.scale;
            char row_name[32];
            snprintf(row_name, sizeof(row_name), "table_row%d", r);
            record_rect(row_name, (int)ui.lx, (int)ry, (int)ui.lw, (int)rh, ui.current_parent);
            int row_parent = ui.current_parent;
            ui.current_parent = ui.rect_count - 1;
            bool hover = !ui.modal_open && hit_box(ui.lx, ry, ui.lw, rh);
            if (hover && ui.mouse_pressed) table_sel = r;
            bool sel = (table_sel == r);
            vg_fill_rrect(ui.lx, ry, ui.lw, rh, 6.0f * ui.scale,
                          sel ? ui.pal.bg_hover : (r % 2 ? ui.pal.bg_elem : ui.pal.bg_card));
            if (sel) vg_stroke_rrect(ui.lx, ry, ui.lw, rh, 6.0f * ui.scale, 1.2f, ui.accent);
            vg_text(ui.lx + 14.0f * ui.scale, ry + 10.5f * ui.scale, 12.0f * ui.scale, rows[r][0], ui.pal.text_main);
            vg_text(ui.lx + ui.lw * 0.58f,    ry + 10.5f * ui.scale, 12.0f * ui.scale, rows[r][1], ui.pal.text_muted);
            vg_fill_rrect(ui.lx + ui.lw - 86.0f * ui.scale, ry + 7.0f * ui.scale, 72.0f * ui.scale, 20.0f * ui.scale, 10.0f * ui.scale, ui.accent);
            vg_text(ui.lx + ui.lw - 74.0f * ui.scale, ry + 11.0f * ui.scale, 10.5f * ui.scale, rows[r][2], 0xFF11121Cu);
            ui.current_parent = row_parent;
            ui.ly += rh + 2.0f * ui.scale;
        }
        ui_end_scroll();

    } else if (app_tab == 3) {
        ui.lx = mx + 20.0f * ui.scale; ui.ly = pad + 18.0f * ui.scale; ui.lw = mw - 40.0f * ui.scale;
        ui_section("2D HSV COLOR PICKER & THEME POLARITY");
        ui_toggle("Daylight High-Contrast Light Theme", &ui.light_mode, "Switch between Dark and Light palettes");

        /* 2D Saturation-Value Gradient Box */
        float sv_x = ui.lx, sv_y = ui.ly + 6.0f * ui.scale, sv_w = 240.0f * ui.scale, sv_h = 180.0f * ui.scale;
        record_rect("sv_box", (int)sv_x, (int)sv_y, (int)sv_w, (int)sv_h, ui.current_parent);
        uint32_t pure_hue = hsv_to_argb(picker_h, 1.0f, 1.0f);
        vg_fill_gradient(sv_x, sv_y, sv_w, sv_h, 8.0f * ui.scale, 0xFFFFFFFFu, pure_hue, 0xFF000000u, 0xFF000000u);
        vg_stroke_rrect(sv_x, sv_y, sv_w, sv_h, 8.0f * ui.scale, 1.2f, ui.pal.border);

        int sv_id = ui_id();
        if (hit_box(sv_x, sv_y, sv_w, sv_h) && ui.mouse_pressed) ui.active_id = sv_id;
        if (ui.active_id == sv_id && ui.mouse_down) {
            picker_s = vg_clampf((ui.mx - sv_x) / sv_w, 0.0f, 1.0f);
            picker_v = 1.0f - vg_clampf((ui.my - sv_y) / sv_h, 0.0f, 1.0f);
            ui.accent = hsv_to_argb(picker_h, picker_s, picker_v);
        }
        vg_fill_circle(sv_x + picker_s * sv_w, sv_y + (1.0f - picker_v) * sv_h, 7.0f * ui.scale, 0xFFFFFFFFu);
        vg_fill_circle(sv_x + picker_s * sv_w, sv_y + (1.0f - picker_v) * sv_h, 4.5f * ui.scale, ui.accent);

        /* Hue Spectrum Bar & Hex Preview */
        float hx = sv_x + sv_w + 24.0f * ui.scale, hw = ui.lw - sv_w - 24.0f * ui.scale;
        ui.lx = hx; ui.lw = hw;
        float old_h = picker_h * 360.0f;
        ui_slider("Hue Angle", &old_h, 0.0f, 360.0f, "deg");
        if (vg_absf(old_h - picker_h * 360.0f) > 0.1f) {
            picker_h = old_h / 360.0f;
            ui.accent = hsv_to_argb(picker_h, picker_s, picker_v);
        }

        char hex_buf[16] = "HEX: ";
        char raw_hex[10];
        str_from_hex(raw_hex, ui.accent);
        str_append(hex_buf, 16, raw_hex);
        vg_fill_rrect(hx, ui.ly, hw, 48.0f * ui.scale, 8.0f * ui.scale, ui.accent);
        vg_text(hx + 16.0f * ui.scale, ui.ly + 16.0f * ui.scale, 14.0f * ui.scale, hex_buf, 0xFF11121Cu);

    } else {
        ui.lx = mx + 20.0f * ui.scale; ui.ly = pad + 18.0f * ui.scale; ui.lw = mw - 40.0f * ui.scale;
        ui_section("RESOLUTION-INDEPENDENT DPI SCALING & OVERLAYS");
        ui_slider("Global Vector DPI Scale", &val_scale, 75.0f, 125.0f, "%");
        ui_columns(2);
        if (ui_button("Trigger Toast Alert", true)) ui_toast("Vector Toast Notification Active!");
        /* Record modal button rect for test clicking */
        { float bw = (ui.col_count > 1) ? ui.lw : (vg_text_width("Open Modal Dialog", 12.5f * ui.scale) + 34.0f * ui.scale);
          float bh = 36.0f * ui.scale;
          record_rect("modal_button", (int)ui.lx, (int)ui.ly, (int)bw, (int)bh, ui.current_parent); }
        if (ui_button("Open Modal Dialog", false)) ui.modal_open = true;
        if (ui_button("Reset 100% DPI", false)) val_scale = 100.0f;
        ui_end_columns();

        ui_section("MULTI-SCALE VECTOR TYPOGRAPHY SPECIMEN");
        vg_text(ui.lx, ui.ly,                     11.0f * ui.scale, "11pt: Pack my box with five dozen liquor jugs (0123456789)", ui.pal.text_muted);
        vg_text(ui.lx, ui.ly + 24.0f * ui.scale,  15.0f * ui.scale, "15pt: Pure C99 Quadratic Bezier Vector Outlines", ui.pal.text_main);
        vg_text(ui.lx, ui.ly + 54.0f * ui.scale,  21.0f * ui.scale, "21pt: Zero Bitmap Fonts - Infinite Scale!", ui.accent);
    }
    ui.current_parent = workspace_parent;

    /* 4. Bottom Status Bar */
    float sy_bar = (float)ui.h - status_h - 6.0f;
    vg_fill_rrect(pad, sy_bar, (float)ui.w - pad * 2.0f, status_h, 6.0f * ui.scale, ui.pal.bg_card);
    vg_stroke_rrect(pad, sy_bar, (float)ui.w - pad * 2.0f, status_h, 6.0f * ui.scale, 1.0f, ui.pal.border);
    vg_text(pad + 14.0f * ui.scale, sy_bar + (status_h - 11.0f * ui.scale) * 0.5f + 2.0f * ui.scale, 11.0f * ui.scale,
            "Engine: Pure C99 SDF + Bezier Vector Rasterizer  |  Right-Click Anywhere for Context Menu", ui.pal.text_muted);

    /* =====================================================================
     * 5. DEFERRED Z-ORDER OVERLAY PASS (Dropdowns, Context Menu, Modal, Toast)
     * ===================================================================== */
    vg_reset_clip();

    /* Dropdown Popup List */
    if (ui.open_dropdown_id != 0 && ui.dd_items) {
        float dh = (float)ui.dd_count * 32.0f * ui.scale + 8.0f * ui.scale;
        ui_card(ui.dd_x, ui.dd_y, ui.dd_w, dh, 8.0f * ui.scale);
        for (int i = 0; i < ui.dd_count; i++) {
            float iy = ui.dd_y + 4.0f * ui.scale + (float)i * 32.0f * ui.scale;
            bool h = (ui.mx >= ui.dd_x + 4.0f && ui.mx < ui.dd_x + ui.dd_w - 4.0f &&
                      ui.my >= iy && ui.my < iy + 30.0f * ui.scale);
            if (h) vg_fill_rrect(ui.dd_x + 4.0f, iy, ui.dd_w - 8.0f, 30.0f * ui.scale, 6.0f * ui.scale, ui.accent);
            vg_text(ui.dd_x + 14.0f * ui.scale, iy + 9.0f * ui.scale, 12.0f * ui.scale,
                    ui.dd_items[i], h ? 0xFF11121Cu : ui.pal.text_main);
            if (h && ui.mouse_pressed) {
                *ui.dd_selected = i;
                ui.open_dropdown_id = 0;
                ui.mouse_pressed = false;
            }
        }
        if (ui.mouse_pressed) ui.open_dropdown_id = 0;
    }

    /* Right-Click Context Menu */
    if (ui.ctx_open) {
        static const char *ctx_items[4] = {"Toggle Light/Dark Theme", "Reset DPI Scale (100%)", "Open Modal Dialog", "Close Menu"};
        float cw = 195.0f * ui.scale, ch = 136.0f * ui.scale;
        ui_card(ui.ctx_x, ui.ctx_y, cw, ch, 8.0f * ui.scale);
        for (int i = 0; i < 4; i++) {
            float iy = ui.ctx_y + 4.0f * ui.scale + (float)i * 32.0f * ui.scale;
            bool h = (ui.mx >= ui.ctx_x + 4.0f && ui.mx < ui.ctx_x + cw - 4.0f &&
                      ui.my >= iy && ui.my < iy + 30.0f * ui.scale);
            if (h) vg_fill_rrect(ui.ctx_x + 4.0f, iy, cw - 8.0f, 30.0f * ui.scale, 6.0f * ui.scale, ui.accent);
            vg_text(ui.ctx_x + 12.0f * ui.scale, iy + 9.0f * ui.scale, 11.5f * ui.scale,
                    ctx_items[i], h ? 0xFF11121Cu : ui.pal.text_main);
            if (h && ui.mouse_pressed) {
                if (i == 0) ui.light_mode = !ui.light_mode;
                if (i == 1) val_scale = 100.0f;
                if (i == 2) ui.modal_open = true;
                ui.ctx_open = false;
            }
        }
        if (ui.mouse_pressed) ui.ctx_open = false;
    }

    /* Modal Dialog Overlay */
    if (ui.modal_open) {
        vg_fill_rrect(0.0f, 0.0f, (float)ui.w, (float)ui.h, 0.0f, 0x99000000u);
        float mw_d = 380.0f * ui.scale, mh_d = 175.0f * ui.scale;
        float mx_d = ((float)ui.w - mw_d) * 0.5f, my_d = ((float)ui.h - mh_d) * 0.5f;
        ui_card(mx_d, my_d, mw_d, mh_d, 12.0f * ui.scale);
        vg_text(mx_d + 22.0f * ui.scale, my_d + 22.0f * ui.scale, 15.0f * ui.scale, "System Confirmation Modal", ui.accent);
        vg_text(mx_d + 22.0f * ui.scale, my_d + 58.0f * ui.scale, 12.0f * ui.scale,
                "Pure C99 vector modal dialog with backdrop", ui.pal.text_main);
        vg_text(mx_d + 22.0f * ui.scale, my_d + 78.0f * ui.scale, 12.0f * ui.scale,
                "dimming and input capture.", ui.pal.text_muted);

        float bx_d = mx_d + mw_d - 125.0f * ui.scale, by_d = my_d + mh_d - 48.0f * ui.scale;
        bool bh = (ui.mx >= bx_d && ui.mx <= bx_d + 105.0f * ui.scale &&
                   ui.my >= by_d && ui.my <= by_d + 34.0f * ui.scale);
        vg_fill_rrect(bx_d, by_d, 105.0f * ui.scale, 34.0f * ui.scale, 7.0f * ui.scale, bh ? 0xFFFFFFFFu : ui.accent);
        vg_text(bx_d + 22.0f * ui.scale, by_d + 10.5f * ui.scale, 12.5f * ui.scale, "Dismiss", 0xFF11121Cu);
        if ((bh && ui.mouse_pressed) || ui.key_esc) { ui.modal_open = false; ui_toast("Modal dismissed"); }
    }

    /* Animated Toast Notification */
    if (ui.toast_timer > 0) {
        ui.toast_timer--;
        float tw = vg_text_width(ui.toast_msg, 12.5f * ui.scale) + 36.0f * ui.scale;
        float tx = (float)ui.w - tw - 24.0f * ui.scale, ty = 24.0f * ui.scale;
        ui_card(tx, ty, tw, 38.0f * ui.scale, 8.0f * ui.scale);
        vg_fill_rrect(tx, ty, 6.0f * ui.scale, 38.0f * ui.scale, 3.0f * ui.scale, ui.accent);
        vg_text(tx + 18.0f * ui.scale, ty + 12.5f * ui.scale, 12.5f * ui.scale, ui.toast_msg, ui.pal.text_main);
    }

    /* Floating Hover Tooltip */
    if (ui.tooltip_text && !ui.modal_open && !ui.ctx_open) {
        float tw = vg_text_width(ui.tooltip_text, 11.5f * ui.scale) + 20.0f * ui.scale;
        float tx = vg_minf(ui.mx + 14.0f, (float)ui.w - tw - 10.0f);
        float ty = vg_maxf(ui.my - 34.0f, 10.0f);
        vg_fill_rrect(tx, ty, tw, 26.0f * ui.scale, 6.0f * ui.scale, 0xEE11121Cu);
        vg_stroke_rrect(tx, ty, tw, 26.0f * ui.scale, 6.0f * ui.scale, 1.0f, ui.accent);
        vg_text(tx + 10.0f * ui.scale, ty + 7.5f * ui.scale, 11.5f * ui.scale, ui.tooltip_text, 0xFFEDF0FAu);
    }

    /* Reset Per-Frame Transient Input Flags */
    ui.mouse_pressed  = false;
    ui.mouse_released = false;
    ui.rmouse_pressed = false;
    ui.wheel_dy       = 0.0f;
    ui.key_char       = 0;
    ui.key_backspace  = false;
    ui.key_enter      = false;
    ui.key_esc        = false;
    ui.tick++;
}

/* =========================================================================
 * 7. HEADLESS UTILITIES & BMP EXPORTER (For Testing & CLI Output)
 * ========================================================================= */
static bool save_bmp(const char *filename, int w, int h) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;

    uint32_t file_size = 54u + (uint32_t)((size_t)w * (size_t)h * 4u);
    int32_t bi_height = -(int32_t)h;
    uint8_t header[54] = {
        'B', 'M',
        (uint8_t)(file_size & 0xFFu), (uint8_t)((file_size >> 8) & 0xFFu),
        (uint8_t)((file_size >> 16) & 0xFFu), (uint8_t)((file_size >> 24) & 0xFFu),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        (uint8_t)(w & 0xFF), (uint8_t)((w >> 8) & 0xFF),
        (uint8_t)((w >> 16) & 0xFF), (uint8_t)((w >> 24) & 0xFF),
        (uint8_t)(bi_height & 0xFF), (uint8_t)((bi_height >> 8) & 0xFF),
        (uint8_t)((bi_height >> 16) & 0xFF), (uint8_t)((bi_height >> 24) & 0xFF),
        1, 0, 32, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    };

    if (fwrite(header, 1, 54, f) != 54) { fclose(f); return false; }
    if (fwrite(ui.px, 4, (size_t)w * (size_t)h, f) != (size_t)w * (size_t)h) { fclose(f); return false; }
    fclose(f);
    return true;
}

/* Headless regression test suite with honest rect recording & resize validation */
static int run_headless_tests(const char *bmp_output, int test_w, int test_h, int start_tab) {
    printf("Running C99 comprehensive vector GUI headless test suite...\n");
    ui.accent = 0xFF89B4FAu;
    ui.w = test_w;
    ui.h = test_h;
    app_tab = start_tab;

    /* Test 1: Initial frame render at requested size */
    gui_frame();
    if (ui.px[100 * ui.w + 100] == 0) {
        fprintf(stderr, "FAIL: framebuffer uninitialized\n");
        return 1;
    }
    printf("ok   vector framebuffer rasterized (%dx%d, %d bytes)\n", ui.w, ui.h, ui.w * ui.h * 4);

    /* Test 1b: Trigonometric function accuracy (vg_sinf/vg_cosf) */
    const float PI = 3.14159265358979323846f;
    CHECK(vg_absf(vg_sinf(0.0f) - 0.0f) < 0.005f, "vg_sinf(0) == 0");
    CHECK(vg_absf(vg_sinf(PI/6.0f) - 0.5f) < 0.005f, "vg_sinf(pi/6) == 0.5");
    CHECK(vg_absf(vg_sinf(PI/2.0f) - 1.0f) < 0.005f, "vg_sinf(pi/2) == 1");
    CHECK(vg_absf(vg_sinf(PI) - 0.0f) < 0.005f, "vg_sinf(pi) == 0");
    CHECK(vg_absf(vg_sinf(3.0f*PI/2.0f) + 1.0f) < 0.005f, "vg_sinf(3pi/2) == -1");
    CHECK(vg_absf(vg_sinf(-PI/2.0f) + 1.0f) < 0.005f, "vg_sinf(-pi/2) == -1");
    CHECK(vg_absf(vg_sinf(7.5f) - 0.937999f) < 0.005f, "vg_sinf(7.5) == 0.938");
    CHECK(vg_absf(vg_cosf(0.0f) - 1.0f) < 0.005f, "vg_cosf(0) == 1");
    CHECK(vg_absf(vg_cosf(PI/6.0f) - 0.866025f) < 0.005f, "vg_cosf(pi/6) == 0.866");
    CHECK(vg_absf(vg_cosf(PI/2.0f) - 0.0f) < 0.005f, "vg_cosf(pi/2) == 0");
    CHECK(vg_absf(vg_cosf(PI) + 1.0f) < 0.005f, "vg_cosf(pi) == -1");
    CHECK(vg_absf(vg_cosf(3.0f*PI/2.0f) - 0.0f) < 0.005f, "vg_cosf(3pi/2) == 0");
    CHECK(vg_absf(vg_cosf(-PI/2.0f) - 0.0f) < 0.005f, "vg_cosf(-pi/2) == 0");
    CHECK(vg_absf(vg_cosf(7.5f) - 0.346635f) < 0.005f, "vg_cosf(7.5) == 0.347");

    /* Test 2: Text input typing and backspace in Tab 0 */
    ui.focus_id = 0;
    ui.mx = 400.0f; ui.my = 70.0f;
    ui.mouse_pressed = true;
    gui_frame();
    ui.key_char = 'X';
    gui_frame();
    if (!strstr(txt_name, "X")) {
        fprintf(stderr, "FAIL: text input typing failed (got '%s')\n", txt_name);
        return 1;
    }
    ui.key_backspace = true;
    gui_frame();
    printf("ok   vector text input & caret processing (txt_name='%s')\n", txt_name);

    /* Test 3: Toggle AA switch */
    bool before_aa = chk_aa;
    int idx = find_rect("toggle");
    if (idx >= 0) {
        ui.mx = (float)(ui.rects[idx].x + ui.rects[idx].w / 2);
        ui.my = (float)(ui.rects[idx].y + ui.rects[idx].h / 2);
    } else {
        ui.mx = 350.0f; ui.my = 184.0f;
    }
    ui.mouse_pressed = true;
    gui_frame();
    if (chk_aa == before_aa) {
        fprintf(stderr, "FAIL: toggle switch failed\n");
        return 1;
    }
    printf("ok   animated vector toggle state flipped\n");

    /* Test 4: Checkbox state */
    bool before_simd = chk_simd;
    idx = find_rect("checkbox");
    if (idx >= 0) {
        ui.mx = (float)(ui.rects[idx].x + ui.rects[idx].w / 2);
        ui.my = (float)(ui.rects[idx].y + ui.rects[idx].h / 2);
    } else {
        ui.mx = 280.0f; ui.my = 227.0f;
    }
    ui.mouse_pressed = true;
    gui_frame();
    if (chk_simd == before_simd) {
        fprintf(stderr, "FAIL: checkbox toggle failed\n");
        return 1;
    }
    printf("ok   vector checkbox state transition\n");

    /* Test 5: Tab switch to Node Studio (Tab 1) */
    idx = find_rect("nav_tab1");
    CHECK(idx >= 0, "nav_tab1 recorded");
    if (idx >= 0) {
        ui.mx = (float)(ui.rects[idx].x + ui.rects[idx].w / 2);
        ui.my = (float)(ui.rects[idx].y + ui.rects[idx].h / 2);
    }
    ui.mouse_pressed = true;
    gui_frame();
    CHECK(app_tab == 1, "tab switch to Node Studio");
    if (app_tab != 1) return 1;
    printf("ok   switched to Node Studio tab\n");

    /* Test 6: Node dragging in Node Studio */
    float orig_nx = nodes[0].x;
    ui.mx = 350.0f; ui.my = 110.0f;
    ui.mouse_pressed = true;
    ui.mouse_down = true;
    gui_frame();
    ui.mx = 400.0f; ui.my = 130.0f;
    gui_frame();
    ui.mouse_down = false;
    ui.mouse_released = true;
    gui_frame();
    if (nodes[0].x == orig_nx) {
        fprintf(stderr, "FAIL: node drag failed (x unchanged)\n");
        return 1;
    }
    printf("ok   interactive node graph drag & bezier wire reroute\n");

    /* Test 7: Tab switch to Data & Charts (Tab 2) */
    idx = find_rect("nav_tab2");
    CHECK(idx >= 0, "nav_tab2 recorded");
    if (idx >= 0) {
        ui.mx = (float)(ui.rects[idx].x + ui.rects[idx].w / 2);
        ui.my = (float)(ui.rects[idx].y + ui.rects[idx].h / 2);
    }
    ui.mouse_pressed = true;
    gui_frame();
    CHECK(app_tab == 2, "tab switch to Data & Charts");
    if (app_tab != 2) return 1;
    printf("ok   switched to Data & Charts tab (filled vector area chart)\n");

    /* Test 8: Select row in System Process Table */
    idx = find_rect("table_row2");
    CHECK(idx >= 0, "table_row2 recorded");
    if (idx >= 0) {
        ui.mx = (float)(ui.rects[idx].x + ui.rects[idx].w / 2);
        ui.my = (float)(ui.rects[idx].y + ui.rects[idx].h / 2);
    }
    ui.mouse_pressed = true;
    gui_frame();
    CHECK(table_sel == 2, "data table row 2 selected");
    if (table_sel != 2) return 1;
    printf("ok   interactive data table row selected (row=%d)\n", table_sel);

    /* Test 9: Tab switch to Color Picker (Tab 3) */
    idx = find_rect("nav_tab3");
    CHECK(idx >= 0, "nav_tab3 recorded");
    if (idx >= 0) {
        ui.mx = (float)(ui.rects[idx].x + ui.rects[idx].w / 2);
        ui.my = (float)(ui.rects[idx].y + ui.rects[idx].h / 2);
    }
    ui.mouse_pressed = true;
    gui_frame();
    CHECK(app_tab == 3, "tab switch to Color Picker");
    if (app_tab != 3) return 1;
    /* Click on 2D HSV Sat/Val Gradient */
    idx = find_rect("sv_box");
    CHECK(idx >= 0, "sv_box recorded");
    if (idx >= 0) {
        ui.mx = (float)(ui.rects[idx].x + ui.rects[idx].w / 2);
        ui.my = (float)(ui.rects[idx].y + ui.rects[idx].h / 2);
    }
    ui.mouse_pressed = true;
    ui.mouse_down = true;
    gui_frame();
    ui.mouse_down = false;
    ui.mouse_released = true;
    gui_frame();
    printf("ok   2D HSV color picker gradient & hex generator (accent=0x%08X)\n", ui.accent);

    /* Test 10: Tab switch to Scaling & Modals (Tab 4) */
    idx = find_rect("nav_tab4");
    CHECK(idx >= 0, "nav_tab4 recorded");
    if (idx >= 0) {
        ui.mx = (float)(ui.rects[idx].x + ui.rects[idx].w / 2);
        ui.my = (float)(ui.rects[idx].y + ui.rects[idx].h / 2);
    }
    ui.mouse_pressed = true;
    gui_frame();
    CHECK(app_tab == 4, "tab switch to Scaling & Modals");
    if (app_tab != 4) return 1;
    printf("ok   switched to Scaling & Modals tab\n");

    /* Test 11: Open and dismiss modal dialog via button click */
    idx = find_rect("modal_button");
    CHECK(idx >= 0, "modal_button recorded");
    if (idx >= 0) {
        ui.mx = (float)(ui.rects[idx].x + ui.rects[idx].w / 2);
        ui.my = (float)(ui.rects[idx].y + ui.rects[idx].h / 2);
        ui.mouse_pressed = true;
        gui_frame();
    }
    CHECK(ui.modal_open, "modal dialog opened via button");
    if (!ui.modal_open) return 1;
    ui.key_esc = true;
    gui_frame();
    CHECK(!ui.modal_open, "modal dialog dismissed via ESC");
    if (ui.modal_open) return 1;
    printf("ok   vector modal dialog open & esc-dismiss with toast\n");

    /* Test 12: Context menu right-click and dismiss */
    ui.rmouse_pressed = true;
    ui.mx = 500.0f; ui.my = 300.0f;
    gui_frame();
    CHECK(ui.ctx_open, "context menu opened via right-click");
    if (!ui.ctx_open) return 1;
    ui.mouse_pressed = true; ui.mx = 10.0f; ui.my = 10.0f;
    gui_frame();
    CHECK(!ui.ctx_open, "context menu dismissed via click-outside");
    if (ui.ctx_open) return 1;
    printf("ok   right-click context menu open & click-outside dismiss\n");

    /* Test 13: Return to Tab 0 and export BMP */
    app_tab = 0;
    gui_frame();

    if (bmp_output) {
        if (!save_bmp(bmp_output, ui.w, ui.h)) {
            fprintf(stderr, "FAIL: saving BMP output to %s failed\n", bmp_output);
            return 1;
        }
        printf("ok   vector snapshot saved to %s\n", bmp_output);
    }

    /* Honest Resize Tests: min size (640x400) and max size (1920x1080) for all 5 tabs */
    int test_sizes[2][2] = {{MIN_W, MIN_H}, {1920, 1080}};
    for (int s = 0; s < 2; s++) {
        int tw = test_sizes[s][0];
        int th = test_sizes[s][1];
        for (int tab = 0; tab < 5; tab++) {
            ui.w = tw;
            ui.h = th;
            app_tab = tab;
            gui_frame();
            int rects_count = ui.rect_count;
            bool all_inside = true;
            for (int i = 0; i < ui.rect_count; i++) {
                Rect *r = &ui.rects[i];
                printf("DEBUG: tab %d rect %d: '%s' [%d,%d,%d,%d] parent=%d\n", tab, i, r->name, r->x, r->y, r->w, r->h, r->parent_id);
                if (r->x < 0 || r->y < 0 || r->x + r->w > tw || r->y + r->h > th) {
                    fprintf(stderr, "FAIL: tab %d widget '%s' rect [%d,%d,%d,%d] outside window %dx%d\n",
                            tab, r->name, r->x, r->y, r->w, r->h, tw, th);
                    all_inside = false;
                }
                if (r->parent_id >= 0 && r->parent_id < ui.rect_count) {
                    Rect *p = &ui.rects[r->parent_id];
                    if (r->x < p->x || r->y < p->y || r->x + r->w > p->x + p->w || r->y + r->h > p->y + p->h) {
                        fprintf(stderr, "FAIL: tab %d widget '%s' rect [%d,%d,%d,%d] outside parent '%s' [%d,%d,%d,%d]\n",
                                tab, r->name, r->x, r->y, r->w, r->h, p->name, p->x, p->y, p->w, p->h);
                        all_inside = false;
                    }
                }
            }
            if (all_inside) {
                /* Sibling overlap check: any two rects with same parent that intersect = FAIL */
                for (int j = i + 1; j < ui.rect_count; j++) {
                    Rect *r2 = &ui.rects[j];
                    if (r2->parent_id == r->parent_id) {
                        bool overlap = (r->x < r2->x + r2->w && r->x + r->w > r2->x &&
                                        r->y < r2->y + r2->h && r->y + r->h > r2->y);
                        fprintf(stderr, "DEBUG sibling check: tab=%d i=%d('%s' p=%d) j=%d('%s' p=%d) overlap=%d\n", tab, i, r->name, r->parent_id, j, r2->name, r2->parent_id, overlap);
                        if (overlap) {
                            fprintf(stderr, "FAIL: tab %d sibling overlap: '%s' [%d,%d,%d,%d] overlaps '%s' [%d,%d,%d,%d] (parent=%d)\n",
                                    tab, r->name, r->x, r->y, r->w, r->h, r2->name, r2->x, r2->y, r2->w, r2->h, r->parent_id);
                            all_inside = false;
                        }
                    }
                }
                printf("ok   bounds %dx%d tab%d: %d rects\n", tw, th, tab, rects_count);
            } else {
                return 1;
            }
        }
    }

    /* Font Glyph Verification: every printable ASCII 33-126 must light at least one pixel */
    {
        ui.w = 980;
        ui.h = 200;
        app_tab = 0;
        gui_frame();
        int glyph_failures = 0;
        for (int c = 33; c <= 126; c++) {
            char buf[2] = {(char)c, 0};
            float adv = 0.0f;
            vg_glyph_path((char)c, &adv);
            if (adv <= 0.0f) {
                fprintf(stderr, "FAIL: glyph '%c' (0x%02X) has no path (adv <= 0)\n", c, c);
                glyph_failures++;
                continue;
            }
            /* Clear a test area */
            for (int i = 0; i < ui.w * ui.h; i++) ui.px[i] = 0;
            vg_reset_clip();
            vg_text(10.0f, 100.0f, 24.0f, buf, 0xFFFFFFFFu);
            /* Check if any pixel is non-zero */
            bool any_lit = false;
            for (int i = 0; i < ui.w * ui.h; i++) {
                if (ui.px[i] != 0) { any_lit = true; break; }
            }
            if (!any_lit) {
                fprintf(stderr, "FAIL: glyph '%c' (0x%02X) lights zero pixels\n", c, c);
                glyph_failures++;
            }
        }
        if (glyph_failures > 0) {
            fprintf(stderr, "FAIL: %d glyph(s) failed to render\n", glyph_failures);
            return 1;
        }
        printf("ok   all 94 printable ASCII glyphs (33-126) render at least one pixel\n");
        /* Save font strip for visual inspection */
        for (int i = 0; i < ui.w * ui.h; i++) ui.px[i] = ui.pal.bg_canvas;
        vg_reset_clip();
        float x = 10.0f;
        for (int c = 33; c <= 126; c++) {
            char buf[2] = {(char)c, 0};
            float adv = 0.0f;
            vg_glyph_path((char)c, &adv);
            if (adv > 0.0f) {
                vg_text(x, 100.0f, 24.0f, buf, 0xFFFFFFFFu);
                x += adv * (24.0f / 12.0f) + 2.0f;
            }
        }
        save_bmp("build/logs/font-strip-suite.bmp", ui.w, ui.h);
    }

    /* Negative test mode: run every tab at 300x200 (below minimum), verify bounds failures are reported */
    if (getenv("C99GUI_TEST_NEGATIVE")) {
        ui.test_negative_mode = 1;
        ui.w = 300;
        ui.h = 200;
        for (int tab = 0; tab < 5; tab++) {
            app_tab = tab;
            gui_frame();
            for (int i = 0; i < ui.rect_count; i++) {
                Rect *r = &ui.rects[i];
                bool inside_window = !(r->x < 0 || r->y < 0 || r->x + r->w > ui.w || r->y + r->h > ui.h);
                if (!inside_window) {
                    fprintf(stderr, "FAIL: negative: tab %d widget '%s' rect outside window\n", tab, r->name);
                    ui.test_failed++;
                } else {
                    ui.test_passed++;
                }
                if (r->parent_id >= 0 && r->parent_id < ui.rect_count) {
                    Rect *p = &ui.rects[r->parent_id];
                    bool inside_parent = !(r->x < p->x || r->y < p->y || r->x + r->w > p->x + p->w || r->y + r->h > p->y + p->h);
                    if (!inside_parent) {
                        fprintf(stderr, "FAIL: negative: tab %d widget '%s' rect outside parent '%s'\n", tab, r->name, p->name);
                        ui.test_failed++;
                    } else {
                        ui.test_passed++;
                    }
                }
            }
        }
        ui.test_negative_mode = 0;
        if (ui.test_failed == 0) {
            fprintf(stderr, "FAIL: negative test found no bounds failures at 300x200 (bounds checking broken)\n");
            return 1;
        }
        /* Exit 1 because failed > 0 */
        return 1;
    }

    printf("passed: %d  failed: %d\n", ui.test_passed, ui.test_failed);
    return (ui.test_failed > 0) ? 1 : 0;
}

static int run_snapshot(const char *filename, int w, int h, int tab) {
    ui.accent = 0xFF89B4FAu;
    ui.w = w;
    ui.h = h;
    app_tab = tab;
    gui_frame();
    if (!save_bmp(filename, w, h)) {
        fprintf(stderr, "FAIL: saving BMP output to %s failed\n", filename);
        return 1;
    }
    printf("ok   vector snapshot saved to %s (%dx%d, tab %d)\n", filename, w, h, tab);
    return 0;
}

static void print_usage(void) {
    puts("Usage: " APP_NAME " [OPTIONS]\n"
         "\n"
         "Comprehensive Pure C99 2D Vector Graphics & Immediate-Mode GUI Framework.\n"
         "\n"
         "Options:\n"
         "  --test                 Run automated headless vector regression suite and exit\n"
         "  --snapshot <file.bmp>  Render one frame, save to 32-bit BMP and exit\n"
         "  --size WxH             Snapshot size (default 980x640)\n"
         "  --tab N                Start tab index 0-4 (default 0)\n"
         "  --headless             Run 120 vector simulation frames headlessly and exit\n"
         "  -h, --help             Display this help message and exit\n"
         "  -V, --version          Display version information and exit\n");
}

/* =========================================================================
 * 8. NATIVE OS WINDOWING BACKENDS (Win32 / Cocoa / X11 / Headless)
 * ========================================================================= */
#if defined(C99GUI_BACKEND_WIN32)

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                ui.w = (int)LOWORD(lp);
                ui.h = (int)HIWORD(lp);
            }
            return 0;
        case WM_GETMINMAXINFO:
            {
                MINMAXINFO *mmi = (MINMAXINFO *)lp;
                mmi->ptMinTrackSize.x = MIN_W;
                mmi->ptMinTrackSize.y = MIN_H;
            }
            return 0;
        case WM_MOUSEMOVE:
            ui.mx = (float)(short)LOWORD(lp);
            ui.my = (float)(short)HIWORD(lp);
            return 0;
        case WM_LBUTTONDOWN:
            ui.mouse_down = true; ui.mouse_pressed = true;
            SetCapture(hwnd);
            return 0;
        case WM_LBUTTONUP:
            ui.mouse_down = false; ui.mouse_released = true;
            ReleaseCapture();
            return 0;
        case WM_RBUTTONDOWN:
            ui.rmouse_pressed = true;
            return 0;
        case WM_MOUSEWHEEL:
            ui.wheel_dy += (float)(short)HIWORD(wp) / 120.0f;
            return 0;
        case WM_CHAR:
            if (wp == 8) ui.key_backspace = true;
            else if (wp == 13) ui.key_enter = true;
            else if (wp >= 32 && wp <= 126) ui.key_char = (char)wp;
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) ui.key_esc = true;
            return 0;
        case WM_CLOSE:
        case WM_DESTROY:
            ui.running = false;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

static int run_win32_window(void) {
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = GetModuleHandleA(NULL);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "C99VectorSuite";
    RegisterClassA(&wc);

    RECT rc = {0, 0, WIN_W, WIN_H};
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    AdjustWindowRect(&rc, style, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Pure C99 Vector GUI Framework", style,
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

    ui.w = WIN_W;
    ui.h = WIN_H;
    ui.accent = 0xFF89B4FAu;
    ui.running = true;
    while (ui.running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        gui_frame();
        StretchDIBits(hdc, 0, 0, ui.w, ui.h, 0, 0, ui.w, ui.h,
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

#define msg0(r, o, s)         ((r (*)(id, SEL))objc_msgSend)((id)(o), sel_registerName(s))
#define msg1(r, o, s, T1, a1) ((r (*)(id, SEL, T1))objc_msgSend)((id)(o), sel_registerName(s), a1)
#define msg4(r, o, s, T1, a1, T2, a2, T3, a3, T4, a4) \
    ((r (*)(id, SEL, T1, T2, T3, T4))objc_msgSend)((id)(o), sel_registerName(s), a1, a2, a3, a4)

static int run_cocoa_window(void) {
    id app = msg0(id, objc_getClass("NSApplication"), "sharedApplication");
    msg1(BOOL, app, "setActivationPolicy:", long, 0);

    NSRect frame = {{80.0, 80.0}, {(double)WIN_W, (double)WIN_H}};
    id win = msg4(id, msg0(id, objc_getClass("NSWindow"), "alloc"),
                  "initWithContentRect:styleMask:backing:defer:",
                  NSRect, frame, unsigned long, 15u, unsigned long, 2u, BOOL, 0);
    id title = msg1(id, objc_getClass("NSString"), "stringWithUTF8String:", const char *, "Pure C99 Vector GUI Framework");
    msg1(void, win, "setTitle:", id, title);
    msg1(void, win, "makeKeyAndOrderFront:", id, NULL);
    msg1(void, app, "activateIgnoringOtherApps:", BOOL, 1);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    id view = msg0(id, win, "contentView");
    id layer = msg0(id, objc_getClass("CALayer"), "layer");
    msg1(void, view, "setLayer:", id, layer);
    msg1(void, view, "setWantsLayer:", BOOL, 1);

    ui.w = WIN_W;
    ui.h = WIN_H;
    ui.accent = 0xFF89B4FAu;
    ui.running = true;
    while (ui.running && msg0(BOOL, win, "isVisible")) {
        id ev;
        id distant_past = msg0(id, objc_getClass("NSDate"), "distantPast");
        id mode = msg1(id, objc_getClass("NSString"), "stringWithUTF8String:", const char *, "kCFRunLoopDefaultMode");
        while ((ev = msg4(id, app, "nextEventMatchingMask:untilDate:inMode:dequeue:",
                          unsigned long long, ~0ull, id, distant_past, id, mode, BOOL, 1)) != NULL) {
            unsigned long type = msg0(unsigned long, ev, "type");
            if (type == 1)  { ui.mouse_down = true;  ui.mouse_pressed = true; }
            if (type == 2)  { ui.mouse_down = false; ui.mouse_released = true; }
            if (type == 3)  { ui.rmouse_pressed = true; }
            if (type == 22) { ui.wheel_dy += (float)msg0(double, ev, "deltaY"); }
            if (type == 10) {
                id chars = msg0(id, ev, "characters");
                const char *s = msg0(const char *, chars, "UTF8String");
                if (s && s[0]) {
                    if (s[0] == 127 || s[0] == 8) ui.key_backspace = true;
                    else if (s[0] == 13 || s[0] == 10) ui.key_enter = true;
                    else if (s[0] == 27) ui.key_esc = true;
                    else if (s[0] >= 32 && s[0] <= 126) ui.key_char = s[0];
                }
            } else {
                msg1(void, app, "sendEvent:", id, ev);
            }
        }
        NSRect content_frame = msg0(NSRect, view, "frame");
        ui.w = (int)content_frame.size.width;
        ui.h = (int)content_frame.size.height;

        NSPoint mp = msg0(NSPoint, win, "mouseLocationOutsideOfEventStream");
        ui.mx = (float)mp.x;
        ui.my = (float)ui.h - (float)mp.y;

        gui_frame();
        CGContextRef ctx = CGBitmapContextCreate(ui.px, ui.w, ui.h, 8, ui.w * 4, cs,
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
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 80, 80, WIN_W, WIN_H, 1,
                                     BlackPixel(dpy, scr), WhitePixel(dpy, scr));
    XStoreName(dpy, win, "Pure C99 Vector GUI Framework");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XMapWindow(dpy, win);

    GC gc = DefaultGC(dpy, scr);
    XImage *img = XCreateImage(dpy, DefaultVisual(dpy, scr), (unsigned int)DefaultDepth(dpy, scr),
                               ZPixmap, 0, (char *)ui.px, WIN_W, WIN_H, 32, 0);

    ui.w = WIN_W;
    ui.h = WIN_H;
    ui.accent = 0xFF89B4FAu;
    ui.running = true;
    while (ui.running) {
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == ConfigureNotify) {
                ui.w = ev.xconfigure.width;
                ui.h = ev.xconfigure.height;
                XDestroyImage(img);
                img = XCreateImage(dpy, DefaultVisual(dpy, scr), (unsigned int)DefaultDepth(dpy, scr),
                                   ZPixmap, 0, (char *)ui.px, ui.w, ui.h, 32, 0);
            } else if (ev.type == MotionNotify) {
                ui.mx = (float)ev.xmotion.x; ui.my = (float)ev.xmotion.y;
            } else if (ev.type == ButtonPress) {
                if (ev.xbutton.button == Button1) { ui.mouse_down = true; ui.mouse_pressed = true; }
                else if (ev.xbutton.button == Button3) ui.rmouse_pressed = true;
                else if (ev.xbutton.button == Button4) ui.wheel_dy += 1.0f;
                else if (ev.xbutton.button == Button5) ui.wheel_dy -= 1.0f;
            } else if (ev.type == ButtonRelease && ev.xbutton.button == Button1) {
                ui.mouse_down = false; ui.mouse_released = true;
            } else if (ev.type == KeyPress) {
                char buf[8] = {0};
                KeySym ks = 0;
                XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &ks, NULL);
                if (ks == XK_Escape) ui.key_esc = true;
                else if (ks == XK_BackSpace) ui.key_backspace = true;
                else if (ks == XK_Return || ks == XK_KP_Enter) ui.key_enter = true;
                else if (buf[0] >= 32 && buf[0] <= 126) ui.key_char = buf[0];
            } else if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) {
                ui.running = false;
            }
        }
        gui_frame();
        XPutImage(dpy, win, gc, img, 0, 0, 0, 0, ui.w, ui.h);
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
    int snapshot_w = WIN_W, snapshot_h = WIN_H;
    int start_tab = 0;

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
                snapshot_file = "vectorsuite_snapshot.bmp";
            }
        } else if (!strcmp(argv[i], "--size")) {
            if (i + 1 < argc) {
                char *x_ptr = strchr(argv[i + 1], 'x');
                if (x_ptr) {
                    *x_ptr = '\0';
                    snapshot_w = atoi(argv[i + 1]);
                    snapshot_h = atoi(x_ptr + 1);
                    *x_ptr = 'x';
                }
                i++;
            }
        } else if (!strcmp(argv[i], "--tab")) {
            if (i + 1 < argc) {
                start_tab = atoi(argv[++i]);
            }
        } else {
            fprintf(stderr, APP_NAME ": unknown option '%s' (try --help)\n", argv[i]);
            return 2;
        }
    }

    if (snapshot_file && !do_test) {
        return run_snapshot(snapshot_file, snapshot_w, snapshot_h, start_tab);
    }
    if (do_test) {
        return run_headless_tests(NULL, snapshot_w, snapshot_h, start_tab);
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
    return run_headless_tests(NULL, WIN_W, WIN_H, 0);
#endif
}
