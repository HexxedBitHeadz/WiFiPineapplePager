/*
 * pager_gfx.h — Framebuffer Graphics Library for WiFi Pineapple Pager
 * Target: WiFi Pineapple Pager (480x222, 16-bit RGB565 display)
 *
 * Single-header library. Define PAGER_GFX_IMPLEMENTATION in exactly ONE
 * .c file before including this header to get the implementation.
 *
 * Usage:
 *   #define PAGER_GFX_IMPLEMENTATION
 *   #include "pager_gfx.h"
 */

#ifndef PAGER_GFX_H
#define PAGER_GFX_H

#include <stdint.h>
#include <stddef.h>

/* ── Display Constants ──────────────────────────────────────────────── */

#define SCREEN_W       480
#define SCREEN_H       222
#define SCREEN_BPP     16
#define SCREEN_STRIDE   (SCREEN_W * 2)   /* bytes per row */
#define SCREEN_SIZE     (SCREEN_W * SCREEN_H * 2)  /* ~208 KB */

/* ── RGB565 Color Macros ────────────────────────────────────────────── */

#define RGB565(r, g, b) \
    ((uint16_t)(((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

#define RGB565_R(c) (((c) >> 11) & 0x1F)
#define RGB565_G(c) (((c) >> 5)  & 0x3F)
#define RGB565_B(c) ((c)         & 0x1F)

/* Expand RGB565 back to 8-bit channels */
#define RGB565_R8(c) ((RGB565_R(c) << 3) | (RGB565_R(c) >> 2))
#define RGB565_G8(c) ((RGB565_G(c) << 2) | (RGB565_G(c) >> 4))
#define RGB565_B8(c) ((RGB565_B(c) << 3) | (RGB565_B(c) >> 2))

/* ── Predefined Colors ──────────────────────────────────────────────── */

#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_YELLOW      0xFFE0
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_ORANGE      0xFD20
#define COLOR_GRAY        0x8410
#define COLOR_DARK_GRAY   0x4208
#define COLOR_LIGHT_GRAY  0xC618
#define COLOR_DARK_GREEN  0x03E0
#define COLOR_DARK_RED    0x7800
#define COLOR_DARK_BLUE   0x0010
#define COLOR_PURPLE      0x780F
#define COLOR_PINK        0xF81F
#define COLOR_BROWN       0x8200
#define COLOR_LIME        0x87E0
#define COLOR_NAVY        0x000F
#define COLOR_TEAL        0x0410
#define COLOR_OLIVE       0x8400
#define COLOR_MAROON      0x8000

/* Hak5/Pineapple themed colors */
#define COLOR_HAK5_GREEN  RGB565(0x00, 0xCC, 0x66)
#define COLOR_HAK5_DARK   RGB565(0x1A, 0x1A, 0x2E)
#define COLOR_HAK5_BLUE   RGB565(0x16, 0x21, 0x3E)
#define COLOR_TERMINAL    RGB565(0x00, 0xFF, 0x41)

/* ── Sprite Structure ───────────────────────────────────────────────── */

typedef struct {
    int      width;
    int      height;
    uint16_t transparent;   /* transparent color key (use 0xFFFF for none) */
    const uint16_t *data;   /* pixel data in RGB565 */
} Sprite;

/* ── Rect Structure ─────────────────────────────────────────────────── */

typedef struct {
    int x, y, w, h;
} Rect;

/* ── Graphics Context ───────────────────────────────────────────────── */

typedef struct {
    int       fb_fd;         /* framebuffer file descriptor */
    uint16_t *fb_ptr;        /* mmap'd framebuffer */
    uint16_t *backbuf;       /* off-screen back buffer */
    uint16_t *prevbuf;       /* previous frame for dirty-checking */
    int       width;         /* virtual width  (game sees this — always landscape) */
    int       height;        /* virtual height (game sees this — always landscape) */
    int       stride;        /* bytes per row in the real FB */
    int       fb_width;      /* real FB xres */
    int       fb_height;     /* real FB yres */
    int       rotated;       /* 1 if we detected portrait FB and rotate to landscape */
    int       frame_dirty;   /* 1 if backbuf differs from prevbuf */
} GfxContext;

/* ── Function Prototypes ────────────────────────────────────────────── */

/* Lifecycle */
int  gfx_init(GfxContext *ctx);
void gfx_cleanup(GfxContext *ctx);
void gfx_flip(GfxContext *ctx);

/* Primitives */
void gfx_clear(GfxContext *ctx, uint16_t color);
void gfx_pixel(GfxContext *ctx, int x, int y, uint16_t color);
void gfx_line(GfxContext *ctx, int x0, int y0, int x1, int y1, uint16_t color);
void gfx_hline(GfxContext *ctx, int x, int y, int w, uint16_t color);
void gfx_vline(GfxContext *ctx, int x, int y, int h, uint16_t color);
void gfx_rect(GfxContext *ctx, int x, int y, int w, int h, uint16_t color);
void gfx_rect_fill(GfxContext *ctx, int x, int y, int w, int h, uint16_t color);
void gfx_circle(GfxContext *ctx, int cx, int cy, int r, uint16_t color);
void gfx_circle_fill(GfxContext *ctx, int cx, int cy, int r, uint16_t color);
void gfx_triangle(GfxContext *ctx, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);

/* Sprites */
void gfx_blit(GfxContext *ctx, const Sprite *sprite, int x, int y);
void gfx_blit_scaled(GfxContext *ctx, const Sprite *sprite, int x, int y, int scale);

/* Text (built-in 8x8 font, scaled by FONT_SCALE) */
#ifndef FONT_SCALE
#define FONT_SCALE 2    /* Default 2x scale: 16x16 chars (30 cols x 13 rows) */
#endif
#define CHAR_W  (8 * FONT_SCALE)
#define CHAR_H  (8 * FONT_SCALE)
#define LINE_H  (CHAR_H + 2 * FONT_SCALE)  /* char height + spacing */

void gfx_char(GfxContext *ctx, int x, int y, char c, uint16_t color);
void gfx_char_scaled(GfxContext *ctx, int x, int y, char c, uint16_t color, int scale);
void gfx_text(GfxContext *ctx, int x, int y, const char *str, uint16_t color);
void gfx_text_scaled(GfxContext *ctx, int x, int y, const char *str, uint16_t color, int scale);
void gfx_text_centered(GfxContext *ctx, int y, const char *str, uint16_t color);
void gfx_text_centered_scaled(GfxContext *ctx, int y, const char *str, uint16_t color, int scale);
void gfx_printf(GfxContext *ctx, int x, int y, uint16_t color, const char *fmt, ...);
void gfx_printf_scaled(GfxContext *ctx, int x, int y, uint16_t color, int scale, const char *fmt, ...);

/* Utility */
uint16_t gfx_blend(uint16_t fg, uint16_t bg, uint8_t alpha);
int  gfx_rect_intersect(const Rect *a, const Rect *b);
void gfx_gradient_h(GfxContext *ctx, int x, int y, int w, int h, uint16_t c1, uint16_t c2);
void gfx_gradient_v(GfxContext *ctx, int x, int y, int w, int h, uint16_t c1, uint16_t c2);

#endif /* PAGER_GFX_H */

/* ════════════════════════════════════════════════════════════════════ */
/*                         IMPLEMENTATION                              */
/* ════════════════════════════════════════════════════════════════════ */

#ifdef PAGER_GFX_IMPLEMENTATION
#undef PAGER_GFX_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

/* ── Built-in 8x8 Font (printable ASCII 32-126) ────────────────────── */

static const uint8_t font8x8_basic[128][8] = {
    /* 0x00-0x1F: control characters (blank) */
    [0 ... 31] = {0,0,0,0,0,0,0,0},
    /* 0x20 SPACE */
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['!'] = {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    ['"'] = {0x36,0x36,0x14,0x00,0x00,0x00,0x00,0x00},
    ['#'] = {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    ['$'] = {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},
    ['%'] = {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    ['&'] = {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},
    ['\'']= {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    ['('] = {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},
    [')'] = {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    ['*'] = {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    ['+'] = {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    [','] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06},
    ['-'] = {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    ['.'] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},
    ['/'] = {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    ['0'] = {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},
    ['1'] = {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    ['2'] = {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},
    ['3'] = {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    ['4'] = {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},
    ['5'] = {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    ['6'] = {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},
    ['7'] = {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    ['8'] = {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},
    ['9'] = {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    [':'] = {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00},
    [';'] = {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    ['<'] = {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},
    ['='] = {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    ['>'] = {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    ['?'] = {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    ['@'] = {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},
    ['A'] = {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    ['B'] = {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},
    ['C'] = {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    ['D'] = {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},
    ['E'] = {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    ['F'] = {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},
    ['G'] = {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    ['H'] = {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},
    ['I'] = {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    ['J'] = {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},
    ['K'] = {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    ['L'] = {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},
    ['M'] = {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    ['N'] = {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},
    ['O'] = {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    ['P'] = {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},
    ['Q'] = {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    ['R'] = {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},
    ['S'] = {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    ['T'] = {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    ['U'] = {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    ['V'] = {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},
    ['W'] = {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    ['X'] = {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},
    ['Y'] = {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    ['Z'] = {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
    ['['] = {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    ['\\']= {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    [']'] = {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    ['^'] = {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},
    ['_'] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    ['`'] = {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00},
    ['a'] = {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00},
    ['b'] = {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00},
    ['c'] = {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
    ['d'] = {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00},
    ['e'] = {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00},
    ['f'] = {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00},
    ['g'] = {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    ['h'] = {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00},
    ['i'] = {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
    ['j'] = {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E},
    ['k'] = {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
    ['l'] = {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    ['m'] = {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
    ['n'] = {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00},
    ['o'] = {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
    ['p'] = {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F},
    ['q'] = {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
    ['r'] = {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00},
    ['s'] = {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
    ['t'] = {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00},
    ['u'] = {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
    ['v'] = {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00},
    ['w'] = {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    ['x'] = {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00},
    ['y'] = {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    ['z'] = {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00},
    ['{'] = {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
    ['|'] = {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
    ['}'] = {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
    ['~'] = {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},
    [0x7F] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

/* ── Lifecycle ──────────────────────────────────────────────────────── */

int gfx_init(GfxContext *ctx) {
    memset(ctx, 0, sizeof(GfxContext));

    ctx->fb_fd = open("/dev/fb0", O_RDWR);
    if (ctx->fb_fd < 0) {
        perror("gfx_init: open /dev/fb0");
        return -1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(ctx->fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(ctx->fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("gfx_init: ioctl");
        close(ctx->fb_fd);
        return -1;
    }

    ctx->fb_width  = vinfo.xres;
    ctx->fb_height = vinfo.yres;
    ctx->stride    = finfo.line_length;

    /* Detect portrait FB and present landscape to game code */
    if (ctx->fb_width < ctx->fb_height) {
        ctx->rotated = 1;
        ctx->width   = ctx->fb_height;   /* 480 */
        ctx->height  = ctx->fb_width;    /* 222 */
        fprintf(stderr, "[gfx] Portrait FB %dx%d detected — rotating to %dx%d landscape\n",
                ctx->fb_width, ctx->fb_height, ctx->width, ctx->height);
    } else {
        ctx->rotated = 0;
        ctx->width   = ctx->fb_width;
        ctx->height  = ctx->fb_height;
    }

    size_t fb_size = ctx->stride * ctx->fb_height;
    ctx->fb_ptr = (uint16_t *)mmap(NULL, fb_size,
                                   PROT_READ | PROT_WRITE, MAP_SHARED,
                                   ctx->fb_fd, 0);
    if (ctx->fb_ptr == MAP_FAILED) {
        perror("gfx_init: mmap");
        close(ctx->fb_fd);
        return -1;
    }

    /* Back buffer is in virtual (game) dimensions */
    ctx->backbuf = (uint16_t *)calloc(ctx->width * ctx->height, sizeof(uint16_t));
    if (!ctx->backbuf) {
        perror("gfx_init: calloc backbuf");
        munmap(ctx->fb_ptr, fb_size);
        close(ctx->fb_fd);
        return -1;
    }

    /* Previous frame buffer for dirty-checking (skip flip when unchanged) */
    ctx->prevbuf = (uint16_t *)calloc(ctx->width * ctx->height, sizeof(uint16_t));
    if (!ctx->prevbuf) {
        perror("gfx_init: calloc prevbuf");
        free(ctx->backbuf);
        munmap(ctx->fb_ptr, fb_size);
        close(ctx->fb_fd);
        return -1;
    }
    ctx->frame_dirty = 1; /* first frame is always dirty */

    fprintf(stderr, "[gfx] Initialized: %dx%d @ %d bpp (stride=%d, rotated=%d)\n",
            ctx->width, ctx->height, vinfo.bits_per_pixel, ctx->stride, ctx->rotated);
    return 0;
}

void gfx_cleanup(GfxContext *ctx) {
    if (ctx->backbuf) {
        free(ctx->backbuf);
        ctx->backbuf = NULL;
    }
    if (ctx->prevbuf) {
        free(ctx->prevbuf);
        ctx->prevbuf = NULL;
    }
    if (ctx->fb_ptr && ctx->fb_ptr != MAP_FAILED) {
        munmap(ctx->fb_ptr, ctx->stride * ctx->fb_height);
        ctx->fb_ptr = NULL;
    }
    if (ctx->fb_fd >= 0) {
        close(ctx->fb_fd);
        ctx->fb_fd = -1;
    }
}

void gfx_flip(GfxContext *ctx) {
    /*
     * Dirty-line partial update: instead of blitting the entire framebuffer,
     * only copy scanlines that actually changed. This reduces the write
     * window and minimizes tearing on SPI displays without lowering FPS.
     */
    int w = ctx->width;
    int h = ctx->height;
    size_t row_bytes = (size_t)w * sizeof(uint16_t);

    /* Find dirty line range */
    int dirty_top = -1, dirty_bot = -1;
    for (int y = 0; y < h; y++) {
        if (memcmp(ctx->backbuf + y * w, ctx->prevbuf + y * w, row_bytes) != 0) {
            if (dirty_top < 0) dirty_top = y;
            dirty_bot = y;
        }
    }

    /* Nothing changed — skip entirely */
    if (dirty_top < 0) return;

    /* Save current frame for next comparison */
    memcpy(ctx->prevbuf + dirty_top * w,
           ctx->backbuf + dirty_top * w,
           (size_t)(dirty_bot - dirty_top + 1) * row_bytes);

    /* Try vsync if available (silently ignored if unsupported) */
    {
        int zero = 0;
        ioctl(ctx->fb_fd, FBIO_WAITFORVSYNC, &zero);
    }
    if (!ctx->rotated) {
        /* No rotation — copy only dirty lines */
        if (ctx->stride == w * 2) {
            memcpy((uint8_t *)ctx->fb_ptr + dirty_top * row_bytes,
                   ctx->backbuf + dirty_top * w,
                   (size_t)(dirty_bot - dirty_top + 1) * row_bytes);
        } else {
            for (int y = dirty_top; y <= dirty_bot; y++) {
                memcpy((uint8_t *)ctx->fb_ptr + y * ctx->stride,
                       ctx->backbuf + y * w,
                       row_bytes);
            }
        }
    } else {
        /*
         * Rotate 90° CW: game landscape (480×222) → FB portrait (222×480)
         *
         * Game (gx, gy) → FB (fb_x, fb_y):
         *   fb_x = (fb_width - 1) - gy   = 221 - gy
         *   fb_y = gx
         *
         * Only rebuild the rotated columns corresponding to dirty game rows.
         * Since a dirty game row y maps to FB column x = (fb_w-1-y), we
         * still need to write full FB rows that touch those columns.
         * But we can limit the rotation work to only dirty source rows.
         */
        int fb_w = ctx->fb_width;   /* 222 */
        int fb_h = ctx->fb_height;  /* 480 */
        int game_w = ctx->width;    /* 480 */
        int game_h = ctx->height;   /* 222 */
        size_t fb_pixels = (size_t)fb_w * fb_h;

        /* Use a static temp buffer to avoid malloc/free every frame */
        static uint16_t *rot_buf = NULL;
        static size_t rot_buf_size = 0;
        if (!rot_buf || rot_buf_size != fb_pixels) {
            free(rot_buf);
            rot_buf = (uint16_t *)malloc(fb_pixels * sizeof(uint16_t));
            rot_buf_size = fb_pixels;
        }

        /* Build rotated image — only process dirty game rows */
        for (int gy = dirty_top; gy <= dirty_bot; gy++) {
            const uint16_t *src_row = ctx->backbuf + gy * game_w;
            int fb_x = (fb_w - 1) - gy;
            for (int gx = 0; gx < game_w; gx++) {
                rot_buf[gx * fb_w + fb_x] = src_row[gx];
            }
        }

        /*
         * Dirty game rows [dirty_top..dirty_bot] map to FB columns
         * [(fb_w-1-dirty_bot)..(fb_w-1-dirty_top)]. Every FB row
         * that spans those columns needs updating, which is all rows
         * (0..fb_h-1) since FB rows span the full width. But we only
         * need to write the dirty column range within each row.
         *
         * For simplicity and to keep the write as sequential as
         * possible, blast the whole rotated buffer. The rotation
         * work itself is already reduced.
         */
        if (ctx->stride == fb_w * 2) {
            memcpy(ctx->fb_ptr, rot_buf, fb_pixels * sizeof(uint16_t));
        } else {
            int fb_stride_px = ctx->stride / 2;
            for (int y = 0; y < fb_h; y++) {
                memcpy(ctx->fb_ptr + y * fb_stride_px,
                       rot_buf + y * fb_w,
                       fb_w * sizeof(uint16_t));
            }
        }
    }
}

/* ── Pixel Helpers ──────────────────────────────────────────────────── */

static inline void _put_pixel(GfxContext *ctx, int x, int y, uint16_t color) {
    if (x >= 0 && x < ctx->width && y >= 0 && y < ctx->height)
        ctx->backbuf[y * ctx->width + x] = color;
}

void gfx_pixel(GfxContext *ctx, int x, int y, uint16_t color) {
    _put_pixel(ctx, x, y, color);
}

/* ── Clear ──────────────────────────────────────────────────────────── */

void gfx_clear(GfxContext *ctx, uint16_t color) {
    int total = ctx->width * ctx->height;
    if (color == 0) {
        memset(ctx->backbuf, 0, total * sizeof(uint16_t));
    } else {
        for (int i = 0; i < total; i++)
            ctx->backbuf[i] = color;
    }
}

/* ── Lines ──────────────────────────────────────────────────────────── */

void gfx_hline(GfxContext *ctx, int x, int y, int w, uint16_t color) {
    for (int i = 0; i < w; i++)
        _put_pixel(ctx, x + i, y, color);
}

void gfx_vline(GfxContext *ctx, int x, int y, int h, uint16_t color) {
    for (int i = 0; i < h; i++)
        _put_pixel(ctx, x, y + i, color);
}

void gfx_line(GfxContext *ctx, int x0, int y0, int x1, int y1, uint16_t color) {
    /* Bresenham's line algorithm */
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        _put_pixel(ctx, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ── Rectangles ─────────────────────────────────────────────────────── */

void gfx_rect(GfxContext *ctx, int x, int y, int w, int h, uint16_t color) {
    gfx_hline(ctx, x, y, w, color);
    gfx_hline(ctx, x, y + h - 1, w, color);
    gfx_vline(ctx, x, y, h, color);
    gfx_vline(ctx, x + w - 1, y, h, color);
}

void gfx_rect_fill(GfxContext *ctx, int x, int y, int w, int h, uint16_t color) {
    for (int row = y; row < y + h; row++)
        gfx_hline(ctx, x, row, w, color);
}

/* ── Circles ────────────────────────────────────────────────────────── */

void gfx_circle(GfxContext *ctx, int cx, int cy, int r, uint16_t color) {
    /* Midpoint circle algorithm */
    int x = r, y = 0;
    int err = 1 - r;

    while (x >= y) {
        _put_pixel(ctx, cx + x, cy + y, color);
        _put_pixel(ctx, cx + y, cy + x, color);
        _put_pixel(ctx, cx - y, cy + x, color);
        _put_pixel(ctx, cx - x, cy + y, color);
        _put_pixel(ctx, cx - x, cy - y, color);
        _put_pixel(ctx, cx - y, cy - x, color);
        _put_pixel(ctx, cx + y, cy - x, color);
        _put_pixel(ctx, cx + x, cy - y, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void gfx_circle_fill(GfxContext *ctx, int cx, int cy, int r, uint16_t color) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)(sqrt((double)(r * r - dy * dy)) + 0.5);
        gfx_hline(ctx, cx - dx, cy + dy, 2 * dx + 1, color);
    }
}

/* ── Triangle ───────────────────────────────────────────────────────── */

void gfx_triangle(GfxContext *ctx, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    gfx_line(ctx, x0, y0, x1, y1, color);
    gfx_line(ctx, x1, y1, x2, y2, color);
    gfx_line(ctx, x2, y2, x0, y0, color);
}

/* ── Sprites ────────────────────────────────────────────────────────── */

void gfx_blit(GfxContext *ctx, const Sprite *sprite, int x, int y) {
    for (int sy = 0; sy < sprite->height; sy++) {
        for (int sx = 0; sx < sprite->width; sx++) {
            uint16_t pixel = sprite->data[sy * sprite->width + sx];
            if (pixel != sprite->transparent)
                _put_pixel(ctx, x + sx, y + sy, pixel);
        }
    }
}

void gfx_blit_scaled(GfxContext *ctx, const Sprite *sprite, int x, int y, int scale) {
    for (int sy = 0; sy < sprite->height; sy++) {
        for (int sx = 0; sx < sprite->width; sx++) {
            uint16_t pixel = sprite->data[sy * sprite->width + sx];
            if (pixel != sprite->transparent) {
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        _put_pixel(ctx, x + sx * scale + dx, y + sy * scale + dy, pixel);
            }
        }
    }
}

/* ── Text ───────────────────────────────────────────────────────────── */

void gfx_char_scaled(GfxContext *ctx, int x, int y, char c, uint16_t color, int scale) {
    if (c < 0 || c > 127) c = '?';
    const uint8_t *glyph = font8x8_basic[(int)c];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col)) {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        _put_pixel(ctx, x + col * scale + sx, y + row * scale + sy, color);
            }
        }
    }
}

void gfx_char(GfxContext *ctx, int x, int y, char c, uint16_t color) {
    gfx_char_scaled(ctx, x, y, c, color, FONT_SCALE);
}

void gfx_text_scaled(GfxContext *ctx, int x, int y, const char *str, uint16_t color, int scale) {
    int orig_x = x;
    int cw = 8 * scale;
    int lh = cw + 2 * scale;  /* char height + spacing */
    while (*str) {
        if (*str == '\n') {
            x = orig_x;
            y += lh;
        } else {
            gfx_char_scaled(ctx, x, y, *str, color, scale);
            x += cw;
        }
        str++;
    }
}

void gfx_text(GfxContext *ctx, int x, int y, const char *str, uint16_t color) {
    gfx_text_scaled(ctx, x, y, str, color, FONT_SCALE);
}

void gfx_text_centered(GfxContext *ctx, int y, const char *str, uint16_t color) {
    int len = (int)strlen(str);
    int x = (ctx->width - len * CHAR_W) / 2;
    gfx_text(ctx, x, y, str, color);
}

void gfx_text_centered_scaled(GfxContext *ctx, int y, const char *str, uint16_t color, int scale) {
    int len = (int)strlen(str);
    int cw = 8 * scale;
    int x = (ctx->width - len * cw) / 2;
    gfx_text_scaled(ctx, x, y, str, color, scale);
}

void gfx_printf(GfxContext *ctx, int x, int y, uint16_t color, const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    gfx_text(ctx, x, y, buf, color);
}

void gfx_printf_scaled(GfxContext *ctx, int x, int y, uint16_t color, int scale, const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    gfx_text_scaled(ctx, x, y, buf, color, scale);
}

/* ── Utility ────────────────────────────────────────────────────────── */

uint16_t gfx_blend(uint16_t fg, uint16_t bg, uint8_t alpha) {
    uint8_t inv = 255 - alpha;
    uint32_t fg_rb = fg & 0xF81F;
    uint32_t fg_g  = fg & 0x07E0;
    uint32_t bg_rb = bg & 0xF81F;
    uint32_t bg_g  = bg & 0x07E0;
    uint32_t rb = ((fg_rb * alpha + bg_rb * inv) >> 8) & 0xF81F;
    uint32_t g  = ((fg_g  * alpha + bg_g  * inv) >> 8) & 0x07E0;
    return (uint16_t)(rb | g);
}

int gfx_rect_intersect(const Rect *a, const Rect *b) {
    return !(a->x + a->w <= b->x || b->x + b->w <= a->x ||
             a->y + a->h <= b->y || b->y + b->h <= a->y);
}

void gfx_gradient_h(GfxContext *ctx, int x, int y, int w, int h, uint16_t c1, uint16_t c2) {
    for (int col = 0; col < w; col++) {
        uint8_t t = (uint8_t)((col * 255) / (w - 1));
        uint16_t c = gfx_blend(c2, c1, t);
        gfx_vline(ctx, x + col, y, h, c);
    }
}

void gfx_gradient_v(GfxContext *ctx, int x, int y, int w, int h, uint16_t c1, uint16_t c2) {
    for (int row = 0; row < h; row++) {
        uint8_t t = (uint8_t)((row * 255) / (h - 1));
        uint16_t c = gfx_blend(c2, c1, t);
        gfx_hline(ctx, x, y + row, w, c);
    }
}

#endif /* PAGER_GFX_IMPLEMENTATION */
