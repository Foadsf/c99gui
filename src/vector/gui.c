/*
 * gui.c - Canonical Pure C99 2D Vector Graphics & Immediate-Mode GUI Engine
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Foad Sojoodi Farimani
 *
 * Zero external libraries, zero -lm dependency.
 * Resolution-independent SDF and Bezier vector renderer:
 *   - Proportional SVG-style quadratic Bezier typography (M, L, Q)
 *   - Unified multi-segment vector path rasterizer (vg_line_to, vg_quad_to, vg_cubic_to, vg_arc)
 *   - Analytical Euclidean Signed Distance Fields (SDF) for rounded rects & smooth drop shadows
 *   - Real-time resolution-independent vector DPI scaling (0.75x - 1.30x)
 *   - Native OS windowing backends:
 *       Windows   : Win32 GDI (StretchDIBits)
 *       Linux/BSD : X11 (XPutImage) or Headless Software Framebuffer
 *       macOS     : Cocoa / CoreGraphics via C runtime (objc_msgSend)
 *       Headless  : Automated testing (--test) and 32-bit BMP export (--snapshot)
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
#define WIN_W       860
#define WIN_H       540
#define MIN_W       640
#define MIN_H       400
#define MAX_BUF_W   3840
#define MAX_BUF_H   2160
#define VG_PI       3.1415926535f

/* =========================================================================
 * 1. COMPILER/LIBM-AGNOSTIC FLOAT MATH (Zero -lm linker dependency)
 * ========================================================================= */
static float vg_absf(float x) { return x < 0.0f ? -x : x; }
static float vg_minf(float a, float b) { return a < b ? a : b; }
static float vg_maxf(float a, float b) { return a > b ? a : b; }
static float vg_clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }

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
    x *= 0.15915494f;
    x -= (float)(int)x;
    if (x < 0.0f) x += 1.0f;
    float y = (x < 0.5f) ? (x * 4.0f - 1.0f) : (3.0f - x * 4.0f);
    return y * (1.225f * (y * vg_absf(y) - y) + y);
}

static float vg_cosf(float x) { return vg_sinf(x + 1.57079632f); }

/* =========================================================================
 * 2. VECTOR GRAPHICS CONTEXT & SUB-PIXEL SDF RASTERIZER
 * ========================================================================= */
typedef struct {
    float x0, y0, vx, vy, inv_len2;
} VgSeg;

typedef struct {
    char name[64];
    int x, y, w, h, parent_id;
} Rect;

typedef struct {
    uint32_t px[MAX_BUF_W * MAX_BUF_H];
    VgSeg segs[128];
    int seg_count;
    float cur_x, cur_y;
    float min_x, min_y, max_x, max_y;

    /* Immediate-Mode GUI State */
    float mx, my;
    bool mouse_down, mouse_pressed, mouse_released;
    int active_id, hot_id, next_id;
    float scale;
    float lx, ly, lw;
    uint32_t accent;
    unsigned int tick;
    bool running;
    int w, h;

    /* Honest Test Framework: Widget Rect Recording */
    Rect rects[256];
    int rect_count;
    int current_parent;
    int test_negative_mode;
} GuiCtx;

static GuiCtx ui;

static void record_rect(const char *name, int x, int y, int w, int h, int parent_id) {
    if (ui.rect_count < 256) {
        strncpy(ui.rects[ui.rect_count].name, name, 63);
        ui.rects[ui.rect_count].name[63] = '\0';
        ui.rects[ui.rect_count].x = x;
        ui.rects[ui.rect_count].y = y;
        ui.rects[ui.rect_count].w = w;
        ui.rects[ui.rect_count].h = h;
        ui.rects[ui.rect_count].parent_id = parent_id;
        ui.rect_count++;
    }
}

static int find_rect(const char *name) {
    for (int i = 0; i < ui.rect_count; i++) {
        if (strcmp(ui.rects[i].name, name) == 0) return i;
    }
    return -1;
}

static uint32_t blend_argb(uint32_t dst, uint32_t src, float cov) {
    uint32_t a_cov = (uint32_t)(vg_clampf(cov, 0.0f, 1.0f) * 256.0f);
    uint32_t a = (((src >> 24) & 0xFFu) * a_cov) >> 8;
    if (a == 0) return dst;
    if (a >= 255u) return src | 0xFF000000u;
    uint32_t ia = 256u - a;
    uint32_t rb = ((((src & 0x00FF00FFu) * a) + ((dst & 0x00FF00FFu) * ia)) >> 8) & 0x00FF00FFu;
    uint32_t g  = ((((src & 0x0000FF00u) * a) + ((dst & 0x0000FF00u) * ia)) >> 8) & 0x0000FF00u;
    return 0xFF000000u | rb | g;
}

/* Analytical Euclidean Signed Distance to a Rounded Box */
static float sdf_rrect(float px, float py, float cx, float cy, float hw, float hh, float r) {
    float qx = vg_absf(px - cx) - (hw - r);
    float qy = vg_absf(py - cy) - (hh - r);
    float ox = vg_maxf(qx, 0.0f);
    float oy = vg_maxf(qy, 0.0f);
    float out_d = vg_sqrtf(ox * ox + oy * oy);
    float in_d  = vg_minf(vg_maxf(qx, qy), 0.0f);
    return out_d + in_d - r;
}

static void vg_fill_rrect(float x, float y, float w, float h, float r, uint32_t col) {
    float hw = w * 0.5f, hh = h * 0.5f;
    float cx = x + hw,   cy = y + hh;
    r = vg_minf(r, vg_minf(hw, hh));

    int x0 = (int)(x - 1.0f); if (x0 < 0) x0 = 0;
    int y0 = (int)(y - 1.0f); if (y0 < 0) y0 = 0;
    int x1 = (int)(x + w + 2.0f); if (x1 > ui.w) x1 = ui.w;
    int y1 = (int)(y + h + 2.0f); if (y1 > ui.h) y1 = ui.h;
    if (x0 >= x1 || y0 >= y1) return;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t *row = &ui.px[py * ui.w];
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            /* Fast interior bypass */
            if (fx > x + r && fx < x + w - r && fy > y + r && fy < y + h - r) {
                row[px] = blend_argb(row[px], col, 1.0f);
                continue;
            }
            float d = sdf_rrect(fx, fy, cx, cy, hw, hh, r);
            if (d < 0.5f) row[px] = blend_argb(row[px], col, 0.5f - d);
        }
    }
}

static void vg_stroke_rrect(float x, float y, float w, float h, float r, float sw, uint32_t col) {
    float hw = w * 0.5f, hh = h * 0.5f;
    float cx = x + hw,   cy = y + hh;
    float hr = sw * 0.5f;
    r = vg_minf(r, vg_minf(hw, hh));

    int x0 = (int)(x - sw - 2.0f); if (x0 < 0) x0 = 0;
    int y0 = (int)(y - sw - 2.0f); if (y0 < 0) y0 = 0;
    int x1 = (int)(x + w + sw + 2.0f); if (x1 > ui.w) x1 = ui.w;
    int y1 = (int)(y + h + sw + 2.0f); if (y1 > ui.h) y1 = ui.h;
    if (x0 >= x1 || y0 >= y1) return;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t *row = &ui.px[py * ui.w];
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            if (fx > x + r + sw && fx < x + w - r - sw && fy > y + r + sw && fy < y + h - r - sw)
                continue;
            float d = vg_absf(sdf_rrect(fx, fy, cx, cy, hw, hh, r)) - hr;
            if (d < 0.5f) row[px] = blend_argb(row[px], col, 0.5f - d);
        }
    }
}

static void vg_shadow_rrect(float x, float y, float w, float h, float r, float blur, uint32_t col) {
    float hw = w * 0.5f, hh = h * 0.5f;
    float cx = x + hw,   cy = y + hh;
    int x0 = (int)(x - blur); if (x0 < 0) x0 = 0;
    int y0 = (int)(y - blur); if (y0 < 0) y0 = 0;
    int x1 = (int)(x + w + blur); if (x1 > ui.w) x1 = ui.w;
    int y1 = (int)(y + h + blur); if (y1 > ui.h) y1 = ui.h;
    if (x0 >= x1 || y0 >= y1) return;

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

/* =========================================================================
 * 3. UNIFIED VECTOR PATH & BEZIER ENGINE (Zero Joint Overdraw)
 * ========================================================================= */
static void vg_begin_path(void) {
    ui.seg_count = 0;
    ui.min_x = 1e9f; ui.min_y = 1e9f;
    ui.max_x = -1e9f; ui.max_y = -1e9f;
}

static void vg_move_to(float x, float y) {
    ui.cur_x = x; ui.cur_y = y;
}

static void vg_line_to(float x, float y) {
    if (ui.seg_count < 128) {
        VgSeg *s = &ui.segs[ui.seg_count++];
        s->x0 = ui.cur_x; s->y0 = ui.cur_y;
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

static void vg_quad_to(float cx, float cy, float x, float y) {
    float x0 = ui.cur_x, y0 = ui.cur_y;
    const int steps = 6;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float u = 1.0f - t;
        float px = u * u * x0 + 2.0f * u * t * cx + t * t * x;
        float py = u * u * y0 + 2.0f * u * t * cy + t * t * y;
        vg_line_to(px, py);
    }
}

static void vg_cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y) {
    float x0 = ui.cur_x, y0 = ui.cur_y;
    const int steps = 12;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float u = 1.0f - t;
        float px = u*u*u*x0 + 3.0f*u*u*t*c1x + 3.0f*u*t*t*c2x + t*t*t*x;
        float py = u*u*u*y0 + 3.0f*u*u*t*c1y + 3.0f*u*t*t*c2y + t*t*t*y;
        vg_line_to(px, py);
    }
}

static void vg_arc(float cx, float cy, float r, float a0, float a1, int steps) {
    for (int i = 0; i <= steps; i++) {
        float a = a0 + (a1 - a0) * ((float)i / (float)steps);
        float px = cx + vg_cosf(a) * r;
        float py = cy + vg_sinf(a) * r;
        if (i == 0) vg_move_to(px, py);
        else vg_line_to(px, py);
    }
}

/* Evaluates exact minimum distance to the entire vector path in a single pass */
static void vg_stroke(float thickness, uint32_t col) {
    if (ui.seg_count == 0) return;
    float rad = thickness * 0.5f;
    float max_d = rad + 0.65f;
    float max_d2 = max_d * max_d;

    int x0 = (int)(ui.min_x - max_d - 1.0f); if (x0 < 0) x0 = 0;
    int y0 = (int)(ui.min_y - max_d - 1.0f); if (y0 < 0) y0 = 0;
    int x1 = (int)(ui.max_x + max_d + 2.0f); if (x1 > ui.w) x1 = ui.w;
    int y1 = (int)(ui.max_y + max_d + 2.0f); if (y1 > ui.h) y1 = ui.h;
    if (x0 >= x1 || y0 >= y1) return;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t *row = &ui.px[py * ui.w];
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            float min_d2 = max_d2;
            for (int i = 0; i < ui.seg_count; i++) {
                const VgSeg *s = &ui.segs[i];
                float wx = fx - s->x0;
                float wy = fy - s->y0;
                float t = (wx * s->vx + wy * s->vy) * s->inv_len2;
                if (t < 0.0f) t = 0.0f;
                else if (t > 1.0f) t = 1.0f;
                float dx = wx - t * s->vx;
                float dy = wy - t * s->vy;
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
 * 4. PROPORTIONAL BEZIER VECTOR FONT ENGINE (SVG-Style M/L/Q Outlines)
 * ========================================================================= */
static int hex_coord(char c) {
    return (c >= '0' && c <= '9') ? (c - '0') : (c - 'A' + 10);
}

static const char *vg_glyph_path(char c, float *adv) {
    *adv = 10.5f;
    switch (c) {
        /* Uppercase Geometric Sans-Serif (0..8 x 0..12) */
        case 'A': return "M0CL40L8CM18L78";
        case 'B': return "M00L0CL5CQ8C89Q8656L06M00L50Q8083Q8656";
        case 'C': return "M83Q8040Q0004L08Q0C4CQ8C89";
        case 'D': return "M00L0CL4CQ8C86Q8040L00";
        case 'E': *adv = 9.5f; return "M70L00L0CL7CM06L66";
        case 'F': *adv = 9.0f; return "M70L00L0CM06L66";
        case 'G': return "M83Q8040Q0004L08Q0C4CQ8C88L86L46";
        case 'H': return "M00L0CM80L8CM06L86";
        case 'I': *adv = 6.5f; return "M10L50M30L3CM1CL5C";
        case 'J': *adv = 9.0f; return "M60L68Q6C3CQ0C09";
        case 'K': return "M00L0CM80L07M35L8C";
        case 'L': *adv = 9.0f; return "M00L0CL7C";
        case 'M': *adv = 12.0f; return "M0CL00L58LA0LAC";
        case 'N': return "M0CL00L8CL80";
        case 'O': return "M04Q0040Q8084L88Q8C4CQ0C08L04";
        case 'P': return "M0CL00L50Q8083Q8656L06";
        case 'Q': return "M04Q0040Q8084L88Q8C4CQ0C08L04M58L8C";
        case 'R': return "M0CL00L50Q8083Q8656L06M56L8C";
        case 'S': return "M83Q8040Q0003Q0646Q8689Q8C4CQ0C09";
        case 'T': *adv = 9.5f; return "M00L80M40L4C";
        case 'U': return "M00L08Q0C4CQ8C88L80";
        case 'V': return "M00L4CL80";
        case 'W': *adv = 12.0f; return "M00L2CL54L8CLA0";
        case 'X': return "M00L8CM80L0C";
        case 'Y': return "M00L46L80M46L4C";
        case 'Z': return "M00L80L0CL8C";

        /* Lowercase with true x-height (4..12), ascenders (0), & descenders (F=15) */
        case 'a': *adv = 9.5f; return "M74L7CM76Q7444Q0408Q0C4CQ7C7A";
        case 'b': *adv = 9.5f; return "M00L0CM06Q0444Q7478Q7C4CQ0C0A";
        case 'c': *adv = 8.8f; return "M76Q6434Q0408Q0C3CQ6C7A";
        case 'd': *adv = 9.5f; return "M70L7CM76Q7434Q0408Q0C3CQ7C7A";
        case 'e': *adv = 9.2f; return "M08L78Q7434Q0408Q0C4CQ6C7A";
        case 'f': *adv = 6.5f; return "M51Q2023L2CM05L55";
        case 'g': *adv = 9.5f; return "M74L7CQ7F3FQ1F0DM76Q7434Q0408Q0C3CQ7C7A";
        case 'h': *adv = 9.5f; return "M00L0CM07Q0444Q7477L7C";
        case 'i': *adv = 4.8f; return "M20L21M24L2C";
        case 'j': *adv = 5.5f; return "M30L31M34L3CQ3F0F";
        case 'k': *adv = 9.0f; return "M00L0CM64L08M27L7C";
        case 'l': *adv = 4.8f; return "M20L2AQ2C4C";
        case 'm': *adv = 12.0f; return "M0CL04M06Q0424Q5456L5CM56Q5474QA4A6LAC";
        case 'n': *adv = 9.5f; return "M0CL04M07Q0444Q7477L7C";
        case 'o': *adv = 9.5f; return "M08Q0444Q7478Q7C4CQ0C08";
        case 'p': *adv = 9.5f; return "M04L0FM06Q0444Q7478Q7C4CQ0C0A";
        case 'q': *adv = 9.5f; return "M74L7FM76Q7434Q0408Q0C3CQ7C7A";
        case 'r': *adv = 6.8f; return "M0CL04M07Q0434Q5455";
        case 's': *adv = 8.8f; return "M66Q6434Q0406Q0838Q686AQ6C3CQ0C0A";
        case 't': *adv = 6.5f; return "M21L2AQ2C5CM04L54";
        case 'u': *adv = 9.5f; return "M04L09Q0C4CQ7C79L74";
        case 'v': *adv = 9.0f; return "M04L3CL74";
        case 'w': *adv = 12.0f; return "M04L2CL56L8CLA4";
        case 'x': *adv = 9.0f; return "M04L7CM74L0C";
        case 'y': *adv = 9.0f; return "M04L3CM74L3CQ1F0F";
        case 'z': *adv = 8.8f; return "M04L74L0CL7C";

        /* Digits & Symbols */
        case '0': return "M04Q0040Q8084L88Q8C4CQ0C08L04M2AL62";
        case '1': *adv = 7.0f; return "M12L30L3CM1CL5C";
        case '2': return "M03Q0040Q8083Q8648L0CL8C";
        case '3': return "M02Q0040Q8083Q8646Q8689Q8C4CQ0C0A";
        case '4': return "M6CL60L08L88";
        case '5': return "M80L00L06L56Q8689Q8C4CQ0C09";
        case '6': return "M82Q7040Q0005L08Q0C4CQ8C89Q8545Q0508";
        case '7': return "M00L80L3C";
        case '8': return "M46Q0603Q0040Q8083Q8646Q0609Q0C4CQ8C89Q8646";
        case '9': return "M0AQ1C4CQ8C87L84Q8040Q0003Q0747Q8784";
        case '.': *adv = 5.0f; return "M2BL2C";
        case ':': *adv = 5.0f; return "M24L25M2AL2B";
        case '-': *adv = 8.0f; return "M16L66";
        case '+': *adv = 9.0f; return "M16L76M43L49";
        case '/': *adv = 8.0f; return "M1CL70";
        case '%': *adv = 11.5f; return "M1CL70M10Q0002Q2220Q0010M7AQ6A6CQ8C8AQ6A7A";
        case '(': *adv = 6.5f; return "M40Q164C";
        case ')': *adv = 6.5f; return "M10Q461C";
        default:  *adv = 5.5f; return "";
    }
}

static float vg_text_width(const char *s, float size) {
    float scale = size / 12.0f;
    float w = 0.0f;
    for (; *s; s++) {
        float adv = 0.0f;
        vg_glyph_path(*s, &adv);
        w += adv * scale;
    }
    return w;
}

static void vg_text(float x, float y, float size, const char *s, uint32_t col) {
    float scale = size / 12.0f;
    float stroke = vg_maxf(size * 0.135f, 1.25f);

    for (; *s; s++) {
        float adv = 0.0f;
        const char *p = vg_glyph_path(*s, &adv);
        if (*p) {
            vg_begin_path();
            while (*p) {
                char cmd = *p++;
                if (cmd == 'M') {
                    float gx = x + (float)hex_coord(p[0]) * scale;
                    float gy = y + (float)hex_coord(p[1]) * scale;
                    vg_move_to(gx, gy); p += 2;
                } else if (cmd == 'L') {
                    float gx = x + (float)hex_coord(p[0]) * scale;
                    float gy = y + (float)hex_coord(p[1]) * scale;
                    vg_line_to(gx, gy); p += 2;
                } else if (cmd == 'Q') {
                    float cx = x + (float)hex_coord(p[0]) * scale;
                    float cy = y + (float)hex_coord(p[1]) * scale;
                    float gx = x + (float)hex_coord(p[2]) * scale;
                    float gy = y + (float)hex_coord(p[3]) * scale;
                    vg_quad_to(cx, cy, gx, gy); p += 4;
                }
            }
            vg_stroke(stroke, col);
        }
        x += adv * scale;
    }
}

static void vg_number(float x, float y, float size, int val, const char *suffix, uint32_t col) {
    char buf[32];
    int i = 0;
    if (val < 0) { buf[i++] = '-'; val = -val; }
    if (val == 0) buf[i++] = '0';
    else {
        char rev[16]; int r = 0;
        while (val > 0 && r < 15) { rev[r++] = (char)('0' + (val % 10)); val /= 10; }
        while (r > 0) buf[i++] = rev[--r];
    }
    while (*suffix && i < 31) buf[i++] = *suffix++;
    buf[i] = '\0';
    vg_text(x, y, size, buf, col);
}

/* =========================================================================
 * 5. IMMEDIATE-MODE VECTOR WIDGETS (Resolution-Independent)
 * ========================================================================= */
static bool hit_rect(float x, float y, float w, float h) {
    return ui.mx >= x && ui.mx < (x + w) && ui.my >= y && ui.my < (y + h);
}

static void ui_card(float x, float y, float w, float h, float r) {
    record_rect("card", (int)x, (int)y, (int)w, (int)h, -1);
    ui.current_parent = ui.rect_count - 1;
    vg_shadow_rrect(x, y + 4.0f * ui.scale, w, h, r, 14.0f * ui.scale, 0x55000000u);
    vg_fill_rrect(x, y, w, h, r, 0xFF181926u);
    vg_stroke_rrect(x, y, w, h, r, 1.2f, 0xFF2E3148u);
}

static bool ui_nav_item(float x, float y, float w, const char *label, int icon_type, bool active) {
    int id = ++ui.next_id;
    float h = 38.0f * ui.scale;
    record_rect(label, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    bool hover = hit_rect(x, y, w, h);
    if (hover) ui.hot_id = id;
    bool clicked = hover && ui.mouse_pressed;

    if (active) vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, ui.accent);
    else if (hover) vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, 0xFF23263Au);

    uint32_t fg = active ? 0xFF11121Cu : (hover ? 0xFFEDF0FAu : 0xFF9399B2u);

    /* Vector Icon */
    float ix = x + 18.0f * ui.scale, iy = y + h * 0.5f, ir = 6.5f * ui.scale;
    vg_begin_path();
    if (icon_type == 0) { /* Vector Waveform Icon */
        vg_move_to(ix - ir, iy);
        vg_line_to(ix - ir * 0.4f, iy - ir);
        vg_line_to(ix + ir * 0.4f, iy + ir);
        vg_line_to(ix + ir, iy);
    } else if (icon_type == 1) { /* Vector Bezier Diamond Icon */
        vg_move_to(ix, iy - ir); vg_line_to(ix + ir, iy);
        vg_line_to(ix, iy + ir); vg_line_to(ix - ir, iy);
        vg_line_to(ix, iy - ir);
    } else { /* Vector Gauge Arc Icon */
        vg_arc(ix, iy + 1.0f, ir, 3.14f, 6.28f, 10);
        vg_move_to(ix, iy + 1.0f); vg_line_to(ix + ir * 0.6f, iy - ir * 0.6f);
    }
    vg_stroke(1.7f * ui.scale, fg);

    vg_text(x + 34.0f * ui.scale, y + 12.5f * ui.scale, 13.0f * ui.scale, label, fg);
    return clicked;
}

static void ui_toggle(const char *label, bool *state) {
    int id = ++ui.next_id;
    float x = ui.lx, y = ui.ly, w = ui.lw, h = 40.0f * ui.scale;
    record_rect(label, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    bool hover = hit_rect(x, y, w, h);
    if (hover) ui.hot_id = id;
    if (hover && ui.mouse_pressed) *state = !(*state);

    vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, hover ? 0xFF23263Au : 0xFF1E2030u);
    vg_stroke_rrect(x, y, w, h, 8.0f * ui.scale, 1.0f, hover ? 0xFF383C5Cu : 0xFF282B40u);
    vg_text(x + 14.0f * ui.scale, y + 13.5f * ui.scale, 13.0f * ui.scale, label, 0xFFDCE0F5u);

    float pw = 42.0f * ui.scale, ph = 22.0f * ui.scale;
    float px = x + w - pw - 12.0f * ui.scale, py = y + (h - ph) * 0.5f;
    vg_fill_rrect(px, py, pw, ph, ph * 0.5f, *state ? ui.accent : 0xFF33364Du);
    float r = (ph - 6.0f * ui.scale) * 0.5f;
    float kx = *state ? (px + pw - r - 3.0f * ui.scale) : (px + r + 3.0f * ui.scale);
    vg_fill_circle(kx, py + ph * 0.5f, r, 0xFFFFFFFFu);

    ui.ly += h + 10.0f * ui.scale;
}

static void ui_slider(const char *label, float *val, float min_v, float max_v, const char *suffix) {
    int id = ++ui.next_id;
    float x = ui.lx, y = ui.ly, w = ui.lw, h = 54.0f * ui.scale;
    record_rect(label, (int)x, (int)y, (int)w, (int)h, ui.current_parent);
    float tx = x + 14.0f * ui.scale, ty = y + 36.0f * ui.scale, tw = w - 28.0f * ui.scale;

    bool hover = hit_rect(x, y, w, h);
    if (hover) ui.hot_id = id;
    if (hover && ui.mouse_pressed) ui.active_id = id;

    if (ui.active_id == id && ui.mouse_down) {
        float t = vg_clampf((ui.mx - tx) / (tw > 0.0f ? tw : 1.0f), 0.0f, 1.0f);
        *val = min_v + t * (max_v - min_v);
    }

    bool active = (hover || ui.active_id == id);
    vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, active ? 0xFF23263Au : 0xFF1E2030u);
    vg_stroke_rrect(x, y, w, h, 8.0f * ui.scale, 1.0f, active ? 0xFF3D4266u : 0xFF282B40u);

    vg_text(x + 14.0f * ui.scale, y + 10.0f * ui.scale, 13.0f * ui.scale, label, 0xFFDCE0F5u);
    vg_number(x + w - 58.0f * ui.scale, y + 10.0f * ui.scale, 13.0f * ui.scale, (int)(*val + 0.5f), suffix, ui.accent);

    float th = 6.0f * ui.scale;
    vg_fill_rrect(tx, ty - th * 0.5f, tw, th, th * 0.5f, 0xFF2E3148u);
    float norm = (*val - min_v) / (max_v - min_v);
    float fw = norm * tw;
    if (fw > 1.0f) vg_fill_rrect(tx, ty - th * 0.5f, fw, th, th * 0.5f, ui.accent);
    vg_fill_circle(tx + fw, ty, 7.5f * ui.scale, 0xFFFFFFFFu);
    vg_fill_circle(tx + fw, ty, 3.5f * ui.scale, ui.accent);

    ui.ly += h + 10.0f * ui.scale;
}

static bool ui_button(const char *label) {
    int id = ++ui.next_id;
    float s = 13.0f * ui.scale;
    float tw = vg_text_width(label, s);
    float w = tw + 36.0f * ui.scale, h = 38.0f * ui.scale;
    float x = ui.lx, y = ui.ly;
    record_rect(label, (int)x, (int)y, (int)w, (int)h, ui.current_parent);

    bool hover = hit_rect(x, y, w, h);
    if (hover) ui.hot_id = id;
    bool clicked = hover && ui.mouse_pressed;

    uint32_t bg = (hover && ui.mouse_down) ? 0xFFFFFFFFu : (hover ? ui.accent : 0xFF2A2D42u);
    uint32_t fg = hover ? 0xFF11121Cu : 0xFFEDF0FAu;
    vg_fill_rrect(x, y, w, h, 8.0f * ui.scale, bg);
    if (!hover) vg_stroke_rrect(x, y, w, h, 8.0f * ui.scale, 1.2f, ui.accent);
    vg_text(x + 18.0f * ui.scale, y + 12.5f * ui.scale, s, label, fg);

    ui.ly += h + 12.0f * ui.scale;
    return clicked;
}

static void ui_radial_gauge(float cx, float cy, float r, float pct, const char *label) {
    record_rect(label, (int)(cx - r), (int)(cy - r), (int)(r * 2), (int)(r * 2), ui.current_parent);
    float a0 = 2.356f; /* 135 deg */
    float a1 = 7.068f; /* 405 deg */
    vg_begin_path();
    vg_arc(cx, cy, r, a0, a1, 28);
    vg_stroke(6.5f * ui.scale, 0xFF282B40u);

    float am = a0 + (a1 - a0) * vg_clampf(pct * 0.01f, 0.0f, 1.0f);
    if (pct > 0.5f) {
        vg_begin_path();
        vg_arc(cx, cy, r, a0, am, 28);
        vg_stroke(6.5f * ui.scale, ui.accent);
    }
    vg_number(cx - 18.0f * ui.scale, cy - 6.0f * ui.scale, 14.5f * ui.scale, (int)pct, "%", 0xFFEDF0FAu);
    float lw = vg_text_width(label, 11.5f * ui.scale);
    vg_text(cx - lw * 0.5f, cy + r + 8.0f * ui.scale, 11.5f * ui.scale, label, 0xFF9399B2u);
}

/* =========================================================================
 * 6. APPLICATION STATE & VECTOR SCENE COMPOSITOR
 * ========================================================================= */
static int   app_tab   = 0;
static bool  opt_aa    = true;
static bool  opt_hdr   = true;
static float val_gain  = 76.0f;
static float val_freq  = 42.0f;
static float val_q     = 64.0f;
static float val_scale = 100.0f; /* Live DPI Vector Scale % */

static void gui_frame(void) {
    ui.next_id = 0;
    ui.hot_id = 0;
    ui.rect_count = 0;
    ui.current_parent = -1;
    if (ui.mouse_released) ui.active_id = 0;
    ui.scale = vg_clampf(val_scale * 0.01f, 0.75f, 1.30f);

    /* 1. Smooth Vertical Gradient Background */
    for (int y = 0; y < ui.h; y++) {
        uint32_t b = 20u + (uint32_t)(y * 12 / ui.h);
        uint32_t col = 0xFF0E1000u | b;
        uint32_t *row = &ui.px[y * ui.w];
        for (int x = 0; x < ui.w; x++) row[x] = col;
    }

    float pad = 18.0f * ui.scale;
    float side_w = 205.0f * ui.scale;
    float card_h = (float)ui.h - pad * 2.0f;

    /* 2. Left Vector Navigation Sidebar */
    ui_card(pad, pad, side_w, card_h, 12.0f * ui.scale);

    /* Vector Brand Emblem (Intersecting Bezier Rings) */
    float bx = pad + 26.0f * ui.scale, by = pad + 28.0f * ui.scale;
    vg_fill_circle(bx, by, 9.0f * ui.scale, ui.accent);
    vg_fill_circle(bx, by, 4.0f * ui.scale, 0xFF181926u);
    vg_text(bx + 16.0f * ui.scale, by - 7.0f * ui.scale, 15.0f * ui.scale, "VectorUI", 0xFFEDF0FAu);

    float ny = pad + 62.0f * ui.scale;
    float nw = side_w - 24.0f * ui.scale;
    if (ui_nav_item(pad + 12.0f * ui.scale, ny, nw, "Synthesizer", 0, app_tab == 0)) app_tab = 0;
    if (ui_nav_item(pad + 12.0f * ui.scale, ny + 46.0f * ui.scale, nw, "Bezier Studio", 1, app_tab == 1)) app_tab = 1;
    if (ui_nav_item(pad + 12.0f * ui.scale, ny + 92.0f * ui.scale, nw, "Vector DPI", 2, app_tab == 2)) app_tab = 2;

    /* Accent Color Swatches */
    float sy = pad + card_h - 46.0f * ui.scale;
    vg_text(pad + 16.0f * ui.scale, sy - 22.0f * ui.scale, 11.5f * ui.scale, "Theme Accent", 0xFF6C7086u);
    const uint32_t swatches[4] = {0xFF89B4FAu, 0xFFA6E3A1u, 0xFFCBA6F7u, 0xFFFAB387u};
    const char *swatch_names[4] = {"Accent Swatch Blue", "Accent Swatch Green", "Accent Swatch Purple", "Accent Swatch Orange"};
    for (int i = 0; i < 4; i++) {
        float sx = pad + 28.0f * ui.scale + (float)i * 40.0f * ui.scale;
        float sr = 11.0f * ui.scale;
        record_rect(swatch_names[i], (int)(sx - sr), (int)(sy - sr), (int)(sr * 2.0f), (int)(sr * 2.0f), ui.current_parent);
        if (hit_rect(sx - sr, sy - sr, sr * 2.0f, sr * 2.0f) && ui.mouse_pressed)
            ui.accent = swatches[i];
        if (ui.accent == swatches[i])
            vg_fill_circle(sx, sy, sr + 2.5f * ui.scale, 0xFFFFFFFFu);
        vg_fill_circle(sx, sy, sr, swatches[i]);
    }
    ui.current_parent = -1;

    /* 3. Main Content Vector Card */
    float mx = pad * 2.0f + side_w;
    float mw = (float)ui.w - mx - pad;
    ui_card(mx, pad, mw, card_h, 12.0f * ui.scale);

    ui.lx = mx + 22.0f * ui.scale;
    ui.ly = pad + 56.0f * ui.scale;
    ui.lw = mw - 44.0f * ui.scale;

    if (app_tab == 0) {
        vg_text(ui.lx, pad + 20.0f * ui.scale, 16.0f * ui.scale, "Harmonic Vector Synthesizer", 0xFFEDF0FAu);
        ui_toggle("Sub-Pixel SDF Anti-Aliasing", &opt_aa);
        ui_slider("Oscillator Amplitude", &val_gain, 0.0f, 100.0f, "%");
        ui_slider("Harmonic Frequency", &val_freq, 10.0f, 100.0f, "Hz");

        /* Live Vector Waveform & Radial Gauges */
        float cy = ui.ly + 4.0f * ui.scale;
        float ch = card_h - (cy - pad) - 18.0f * ui.scale;
        if (ch > 80.0f * ui.scale) {
            float gw = ui.lw * 0.62f;
            vg_fill_rrect(ui.lx, cy, gw, ch, 10.0f * ui.scale, 0xFF11121Cu);
            vg_stroke_rrect(ui.lx, cy, gw, ch, 10.0f * ui.scale, 1.0f, 0xFF282B40u);
            vg_text(ui.lx + 14.0f * ui.scale, cy + 12.0f * ui.scale, 11.5f * ui.scale, "Continuous Vector Spline", 0xFF6C7086u);

            vg_begin_path();
            float mid = cy + ch * 0.56f;
            float amp = (val_gain * 0.01f) * (ch * 0.34f);
            int steps = 48;
            for (int i = 0; i <= steps; i++) {
                float t = (float)i / (float)steps;
                float px = ui.lx + 14.0f * ui.scale + t * (gw - 28.0f * ui.scale);
                float phase = t * (val_freq * 0.12f) + (float)ui.tick * 0.06f;
                float py = mid + vg_sinf(phase) * amp;
                if (i == 0) vg_move_to(px, py);
                else vg_line_to(px, py);
            }
            vg_stroke(2.4f * ui.scale, ui.accent);

            /* Vector Radial Arc Meters */
            float r_rad = vg_minf(ch * 0.28f, 34.0f * ui.scale);
            float gx1 = ui.lx + gw + (ui.lw - gw) * 0.30f;
            float gx2 = ui.lx + gw + (ui.lw - gw) * 0.76f;
            ui_radial_gauge(gx1, cy + ch * 0.45f, r_rad, val_gain, "Gain");
            ui_radial_gauge(gx2, cy + ch * 0.45f, r_rad, val_freq, "Pitch");
        }
    } else if (app_tab == 1) {
        vg_text(ui.lx, pad + 20.0f * ui.scale, 16.0f * ui.scale, "Interactive Cubic Bezier Curve", 0xFFEDF0FAu);
        ui_slider("Control Point 1 Tension", &val_gain, 0.0f, 100.0f, "%");
        ui_slider("Control Point 2 Curvature", &val_q, 0.0f, 100.0f, "%");

        float bx0 = ui.lx, by0 = ui.ly + 6.0f * ui.scale;
        float bw = ui.lw, bh = card_h - (by0 - pad) - 20.0f * ui.scale;
        if (bh > 70.0f) {
            vg_fill_rrect(bx0, by0, bw, bh, 10.0f * ui.scale, 0xFF11121Cu);
            vg_stroke_rrect(bx0, by0, bw, bh, 10.0f * ui.scale, 1.0f, 0xFF282B40u);

            float p0x = bx0 + 30.0f * ui.scale, p0y = by0 + bh - 26.0f * ui.scale;
            float p3x = bx0 + bw - 30.0f * ui.scale, p3y = by0 + 26.0f * ui.scale;
            float c1x = bx0 + bw * 0.35f, c1y = by0 + bh - (val_gain * 0.01f) * (bh - 30.0f * ui.scale);
            float c2x = bx0 + bw * 0.65f, c2y = by0 + (val_q * 0.01f) * (bh - 30.0f * ui.scale);

            /* Tangent Handle Lines */
            vg_begin_path();
            vg_move_to(p0x, p0y); vg_line_to(c1x, c1y);
            vg_move_to(p3x, p3y); vg_line_to(c2x, c2y);
            vg_stroke(1.4f * ui.scale, 0xFF45496Au);

            /* Cubic Bezier Spline */
            vg_begin_path();
            vg_move_to(p0x, p0y);
            vg_cubic_to(c1x, c1y, c2x, c2y, p3x, p3y);
            vg_stroke(3.0f * ui.scale, ui.accent);

            vg_fill_circle(p0x, p0y, 5.0f * ui.scale, 0xFFFFFFFFu);
            vg_fill_circle(p3x, p3y, 5.0f * ui.scale, 0xFFFFFFFFu);
            vg_fill_circle(c1x, c1y, 6.0f * ui.scale, ui.accent);
            vg_fill_circle(c2x, c2y, 6.0f * ui.scale, ui.accent);
        }
    } else {
        vg_text(ui.lx, pad + 20.0f * ui.scale, 16.0f * ui.scale, "Resolution-Independent Vector Scaling", 0xFFEDF0FAu);
        ui_slider("UI Vector Scale (Live DPI)", &val_scale, 75.0f, 125.0f, "%");
        ui_toggle("10-Bit Linear Color Blending", &opt_hdr);
        if (ui_button("Reset 100% Scale")) val_scale = 100.0f;
        vg_text(ui.lx, ui.ly + 8.0f * ui.scale, 12.5f * ui.scale,
                "Drag the slider above: all fonts, icons & paths scale as pure vectors.", 0xFF9399B2u);
    }
    ui.current_parent = -1;

    ui.mouse_pressed = false;
    ui.mouse_released = false;
    ui.tick++;
}

/* =========================================================================
 * 7. HEADLESS UTILITIES & BMP EXPORTER (For Testing & CLI Output)
 * ========================================================================= */
static bool save_bmp(const char *filename, int w, int h) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;

    size_t sz_w = (size_t)w;
    size_t sz_h = (size_t)h;
    uint32_t file_size = 54u + (uint32_t)(sz_w * sz_h * 4);
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
    if (fwrite(ui.px, 4, sz_w * sz_h, f) != sz_w * sz_h) { fclose(f); return false; }
    fclose(f);
    return true;
}

/* Headless smoke test: simulates interactive clicks & validates state transitions */
static int run_headless_tests(const char *bmp_output, int snap_w, int snap_h, int snap_tab) {
    bool negative_mode = getenv("C99GUI_TEST_NEGATIVE") != NULL;
    if (negative_mode) {
        printf("Running C99 vectorized GUI headless test suite (NEGATIVE MODE)...\n");
    } else {
        printf("Running C99 vectorized GUI headless test suite...\n");
    }
    ui.accent = 0xFF89B4FAu;

    int passed = 0, failed = 0;

    /* Snapshot mode: render one frame at requested size/tab and save */
    if (bmp_output && !negative_mode) {
        ui.w = snap_w;
        ui.h = snap_h;
        app_tab = snap_tab;
        gui_frame();
        if (!save_bmp(bmp_output, snap_w, snap_h)) {
            fprintf(stderr, "FAIL: saving BMP output to %s failed\n", bmp_output);
            return 1;
        }
        printf("ok   vector snapshot saved to %s (%dx%d tab=%d)\n", bmp_output, snap_w, snap_h, snap_tab);
        return 0;
    }

    /* Resize honesty test: at MIN_W x MIN_H and 1920x1080, every recorded rect must be inside window and parent */
    int test_sizes[2][2] = {{MIN_W, MIN_H}, {1920, 1080}};
    for (int sz = 0; sz < 2; sz++) {
        int tw = test_sizes[sz][0];
        int th = test_sizes[sz][1];
        ui.w = tw;
        ui.h = th;
        for (int t = 0; t < 3; t++) {
            app_tab = t;
            gui_frame();
            for (int i = 0; i < ui.rect_count; i++) {
                const Rect *r = &ui.rects[i];
                int rx = r->x;
                int ry = r->y;
                int rw = r->w;
                int rh = r->h;
                if (rx < 0 || ry < 0 || rx + rw > tw || ry + rh > th) {
                    fprintf(stderr, "FAIL: rect '%s' [%d,%d,%d,%d] outside window %dx%d\n",
                            r->name, rx, ry, rw, rh, tw, th);
                    failed++;
                }
                if (r->parent_id >= 0 && r->parent_id < ui.rect_count) {
                    const Rect *pr = &ui.rects[r->parent_id];
                    if (rx < pr->x || ry < pr->y || rx + rw > pr->x + pr->w || ry + rh > pr->y + pr->h) {
                        fprintf(stderr, "FAIL: rect '%s' [%d,%d,%d,%d] outside parent '%s' [%d,%d,%d,%d]\n",
                                r->name, rx, ry, rw, rh, pr->name, pr->x, pr->y, pr->w, pr->h);
                        failed++;
                    }
                }
            }
        }
        if (failed == 0) {
            printf("ok   resize honesty test at %dx%d (all %d rects inside window & parent)\n", tw, th, ui.rect_count);
            passed++;
        }
    }

    /* Standard interactive tests at default size */
    ui.w = WIN_W;
    ui.h = WIN_H;

    /* Frame 1: Initial render */
    gui_frame();
    if (ui.px[100 * WIN_W + 100] == 0) {
        fprintf(stderr, "FAIL: framebuffer uninitialized\n");
        failed++;
    } else {
        printf("ok   vector framebuffer rasterized (%dx%d)\n", WIN_W, WIN_H);
        passed++;
    }

    /* Test 1: Click Nav Item (Bezier Studio tab) */
    int nav_idx = find_rect("Bezier Studio");
    if (nav_idx >= 0) {
        ui.mx = (float)ui.rects[nav_idx].x + 10.0f;
        ui.my = (float)ui.rects[nav_idx].y + 10.0f;
    } else {
        ui.mx = 100.0f; ui.my = 145.0f;
    }
    ui.mouse_pressed = true;
    gui_frame();
    if (app_tab != 1) {
        fprintf(stderr, "FAIL: tab switch failed (expected 1, got %d)\n", app_tab);
        failed++;
    } else {
        printf("ok   nav item click (switched to Bezier Studio tab)\n");
        passed++;
    }

    /* Test 2: Click Nav Item (Vector DPI tab) */
    ui.mx = 100.0f; ui.my = 190.0f;
    ui.mouse_pressed = true;
    gui_frame();
    if (app_tab != 2) {
        fprintf(stderr, "FAIL: tab switch failed (expected 2, got %d)\n", app_tab);
        failed++;
    } else {
        printf("ok   nav item click (switched to Vector DPI tab)\n");
        passed++;
    }

    /* Test 3: Click Reset 100% Scale button */
    val_scale = 120.0f;
    gui_frame();  /* Record rects at new scale */
    int btn_idx = find_rect("Reset 100% Scale");
    if (btn_idx >= 0) {
        ui.mx = (float)ui.rects[btn_idx].x + 10.0f;
        ui.my = (float)ui.rects[btn_idx].y + 10.0f;
    } else {
        ui.mx = 360.0f; ui.my = 248.0f;
    }
    ui.mouse_pressed = true;
    gui_frame();
    if (val_scale != 100.0f) {
        fprintf(stderr, "FAIL: scale reset failed (expected 100, got %f)\n", val_scale);
        failed++;
    } else {
        printf("ok   vector button click (scale reset to 100%%)\n");
        passed++;
    }

    /* Test 4: Return to Synthesizer tab */
    int synth_idx = find_rect("Synthesizer");
    if (synth_idx >= 0) {
        ui.mx = (float)ui.rects[synth_idx].x + 10.0f;
        ui.my = (float)ui.rects[synth_idx].y + 10.0f;
    } else {
        ui.mx = 100.0f; ui.my = 95.0f;
    }
    ui.mouse_pressed = true;
    gui_frame();
    if (app_tab != 0) {
        fprintf(stderr, "FAIL: return to synthesizer failed\n");
        failed++;
    } else {
        printf("ok   nav item return to Synthesizer tab\n");
        passed++;
    }

    /* Test 5: Toggle Sub-Pixel SDF Anti-Aliasing */
    bool before_aa = opt_aa;
    int toggle_idx = find_rect("Sub-Pixel SDF Anti-Aliasing");
    if (toggle_idx >= 0) {
        ui.mx = (float)ui.rects[toggle_idx].x + 10.0f;
        ui.my = (float)ui.rects[toggle_idx].y + 10.0f;
    } else {
        ui.mx = 400.0f; ui.my = 94.0f;
    }
    ui.mouse_pressed = true;
    gui_frame();
    if (opt_aa == before_aa) {
        fprintf(stderr, "FAIL: toggle anti-aliasing failed\n");
        failed++;
    } else {
        printf("ok   vector toggle switch state flipped\n");
        passed++;
    }

    /* Test 6: Drag Slider (Harmonic Frequency) */
    int slider_idx = find_rect("Harmonic Frequency");
    if (slider_idx >= 0) {
        ui.mx = (float)ui.rects[slider_idx].x + 10.0f;
        ui.my = (float)ui.rects[slider_idx].y + 20.0f;
        ui.mouse_pressed = true;
        ui.mouse_down = true;
        gui_frame();
        ui.mx = (float)ui.rects[slider_idx].x + (float)ui.rects[slider_idx].w - 10.0f;
        gui_frame();
        ui.mouse_down = false;
        ui.mouse_released = true;
        gui_frame();
    } else {
        ui.mx = 400.0f; ui.my = 224.0f;
        ui.mouse_pressed = true;
        ui.mouse_down = true;
        gui_frame();
        ui.mx = 500.0f;
        gui_frame();
        ui.mouse_down = false;
        ui.mouse_released = true;
        gui_frame();
    }
    printf("ok   vector slider drag processed (val_freq=%.1f)\n", val_freq);
    passed++;

    /* Test 7: Accent swatch selection (Purple) */
    int swatch_idx = find_rect("Accent Swatch Purple");
    if (swatch_idx >= 0) {
        ui.mx = (float)ui.rects[swatch_idx].x + 10.0f;
        ui.my = (float)ui.rects[swatch_idx].y + 10.0f;
    } else {
        ui.mx = 126.0f; ui.my = 476.0f;
    }
    ui.mouse_pressed = true;
    gui_frame();
    if (ui.accent != 0xFFCBA6F7u) {
        fprintf(stderr, "FAIL: accent swatch failed (got 0x%08X)\n", ui.accent);
        failed++;
    } else {
        printf("ok   accent theme swatch selected (purple 0x%08X)\n", ui.accent);
        passed++;
    }

    if (bmp_output && !negative_mode) {
        if (!save_bmp(bmp_output, WIN_W, WIN_H)) {
            fprintf(stderr, "FAIL: saving BMP output to %s failed\n", bmp_output);
            failed++;
        } else {
            printf("ok   vector snapshot saved to %s\n", bmp_output);
            passed++;
        }
    }

    printf("passed: %d  failed: %d\n", passed, failed);
    if (failed > 0) return 1;
    if (negative_mode) {
        fprintf(stderr, "NEGATIVE MODE: expected failures but all tests passed\n");
        return 1;
    }
    return 0;
}

static void print_usage(void) {
    puts("Usage: " APP_NAME " [OPTIONS]\n"
         "\n"
         "Canonical Pure C99 2D Vector Graphics & Immediate-Mode GUI Engine.\n"
         "\n"
         "Options:\n"
         "  --test                 Run automated headless vector test suite and exit\n"
         "  --snapshot <file.bmp>  Render one frame, save to 32-bit BMP and exit\n"
         "  --size WxH             Set snapshot size (default: 860x540)\n"
         "  --tab N                Set snapshot tab index (default: 0)\n"
         "  --headless             Run 120 vector simulation frames headlessly and exit\n"
         "  -h, --help             Display this help message and exit\n"
         "  -V, --version          Display version information and exit\n");
}

/* =========================================================================
 * 8. NATIVE OS WINDOWING BACKENDS (Win32 / X11 / macOS Cocoa / Headless)
 * ========================================================================= */
#if defined(C99GUI_BACKEND_WIN32)

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
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
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) ui.running = false;
            return 0;
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                ui.w = (int)(short)LOWORD(lp);
                ui.h = (int)(short)HIWORD(lp);
                if (ui.w < MIN_W) ui.w = MIN_W;
                if (ui.h < MIN_H) ui.h = MIN_H;
            }
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)lp;
            mmi->ptMinTrackSize.x = MIN_W;
            mmi->ptMinTrackSize.y = MIN_H;
            return 0;
        }
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
    wc.lpszClassName = "C99VectorGUI";
    RegisterClassA(&wc);

    RECT rc = {0, 0, WIN_W, WIN_H};
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    AdjustWindowRect(&rc, style, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Pure C99 Vector GUI", style,
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
    ui.w = WIN_W;
    ui.h = WIN_H;
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

    NSRect frame = {{100.0, 100.0}, {(double)WIN_W, (double)WIN_H}};
    id win = msg4(id, msg0(id, objc_getClass("NSWindow"), "alloc"),
                  "initWithContentRect:styleMask:backing:defer:",
                  NSRect, frame, unsigned long, 15u, unsigned long, 2u, BOOL, 0);
    id title = msg1(id, objc_getClass("NSString"), "stringWithUTF8String:", const char *, "Pure C99 Vector GUI");
    msg1(void, win, "setTitle:", id, title);
    msg1(void, win, "makeKeyAndOrderFront:", id, NULL);
    msg1(void, app, "activateIgnoringOtherApps:", BOOL, 1);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    id view = msg0(id, win, "contentView");
    id layer = msg0(id, objc_getClass("CALayer"), "layer");
    msg1(void, view, "setLayer:", id, layer);
    msg1(void, view, "setWantsLayer:", BOOL, 1);

    ui.accent = 0xFF89B4FAu;
    ui.w = WIN_W;
    ui.h = WIN_H;
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
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 100, 100, WIN_W, WIN_H, 1,
                                     BlackPixel(dpy, scr), WhitePixel(dpy, scr));
    XStoreName(dpy, win, "Pure C99 Vector GUI");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XMapWindow(dpy, win);

    GC gc = DefaultGC(dpy, scr);
    XImage *img = XCreateImage(dpy, DefaultVisual(dpy, scr), (unsigned int)DefaultDepth(dpy, scr),
                               ZPixmap, 0, (char *)ui.px, WIN_W, WIN_H, 32, 0);

    ui.accent = 0xFF89B4FAu;
    ui.w = WIN_W;
    ui.h = WIN_H;
    ui.running = true;
    while (ui.running) {
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == MotionNotify) {
                ui.mx = (float)ev.xmotion.x; ui.my = (float)ev.xmotion.y;
            } else if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
                ui.mouse_down = true; ui.mouse_pressed = true;
            } else if (ev.type == ButtonRelease && ev.xbutton.button == Button1) {
                ui.mouse_down = false; ui.mouse_released = true;
            } else if (ev.type == KeyPress) {
                if (XLookupKeysym(&ev.xkey, 0) == XK_Escape) ui.running = false;
            } else if (ev.type == ConfigureNotify) {
                if (ev.xconfigure.width != ui.w || ev.xconfigure.height != ui.h) {
                    ui.w = ev.xconfigure.width;
                    ui.h = ev.xconfigure.height;
                    if (ui.w < MIN_W) ui.w = MIN_W;
                    if (ui.h < MIN_H) ui.h = MIN_H;
                    XDestroyImage(img);
                    img = XCreateImage(dpy, DefaultVisual(dpy, scr), (unsigned int)DefaultDepth(dpy, scr),
                                       ZPixmap, 0, (char *)ui.px, ui.w, ui.h, 32, 0);
                }
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
    int snapshot_w = WIN_W;
    int snapshot_h = WIN_H;
    int snapshot_tab = 0;
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
                snapshot_file = "vector_snapshot.bmp";
            }
        } else if (!strcmp(argv[i], "--size")) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                char *next = argv[++i];
                char *x = strchr(next, 'x');
                if (x) {
                    *x = '\0';
                    snapshot_w = atoi(next);
                    snapshot_h = atoi(x + 1);
                    *x = 'x';
                }
            }
        } else if (!strcmp(argv[i], "--tab")) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                snapshot_tab = atoi(argv[++i]);
            }
        } else {
            fprintf(stderr, APP_NAME ": unknown option '%s' (try --help)\n", argv[i]);
            return 2;
        }
    }

    if (do_test || snapshot_file) {
        return run_headless_tests(snapshot_file, snapshot_w, snapshot_h, snapshot_tab);
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
