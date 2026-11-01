/*
 * c99gui.c - a single-file, zero-allocation, immediate-mode terminal GUI in C99.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Foad Sojoodi Farimani
 *
 * Build (any C99 compiler, no libraries, no flags beyond the language level):
 *     cc -std=c99 -O2 -Wall -Wextra -pedantic src/c99gui.c -o c99gui
 *     cl /std:c11 /W4 /O2 /MT /utf-8 src\c99gui.c /Fe:c99gui.exe
 *     cc -std=c99 -DSTRICT_C99 src/c99gui.c -o c99gui        (ISO C only, line input)
 *
 * How to reuse this file as a template:
 *   1. Replace app_frame() and struct App with your own screens.
 *   2. Keep the engine (everything above "APPLICATION") untouched, or trim it.
 *   3. Every widget is one call per frame: ui_toggle(), ui_slider(), ...
 *
 * Design notes:
 *   - Rendering is a software framebuffer of Cells (UTF-8 glyph + 24-bit
 *     foreground + background). Only changed cells are written to the terminal.
 *   - The program never allocates: all state lives in static storage.
 *   - The source is pure 7-bit ASCII. Non-ASCII glyphs are written as \x escapes
 *     so MSVC code pages cannot corrupt them. Beware: \x consumes every hex digit
 *     that follows, so split literals ("Fran\xC3\xA7" "ais"), never "\xA7ais".
 *   - All platform code is isolated in the "PLATFORM LAYER" section. It exports
 *     exactly four functions: sys_init, sys_shutdown, sys_size and sys_key.
 */

/* ---------------------------------------------------------------------------
 * Feature-test macros (must precede every #include)
 * ------------------------------------------------------------------------- */
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_NAME    "c99gui"
#define APP_VERSION "1.0.0"

/* ---------------------------------------------------------------------------
 * Platform selection
 * ------------------------------------------------------------------------- */
#if defined(STRICT_C99)
#define PLAT_STRICT 1
#elif defined(_WIN32)
#define PLAT_WIN 1
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__) || \
    defined(__CYGWIN__) || defined(__MSYS__) || defined(__linux__) || defined(__HAIKU__)
#define PLAT_POSIX 1
#else
#define PLAT_STRICT 1
#endif

/* ---------------------------------------------------------------------------
 * Keys. Printable keys are their ASCII value; the rest are named below.
 * ------------------------------------------------------------------------- */
enum {
    KEY_EOF = -1,   /* input closed                                   */
    KEY_NONE = 0,   /* spurious wake-up (resize, unknown sequence)    */
    KEY_CTRL_C = 3,
    KEY_TAB = 9,
    KEY_ENTER = 13,
    KEY_ESC = 27,
    KEY_BACKSPACE = 127,
    KEY_UP = 256, KEY_DOWN, KEY_RIGHT, KEY_LEFT,
    KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN, KEY_DEL, KEY_BTAB
};

/* Terminal mode switches: alt screen, no autowrap, hidden cursor. */
#define SEQ_LEAVE "\033[0m\033[?7h\033[?25h\033[?1049l"
#if defined(PLAT_STRICT)
#define SEQ_ENTER "\033[?1049h\033[?7l\033[0m\033[2J" /* cursor stays: user types */
#else
#define SEQ_ENTER "\033[?1049h\033[?7l\033[?25l\033[0m\033[2J"
#endif

static bool g_screen_on = false;   /* true while the alt screen is active */

static void term_enter(void)
{
    fputs(SEQ_ENTER, stdout);
    fflush(stdout);
    g_screen_on = true;
}

static void term_leave(void)
{
    if (g_screen_on) {
        g_screen_on = false;
        fputs(SEQ_LEAVE, stdout);
        fflush(stdout);
    }
}

/* Size from COLUMNS/LINES, else 80x24. Used where the OS cannot tell us. */
static void env_size(int *w, int *h)
{
    const char *c = getenv("COLUMNS");
    const char *l = getenv("LINES");
    long cw = c ? strtol(c, NULL, 10) : 0;
    long ch = l ? strtol(l, NULL, 10) : 0;
    *w = (cw > 0 && cw < 10000) ? (int)cw : 80;
    *h = (ch > 0 && ch < 10000) ? (int)ch : 24;
}

/* ---------------------------------------------------------------------------
 * Generic VT key decoder. Used by POSIX, by the Windows non-console path
 * (pipes), and by --snapshot. rd() returns a byte, -1 on EOF, -2 when
 * interrupted; wait(ms) is true when a byte arrives within ms milliseconds.
 * ------------------------------------------------------------------------- */
typedef int (*ByteFn)(void);
typedef bool (*WaitFn)(int ms);

static int vt_key(ByteFn rd, WaitFn wait)
{
    int c = rd();
    int p = 0, np = 0;
    if (c == -1) return KEY_EOF;
    if (c < 0 || c == 0 || c >= 128) return KEY_NONE;
    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == 8 || c == 127) return KEY_BACKSPACE;
    if (c != 27) return c;
    if (!wait(40)) return KEY_ESC;
    c = rd();
    if (c != '[' && c != 'O') return KEY_ESC;
    for (;;) {
        c = rd();
        if (c < 0) return KEY_NONE;
        if (c >= '0' && c <= '9') { if (np == 0 && p < 1000) p = p * 10 + (c - '0'); }
        else if (c == ';') np++;
        else if (c >= 0x40 && c <= 0x7E) break;
    }
    switch (c) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    case 'H': return KEY_HOME;
    case 'F': return KEY_END;
    case 'Z': return KEY_BTAB;
    case '~':
        switch (p) {
        case 1: case 7: return KEY_HOME;
        case 4: case 8: return KEY_END;
        case 3: return KEY_DEL;
        case 5: return KEY_PGUP;
        case 6: return KEY_PGDN;
        default: break;
        }
        break;
    default: break;
    }
    return KEY_NONE;
}

/* stdio-based source: always-ready bytes. Used for --snapshot and Windows pipes. */
static int stdio_rd(void) { int c = getchar(); return c == EOF ? -1 : c; }
static bool stdio_wait(int ms) { (void)ms; return true; }

/* ===========================================================================
 * PLATFORM LAYER
 * ========================================================================= */
#if defined(PLAT_WIN)
/* ---- Windows: console API (MSVC, MinGW, clang, Intel, TinyCC) ----------- */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static HANDLE w_in, w_out;
static DWORD w_in_mode, w_out_mode;
static UINT w_cp;
static bool w_in_con, w_out_con, w_ready;

static void sys_shutdown(void)
{
    if (!w_ready) return;
    w_ready = false;
    term_leave();
    if (w_out_con) SetConsoleMode(w_out, w_out_mode);
    if (w_in_con) SetConsoleMode(w_in, w_in_mode);
    if (w_cp) SetConsoleOutputCP(w_cp);
}

static BOOL WINAPI w_ctrl(DWORD type)
{
    (void)type;
    sys_shutdown();
    return FALSE; /* let the default handler terminate the process */
}

static void sys_init(void)
{
    w_in = GetStdHandle(STD_INPUT_HANDLE);
    w_out = GetStdHandle(STD_OUTPUT_HANDLE);
    w_in_con = GetConsoleMode(w_in, &w_in_mode) != 0;
    w_out_con = GetConsoleMode(w_out, &w_out_mode) != 0;
    w_ready = true;
    atexit(sys_shutdown);
    SetConsoleCtrlHandler(w_ctrl, TRUE);
    if (w_out_con) {
        w_cp = GetConsoleOutputCP();
        SetConsoleOutputCP(65001);
        if (!SetConsoleMode(w_out, w_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            fputs(APP_NAME ": this console does not support ANSI/VT sequences "
                  "(Windows 10 1809+ or Windows Terminal required)\n", stderr);
            exit(1);
        }
    }
    if (w_in_con) {
        /* No line editing, echo or Ctrl-C processing: Ctrl-C arrives as a key. */
        SetConsoleMode(w_in, (w_in_mode & ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
                                                   ENABLE_PROCESSED_INPUT)) | ENABLE_WINDOW_INPUT);
    }
}

static void sys_size(int *w, int *h)
{
    CONSOLE_SCREEN_BUFFER_INFO i;
    if (w_out_con && GetConsoleScreenBufferInfo(w_out, &i)) {
        *w = i.srWindow.Right - i.srWindow.Left + 1;
        *h = i.srWindow.Bottom - i.srWindow.Top + 1;
    } else {
        env_size(w, h);
    }
}

static int sys_key(void)
{
    if (!w_in_con) return vt_key(stdio_rd, stdio_wait);
    for (;;) {
        INPUT_RECORD r;
        DWORD n = 0;
        if (!ReadConsoleInputW(w_in, &r, 1, &n) || n == 0) return KEY_EOF;
        if (r.EventType == WINDOW_BUFFER_SIZE_EVENT) return KEY_NONE;
        if (r.EventType == KEY_EVENT && r.Event.KeyEvent.bKeyDown) {
            WORD vk = r.Event.KeyEvent.wVirtualKeyCode;
            WCHAR ch = r.Event.KeyEvent.uChar.UnicodeChar;
            switch (vk) {
            case VK_UP: return KEY_UP;
            case VK_DOWN: return KEY_DOWN;
            case VK_LEFT: return KEY_LEFT;
            case VK_RIGHT: return KEY_RIGHT;
            case VK_HOME: return KEY_HOME;
            case VK_END: return KEY_END;
            case VK_PRIOR: return KEY_PGUP;
            case VK_NEXT: return KEY_PGDN;
            case VK_DELETE: return KEY_DEL;
            case VK_BACK: return KEY_BACKSPACE;
            case VK_RETURN: return KEY_ENTER;
            case VK_ESCAPE: return KEY_ESC;
            case VK_TAB:
                return (r.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED) ? KEY_BTAB : KEY_TAB;
            default: break;
            }
            if (ch >= 1 && ch < 127) return (int)ch;
        }
    }
}

#elif defined(PLAT_POSIX)
/* ---- POSIX: termios + pselect (Linux, macOS, BSD, Cygwin, MSYS2) -------- */
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

static struct termios px_orig;
static bool px_have_tty = false;

/* Async-signal-safe: restore the tty, leave the alt screen, die by the signal. */
static void px_fatal(int sig)
{
    if (px_have_tty) tcsetattr(0, TCSANOW, &px_orig);
    if (g_screen_on) {
        ssize_t u = write(1, SEQ_LEAVE, sizeof(SEQ_LEAVE) - 1);
        (void)u;
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void px_winch(int sig) { (void)sig; /* only purpose: interrupt pselect */ }

static void sys_shutdown(void)
{
    term_leave();
    if (px_have_tty) {
        tcsetattr(0, TCSANOW, &px_orig);
        px_have_tty = false;
    }
}

static void sys_init(void)
{
    struct sigaction sa;
    sigset_t blk;
    int i;
    static const int fatal[] = { SIGINT, SIGTERM, SIGHUP, SIGQUIT };

    if (tcgetattr(0, &px_orig) == 0) {
        struct termios raw = px_orig;
        px_have_tty = true;
        raw.c_iflag &= ~(tcflag_t)(ICRNL | INLCR | IXON | ISTRIP);
        raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG | IEXTEN);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(0, TCSANOW, &raw);
    }
    atexit(sys_shutdown);

    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = px_fatal;
    for (i = 0; i < (int)(sizeof fatal / sizeof fatal[0]); i++) sigaction(fatal[i], &sa, NULL);
#ifdef SIGWINCH
    /* Blocked except inside pselect, so a resize can never be missed. */
    sa.sa_handler = px_winch;
    sigaction(SIGWINCH, &sa, NULL);
    sigemptyset(&blk);
    sigaddset(&blk, SIGWINCH);
    sigprocmask(SIG_BLOCK, &blk, NULL);
#else
    (void)blk;
#endif
}

static void sys_size(int *w, int *h)
{
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        *w = ws.ws_col;
        *h = ws.ws_row;
    } else {
        env_size(w, h);
    }
}

/* 1 = input ready, 0 = timeout, -1 = interrupted by a signal. ms < 0: forever. */
static int px_poll(int ms)
{
    fd_set fs;
    struct timespec ts;
    sigset_t none;
    int r;
    FD_ZERO(&fs);
    FD_SET(0, &fs);
    sigemptyset(&none);
    if (ms >= 0) { ts.tv_sec = ms / 1000; ts.tv_nsec = (long)(ms % 1000) * 1000000L; }
    r = pselect(1, &fs, NULL, NULL, ms >= 0 ? &ts : NULL, &none);
    if (r < 0) return errno == EINTR ? -1 : 1; /* other errors: let read() report */
    return r > 0 ? 1 : 0;
}

static int px_rd(void)
{
    unsigned char c;
    ssize_t n;
    if (px_poll(-1) < 0) return -2;
    n = read(0, &c, 1);
    if (n == 1) return c;
    if (n < 0 && errno == EINTR) return -2;
    return -1;
}

static bool px_wait(int ms) { return px_poll(ms) > 0; }

static int sys_key(void) { return vt_key(px_rd, px_wait); }

#else
/* ---- Strict ISO C99: stdio only. One key per line (type key + Enter). --- */
static void sys_init(void) { atexit(term_leave); }
static void sys_shutdown(void) { term_leave(); }

static void sys_size(int *w, int *h)
{
    env_size(w, h);
    *h -= 1; /* the last row is the input prompt */
}

static int sys_key(void)
{
    int c = getchar();
    if (c == EOF) return KEY_EOF;
    if (c == '\n') return KEY_ENTER;
    if (c != '\r') {
        int d;
        while ((d = getchar()) != '\n' && d != EOF) { }
    }
    return (c == '\r' || c >= 128) ? KEY_NONE : c;
}
#endif

/* ===========================================================================
 * ENGINE: types, theme, framebuffer, presenter
 * ========================================================================= */
#define FB_MAX_W 240
#define FB_MAX_H 80
#define OUT_CAP  32768

typedef struct { unsigned char r, g, b; } Color;
typedef struct { Color fg, bg; char g[5]; } Cell; /* g: one UTF-8 glyph, NUL padded */

typedef struct {
    Color canvas, shadow, panel, surface, focus, border, text, muted,
          accent, green, yellow, red;
} Theme;

/* Catppuccin Mocha (dark) and Latte (light). */
static const Theme THEME_DARK = {
    { 17, 17, 27 }, { 10, 10, 16 }, { 30, 30, 46 }, { 49, 50, 68 },
    { 69, 71, 90 }, { 88, 91, 112 }, { 205, 214, 244 }, { 147, 153, 178 },
    { 137, 180, 250 }, { 166, 227, 161 }, { 249, 226, 175 }, { 243, 139, 168 }
};
static const Theme THEME_LIGHT = {
    { 220, 224, 232 }, { 188, 192, 204 }, { 239, 241, 245 }, { 204, 208, 218 },
    { 172, 176, 190 }, { 156, 160, 176 }, { 76, 79, 105 }, { 108, 111, 133 },
    { 30, 102, 245 }, { 64, 160, 43 }, { 223, 142, 29 }, { 210, 15, 57 }
};

/* UTF-8 glyphs as hex escapes (source stays pure ASCII). */
#define G_TL   "\xE2\x95\xAD" /* rounded top-left      */
#define G_TR   "\xE2\x95\xAE" /* rounded top-right     */
#define G_BL   "\xE2\x95\xB0" /* rounded bottom-left   */
#define G_BR   "\xE2\x95\xAF" /* rounded bottom-right  */
#define G_H    "\xE2\x94\x80" /* thin horizontal       */
#define G_HH   "\xE2\x94\x81" /* heavy horizontal      */
#define G_V    "\xE2\x94\x82" /* thin vertical         */
#define G_DOT  "\xE2\x97\x8F" /* solid circle          */
#define G_ARR  "\xE2\x96\xB8" /* focus arrow           */
#define G_STAR "\xE2\x9C\xA6" /* four-pointed star     */
#define G_LT   "\xE2\x80\xB9" /* single left angle     */
#define G_GT   "\xE2\x80\xBA" /* single right angle    */

typedef struct {
    Cell cur[FB_MAX_H][FB_MAX_W];
    Cell prev[FB_MAX_H][FB_MAX_W];
    int fw, fh;                 /* active framebuffer size                   */
    bool force;                 /* repaint everything next present           */
    bool color256;              /* quantise to the xterm 256 palette         */
    bool dirty;                 /* a widget changed state: redraw once more  */
    const Theme *t;
    /* window + layout cursor */
    bool open;
    int wx, wy, ww, wh, cx, cy;
    /* interaction */
    int key, focus, nwidgets;
    char status[96];
    /* output staging */
    char out[OUT_CAP];
    size_t out_len;
} UI;

static UI ui;

/* ---- output staging ------------------------------------------------------ */
static void out_flush(void)
{
    if (ui.out_len) {
        fwrite(ui.out, 1, ui.out_len, stdout);
        ui.out_len = 0;
    }
}

static void emit_n(const char *s, size_t n)
{
    if (ui.out_len + n > OUT_CAP) out_flush();
    memcpy(ui.out + ui.out_len, s, n);
    ui.out_len += n;
}

static void emit(const char *s) { emit_n(s, strlen(s)); }

static void emit_uint(unsigned v)
{
    char b[12];
    int i = (int)sizeof b;
    do { b[--i] = (char)('0' + v % 10u); v /= 10u; } while (v);
    emit_n(b + i, sizeof b - (size_t)i);
}

static int to_256(Color c)
{
    int r = c.r, g = c.g, b = c.b;
    if (r == g && g == b) {
        if (r < 8) return 16;
        if (r > 248) return 231;
        return 232 + (r - 8) * 24 / 247;
    }
    return 16 + 36 * ((r * 5 + 127) / 255) + 6 * ((g * 5 + 127) / 255) + (b * 5 + 127) / 255;
}

static void emit_color(bool fg, Color c)
{
    emit(fg ? "\033[38;" : "\033[48;");
    if (ui.color256) {
        emit("5;");
        emit_uint((unsigned)to_256(c));
    } else {
        emit("2;");
        emit_uint(c.r); emit(";");
        emit_uint(c.g); emit(";");
        emit_uint(c.b);
    }
    emit("m");
}

static void emit_move(int x, int y)
{
    emit("\033[");
    emit_uint((unsigned)y + 1u); emit(";");
    emit_uint((unsigned)x + 1u); emit("H");
}

/* ---- colour helpers ------------------------------------------------------ */
static bool color_eq(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

static Color color_mix(Color a, Color b, int num, int den)
{
    Color c;
    if (den <= 0) return a;
    c.r = (unsigned char)(a.r + ((int)b.r - (int)a.r) * num / den);
    c.g = (unsigned char)(a.g + ((int)b.g - (int)a.g) * num / den);
    c.b = (unsigned char)(a.b + ((int)b.b - (int)a.b) * num / den);
    return c;
}

/* ---- framebuffer primitives --------------------------------------------- */
static int utf8_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;
}

/* Display width in cells (one cell per code point; no wide-glyph support). */
static int text_width(const char *s)
{
    int n = 0;
    while (*s) {
        int l = utf8_len((unsigned char)*s), i = 0;
        while (i < l && s[i]) i++;
        s += i;
        n++;
    }
    return n;
}

static void put_cell(int x, int y, const char *s, int n, Color fg, Color bg)
{
    Cell *c;
    int i;
    if (x < 0 || x >= ui.fw || y < 0 || y >= ui.fh) return;
    c = &ui.cur[y][x];
    memset(c->g, 0, sizeof c->g);
    if ((unsigned char)s[0] < 0x20 || s[0] == 0x7F) { /* never forward control bytes */
        c->g[0] = '?';
    } else {
        for (i = 0; i < n && i < 4 && s[i]; i++) c->g[i] = s[i];
    }
    c->fg = fg;
    c->bg = bg;
}

static void put_glyph(int x, int y, const char *s, Color fg, Color bg)
{
    put_cell(x, y, s, utf8_len((unsigned char)s[0]), fg, bg);
}

static void fill_rect(int x, int y, int w, int h, Color bg)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) put_cell(x + i, y + j, " ", 1, ui.t->text, bg);
}

/* Draw at most maxw code points of a UTF-8 string. Returns cells drawn. */
static int draw_text_n(int x, int y, int maxw, const char *s, Color fg, Color bg)
{
    int w = 0;
    while (*s && w < maxw) {
        int l = utf8_len((unsigned char)*s), i = 0;
        while (i < l && s[i]) i++;
        put_cell(x + w, y, s, i, fg, bg);
        s += i;
        w++;
    }
    return w;
}

static int draw_text(int x, int y, const char *s, Color fg, Color bg)
{
    return draw_text_n(x, y, ui.fw, s, fg, bg);
}

/* ---- presenter ----------------------------------------------------------- */
static bool cell_eq(const Cell *a, const Cell *b)
{
    return color_eq(a->fg, b->fg) && color_eq(a->bg, b->bg) && memcmp(a->g, b->g, sizeof a->g) == 0;
}

/* Writes only the cells that changed since the last frame. */
static void ui_present(void)
{
    int x, y, px = -1, py = -1;
    bool have_fg = false, have_bg = false;
    Color lfg = { 0, 0, 0 }, lbg = { 0, 0, 0 };

    out_flush();
    emit("\033[?2026h"); /* synchronized update; ignored where unsupported */
    if (ui.force) emit("\033[0m\033[2J");
    for (y = 0; y < ui.fh; y++) {
        for (x = 0; x < ui.fw; x++) {
            const Cell *c = &ui.cur[y][x];
            if (!ui.force && cell_eq(c, &ui.prev[y][x])) continue;
            if (px != x || py != y) emit_move(x, y);
            if (!have_fg || !color_eq(c->fg, lfg)) { emit_color(true, c->fg); lfg = c->fg; have_fg = true; }
            if (!have_bg || !color_eq(c->bg, lbg)) { emit_color(false, c->bg); lbg = c->bg; have_bg = true; }
            emit(c->g);
            px = x + 1;
            py = y;
        }
    }
    emit("\033[0m");
#if defined(PLAT_STRICT)
    /* Line-input mode: park the cursor on a prompt row below the frame. */
    emit_move(0, ui.fh);
    emit("\033[2K" "key + Enter > ");
    ui.force = true; /* typing scrolls the screen: repaint fully every frame */
#else
    ui.force = false;
#endif
    emit("\033[?2026l");
    out_flush();
    fflush(stdout);
    memcpy(ui.prev, ui.cur, sizeof ui.prev);
}

/* Plain-text dump of the frame (no escapes). Used by --snapshot and tests. */
static void ui_dump(void)
{
    int x, y, last;
    for (y = 0; y < ui.fh; y++) {
        last = -1;
        for (x = 0; x < ui.fw; x++)
            if (ui.cur[y][x].g[0] != ' ') last = x;
        for (x = 0; x <= last; x++) fputs(ui.cur[y][x].g, stdout);
        fputc('\n', stdout);
    }
}

/* ===========================================================================
 * WIDGETS (immediate mode: call once per frame, state lives in your variables)
 * ========================================================================= */
#define ROW_H 2 /* vertical pitch of widget rows */

static void ui_status(const char *s)
{
    size_t n = strlen(s);
    if (n >= sizeof ui.status) n = sizeof ui.status - 1;
    memcpy(ui.status, s, n);
    ui.status[n] = '\0';
}

static int key_delta(void) /* -1 / 0 / +1 from adjust keys */
{
    switch (ui.key) {
    case KEY_LEFT: case 'h': case '-': return -1;
    case KEY_RIGHT: case 'l': case '+': case '=': return 1;
    default: return 0;
    }
}

static bool key_activate(void) { return ui.key == ' ' || ui.key == KEY_ENTER; }

/* Moves focus with the keys the previous frame left unconsumed. */
static void ui_navigate(void)
{
    int n = ui.nwidgets;
    bool used = true;
    if (n > 0) {
        switch (ui.key) {
        case 'j': case KEY_DOWN: case KEY_TAB: ui.focus = (ui.focus + 1) % n; break;
        case 'k': case KEY_UP: case KEY_BTAB: ui.focus = (ui.focus + n - 1) % n; break;
        case KEY_HOME: ui.focus = 0; break;
        case KEY_END: ui.focus = n - 1; break;
        default: used = false; break;
        }
        if (used) ui.key = KEY_NONE;
        if (ui.focus >= n) ui.focus = n - 1;
    }
}

/* Starts a frame: paints the canvas and a centred w x h window.
 * Returns false (and draws a hint) when the terminal is too small; skip the
 * widgets in that case but still call ui_end(). */
static bool ui_begin(int w, int h, const char *title)
{
    int i, j, x, y, tx;
    char msg[64];

    ui_navigate();
    ui.nwidgets = 0;
    fill_rect(0, 0, ui.fw, ui.fh, ui.t->canvas);
    ui.open = false;
    if (ui.fw < w + 2 || ui.fh < h + 1) {
        int l;
        snprintf(msg, sizeof msg, "Terminal too small: need %dx%d, have %dx%d",
                 w + 2, h + 1, ui.fw, ui.fh);
        l = text_width(msg);
        draw_text_n((ui.fw - l) / 2 > 0 ? (ui.fw - l) / 2 : 0, ui.fh / 2, ui.fw, msg,
                    ui.t->red, ui.t->canvas);
        return false;
    }
    x = (ui.fw - (w + 2)) / 2;
    y = (ui.fh - (h + 1)) / 2;
    ui.wx = x; ui.wy = y; ui.ww = w; ui.wh = h;
    ui.cx = x + 3; ui.cy = y + 2;
    ui.open = true;

    fill_rect(x + 2, y + 1, w, h, ui.t->shadow);
    fill_rect(x, y, w, h, ui.t->panel);
    put_glyph(x, y, G_TL, ui.t->border, ui.t->panel);
    put_glyph(x + w - 1, y, G_TR, ui.t->border, ui.t->panel);
    put_glyph(x, y + h - 1, G_BL, ui.t->border, ui.t->panel);
    put_glyph(x + w - 1, y + h - 1, G_BR, ui.t->border, ui.t->panel);
    for (i = 1; i < w - 1; i++) {
        put_glyph(x + i, y, G_H, ui.t->border, ui.t->panel);
        put_glyph(x + i, y + h - 1, G_H, ui.t->border, ui.t->panel);
    }
    for (j = 1; j < h - 1; j++) {
        put_glyph(x, y + j, G_V, ui.t->border, ui.t->panel);
        put_glyph(x + w - 1, y + j, G_V, ui.t->border, ui.t->panel);
    }
    put_glyph(x + 3, y, G_DOT, ui.t->red, ui.t->panel);
    put_glyph(x + 5, y, G_DOT, ui.t->yellow, ui.t->panel);
    put_glyph(x + 7, y, G_DOT, ui.t->green, ui.t->panel);
    tx = x + (w - text_width(title) - 4) / 2;
    put_glyph(tx + 1, y, G_STAR, ui.t->accent, ui.t->panel);
    draw_text_n(tx + 3, y, w - 14, title, ui.t->accent, ui.t->panel);
    return true;
}

/* Ends a frame: footer (status + key hints), focus clamp, key consumed. */
static void ui_end(void)
{
    static const char hint[] = "j/k Move  h/l Adjust  Enter Select  q Quit";
    if (ui.open) {
        int fy = ui.wy + ui.wh - 2;
        int hw = (int)sizeof hint - 1;
        int inner = ui.ww - 2;
        fill_rect(ui.wx + 1, fy, inner, 1, ui.t->surface);
        draw_text_n(ui.wx + 3, fy, inner - hw - 5, ui.status[0] ? ui.status : "Ready",
                    ui.t->green, ui.t->surface);
        if (inner > hw + 4)
            draw_text(ui.wx + ui.ww - hw - 3, fy, hint, ui.t->muted, ui.t->surface);
    }
    if (ui.focus >= ui.nwidgets) ui.focus = ui.nwidgets > 0 ? ui.nwidgets - 1 : 0;
    ui.key = KEY_NONE;
}

static int row_right(void) { return ui.cx + ui.ww - 6; } /* exclusive right edge */

/* Tab strip; the digit keys and PgUp/PgDn switch tabs. */
static void ui_tabs(const char *const *names, int count, int *active)
{
    int i, tx = ui.cx, old = *active;
    if (ui.key >= '1' && ui.key < '1' + count) { *active = ui.key - '1'; ui.key = KEY_NONE; }
    else if (ui.key == KEY_PGDN) { *active = (*active + 1) % count; ui.key = KEY_NONE; }
    else if (ui.key == KEY_PGUP) { *active = (*active + count - 1) % count; ui.key = KEY_NONE; }
    if (*active != old) { ui.focus = 0; ui.dirty = true; }
    for (i = 0; i < count; i++) {
        bool sel = (*active == i);
        Color bg = sel ? ui.t->accent : ui.t->surface;
        Color fg = sel ? ui.t->canvas : ui.t->muted;
        int len = text_width(names[i]) + 2; /* "N " prefix */
        char d[2];
        d[0] = (char)('1' + i); d[1] = '\0';
        fill_rect(tx, ui.cy, len + 2, 1, bg);
        draw_text(tx + 1, ui.cy, d, sel ? ui.t->canvas : ui.t->accent, bg);
        draw_text(tx + 3, ui.cy, names[i], fg, bg);
        tx += len + 3;
    }
    ui.cy += ROW_H;
}

static void ui_section(const char *title)
{
    int i, start = text_width(title) + 1;
    draw_text(ui.cx, ui.cy, title, ui.t->muted, ui.t->panel);
    for (i = start; i < ui.ww - 6; i++) put_glyph(ui.cx + i, ui.cy, G_H, ui.t->surface, ui.t->panel);
    ui.cy += ROW_H;
}

/* A focusable row: highlight, marker and label. Returns true when focused. */
static bool ui_row(const char *label)
{
    int id = ui.nwidgets++;
    bool focused = (ui.focus == id);
    Color bg = focused ? ui.t->surface : ui.t->panel;
    fill_rect(ui.cx, ui.cy, ui.ww - 6, 1, bg);
    put_glyph(ui.cx + 1, ui.cy, focused ? G_ARR : " ", ui.t->accent, bg);
    draw_text_n(ui.cx + 3, ui.cy, ui.ww - 6 - 3 - 24, label,
                focused ? ui.t->text : ui.t->muted, bg);
    return focused;
}

/* Non-focusable "label ... value" row. */
static void ui_info(const char *label, const char *value)
{
    int w = ui.ww - 6;
    int vw = text_width(value);
    int lw = w - 4 - vw;
    if (lw < 4) { lw = w - 4; vw = 0; }
    draw_text_n(ui.cx + 3, ui.cy, lw, label, ui.t->muted, ui.t->panel);
    if (vw > 0) draw_text(row_right() - vw - 1, ui.cy, value, ui.t->text, ui.t->panel);
    ui.cy += ROW_H;
}

static bool ui_toggle(const char *label, bool *val)
{
    bool focused = ui_row(label);
    bool changed = false;
    Color bg = focused ? ui.t->surface : ui.t->panel;
    Color pill = *val ? ui.t->green : ui.t->focus;
    int rx = row_right() - 7;
    if (focused && (key_activate() || key_delta() != 0)) {
        *val = !*val;
        changed = true;
        ui.dirty = true;
        pill = *val ? ui.t->green : ui.t->focus;
    }
    put_glyph(rx, ui.cy, *val ? " " : G_DOT, ui.t->text, pill);
    put_glyph(rx + 1, ui.cy, *val ? G_DOT : " ", ui.t->canvas, pill);
    draw_text(rx + 3, ui.cy, *val ? "ON " : "OFF", *val ? ui.t->green : ui.t->muted, bg);
    ui.cy += ROW_H;
    return changed;
}

static void draw_bar(int x, int y, int w, int filled, Color bg)
{
    int i;
    for (i = 0; i < w; i++) {
        if (i < filled)
            put_glyph(x + i, y, G_HH, color_mix(ui.t->accent, ui.t->green, i, w > 1 ? w - 1 : 1), bg);
        else
            put_glyph(x + i, y, G_H, ui.t->focus, bg);
    }
}

static bool ui_slider(const char *label, int *val, int min_v, int max_v, int step)
{
    enum { BAR = 18 };
    bool focused = ui_row(label);
    bool changed = false;
    Color bg = focused ? ui.t->surface : ui.t->panel;
    int range = max_v > min_v ? max_v - min_v : 1;
    int bx = row_right() - BAR - 6;
    char num[16];
    if (focused) {
        int d = key_delta();
        if (d != 0) {
            int nv = *val + d * (step > 0 ? step : 1);
            if (nv < min_v) nv = min_v;
            if (nv > max_v) nv = max_v;
            if (nv != *val) { *val = nv; changed = true; ui.dirty = true; }
        }
    }
    draw_bar(bx, ui.cy, BAR, (*val - min_v) * BAR / range, bg);
    snprintf(num, sizeof num, "%4d", *val);
    draw_text(bx + BAR + 1, ui.cy, num, ui.t->accent, bg);
    ui.cy += ROW_H;
    return changed;
}

/* Cycles through options with Left/Right (or Enter/Space). */
static bool ui_choice(const char *label, const char *const *opts, int n, int *idx)
{
    bool focused = ui_row(label);
    bool changed = false;
    Color bg = focused ? ui.t->surface : ui.t->panel;
    int w;
    if (n <= 0) return false;
    if (*idx < 0 || *idx >= n) *idx = 0;
    if (focused) {
        int d = key_delta();
        if (d == 0 && key_activate()) d = 1;
        if (d != 0) { *idx = (*idx + d + n) % n; changed = true; ui.dirty = true; }
    }
    w = text_width(opts[*idx]);
    put_glyph(row_right() - w - 5, ui.cy, G_LT, ui.t->accent, bg);
    draw_text(row_right() - w - 3, ui.cy, opts[*idx], ui.t->text, bg);
    put_glyph(row_right() - 2, ui.cy, G_GT, ui.t->accent, bg);
    ui.cy += ROW_H;
    return changed;
}

/* Read-only progress bar, value 0..100. */
static void ui_progress(const char *label, int pct)
{
    enum { BAR = 18 };
    char num[16];
    int bx = row_right() - BAR - 6;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    draw_text_n(ui.cx + 3, ui.cy, bx - ui.cx - 4, label, ui.t->muted, ui.t->panel);
    draw_bar(bx, ui.cy, BAR, pct * BAR / 100, ui.t->panel);
    snprintf(num, sizeof num, "%3d%%", pct);
    draw_text(bx + BAR + 2, ui.cy, num, ui.t->accent, ui.t->panel);
    ui.cy += ROW_H;
}

/* Returns true on the frame the button is activated. */
static bool ui_button(const char *label)
{
    int id = ui.nwidgets++;
    bool focused = (ui.focus == id);
    bool clicked = focused && key_activate();
    int bw = text_width(label) + 6;
    Color bg = focused ? ui.t->accent : ui.t->surface;
    Color fg = focused ? ui.t->canvas : ui.t->text;
    fill_rect(ui.cx + 3, ui.cy, bw, 1, bg);
    put_glyph(ui.cx + 1, ui.cy, focused ? G_ARR : " ", ui.t->accent, ui.t->panel);
    draw_text(ui.cx + 6, ui.cy, label, fg, bg);
    ui.cy += ROW_H;
    if (clicked) ui.dirty = true;
    return clicked;
}

/* ===========================================================================
 * BUILD INFORMATION (shown on the About tab)
 * ========================================================================= */
#define STR_(x) #x
#define STR(x) STR_(x)
#if defined(__INTEL_LLVM_COMPILER)
#define COMPILER_NAME "Intel oneAPI icx " STR(__INTEL_LLVM_COMPILER)
#elif defined(__INTEL_COMPILER)
#define COMPILER_NAME "Intel icc " STR(__INTEL_COMPILER)
#elif defined(__clang__)
#define COMPILER_NAME "clang " STR(__clang_major__) "." STR(__clang_minor__) "." STR(__clang_patchlevel__)
#elif defined(__TINYC__)
#define COMPILER_NAME "TinyCC " STR(__TINYC__)
#elif defined(__GNUC__)
#define COMPILER_NAME "gcc " STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
#define COMPILER_NAME "MSVC " STR(_MSC_VER)
#else
#define COMPILER_NAME "unknown ISO C99 compiler"
#endif

#if defined(_WIN32)
#define PLATFORM_NAME "Windows"
#elif defined(__CYGWIN__)
#define PLATFORM_NAME "Cygwin"
#elif defined(__MSYS__)
#define PLATFORM_NAME "MSYS"
#elif defined(__APPLE__)
#define PLATFORM_NAME "macOS"
#elif defined(__linux__)
#define PLATFORM_NAME "Linux"
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define PLATFORM_NAME "BSD"
#else
#define PLATFORM_NAME "generic"
#endif

#if defined(PLAT_STRICT)
#define INPUT_NAME "strict ISO C (key + Enter)"
#else
#define INPUT_NAME "raw keys"
#endif

/* ===========================================================================
 * APPLICATION: replace this section with your own program
 * ========================================================================= */
#define WIN_MIN_W 66
#define WIN_MAX_W 84
#define WIN_H     19

typedef struct {
    int tab;
    bool notifications, dark, reduce_motion;
    int volume, brightness, language, cpu, mem, load;
} App;

static const char *const TABS[4] = { "Overview", "Display", "Telemetry", "About" };
static const char *const LANGS[4] = { "English", "Deutsch", "Nederlands", ("Fran\xC3\xA7" "ais") };

static void app_defaults(App *a)
{
    a->tab = 0;
    a->notifications = true;
    a->dark = true;
    a->reduce_motion = false;
    a->volume = 70;
    a->brightness = 85;
    a->language = 0;
    a->cpu = 42;
    a->mem = 61;
    a->load = 40;
}

static void app_frame(App *a)
{
    int w = ui.fw - 4;
    if (w > WIN_MAX_W) w = WIN_MAX_W;
    if (w < WIN_MIN_W) w = WIN_MIN_W;
    ui.t = a->dark ? &THEME_DARK : &THEME_LIGHT;

    if (ui_begin(w, WIN_H, "C99 GUI Template")) {
        ui_tabs(TABS, 4, &a->tab);
        switch (a->tab) {
        case 0:
            ui_section("GENERAL");
            ui_toggle("Notifications", &a->notifications);
            ui_toggle("Dark theme", &a->dark);
            ui_slider("Volume", &a->volume, 0, 100, 5);
            ui_choice("Language", LANGS, 4, &a->language);
            if (ui_button("Apply")) ui_status("Settings applied.");
            break;
        case 1:
            ui_section("DISPLAY");
            ui_slider("Brightness", &a->brightness, 10, 100, 10);
            ui_toggle("Reduce motion", &a->reduce_motion);
            if (ui_button("Restore defaults")) {
                bool dark = a->dark;
                app_defaults(a);
                a->tab = 1;
                a->dark = dark;
                ui_status("Defaults restored.");
            }
            break;
        case 2:
            ui_section("REALTIME TELEMETRY");
            ui_progress("CPU", a->cpu);
            ui_progress("Memory", a->mem);
            ui_slider("Simulated load", &a->load, 0, 100, 5);
            if (ui_button("Run stress pulse")) {
                a->cpu = (a->cpu + a->load / 2 + 10) % 101;
                a->mem = (a->mem + a->load / 4 + 3) % 101;
                ui_status("Stress pulse executed.");
            }
            break;
        default: {
            char buf[48];
            ui_section("ABOUT");
            ui_info("Version", APP_NAME " " APP_VERSION);
            ui_info("Compiler", COMPILER_NAME);
            ui_info("Platform", PLATFORM_NAME);
            ui_info("Input", INPUT_NAME);
            snprintf(buf, sizeof buf, "%dx%d, %s", ui.fw, ui.fh,
                     ui.color256 ? "256 colors" : "truecolor");
            ui_info("Terminal", buf);
            break;
        }
        }
    }
    ui_end();
}

/* Run the frame; if any widget changed state, run it once more so the screen
 * never lags one keystroke behind (e.g. after switching theme or tab). */
static void render(App *a)
{
    app_frame(a);
    if (ui.dirty) {
        ui.dirty = false;
        ui.key = KEY_NONE;
        app_frame(a);
    }
}

/* ---------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
static void usage(void)
{
    puts("Usage: " APP_NAME " [--snapshot] [--256] [--help] [--version]\n"
         "\n"
         "  Keys:  j/k, Up/Down, Tab   move focus\n"
         "         h/l, Left/Right     adjust value / toggle\n"
         "         Space, Enter        activate\n"
         "         1-4, PgUp/PgDn      switch tab\n"
         "         q, Esc, Ctrl-C      quit\n"
         "\n"
         "  --snapshot   read keys from stdin, print the final 80x24 frame as\n"
         "               plain text, and exit (headless; used by the tests)\n"
         "  --256        force the 256-colour palette (default: truecolor)\n"
         "  C99GUI_COLOR=256|truecolor overrides colour detection");
}

static bool use_256_colors(void)
{
    const char *o = getenv("C99GUI_COLOR");
    const char *tp = getenv("TERM_PROGRAM");
    const char *t = getenv("TERM");
    if (o) return strcmp(o, "256") == 0;
    if (tp && strcmp(tp, "Apple_Terminal") == 0) return true; /* no truecolor */
    if (t && strcmp(t, "linux") == 0) return true;            /* kernel console */
    return false;
}

static bool is_quit(int k)
{
    return k == KEY_EOF || k == KEY_ESC || k == KEY_CTRL_C || k == 'q' || k == 'Q';
}

int main(int argc, char **argv)
{
    static App app;
    bool snapshot = false, force256 = false;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--snapshot")) snapshot = true;
        else if (!strcmp(argv[i], "--256")) force256 = true;
        else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V")) {
            puts(APP_NAME " " APP_VERSION);
            return 0;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage();
            return 0;
        } else {
            fprintf(stderr, APP_NAME ": unknown option '%s' (try --help)\n", argv[i]);
            return 2;
        }
    }

    app_defaults(&app);
    ui.t = &THEME_DARK;
    ui.color256 = force256 || use_256_colors();

    if (snapshot) {
        int k;
        ui.fw = 80;
        ui.fh = 24;
        for (;;) {
            render(&app);
            k = vt_key(stdio_rd, stdio_wait);
            if (is_quit(k)) break;
            ui.key = k;
        }
        ui_dump();
        return 0;
    }

    sys_init();
    term_enter();
    for (;;) {
        int w, h, k;
        sys_size(&w, &h);
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        if (w > FB_MAX_W) w = FB_MAX_W;
        if (h > FB_MAX_H) h = FB_MAX_H;
        if (w != ui.fw || h != ui.fh) {
            ui.fw = w;
            ui.fh = h;
            ui.force = true;
        }
        render(&app);
        ui_present();
        k = sys_key();
        if (is_quit(k)) break;
        ui.key = k;
    }
    sys_shutdown();
    return 0;
}
