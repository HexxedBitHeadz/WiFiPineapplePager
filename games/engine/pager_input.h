/*
 * pager_input.h — Input Handling Library for WiFi Pineapple Pager
 * Target: WiFi Pineapple Pager (D-pad + A/B + Power via evdev)
 *
 * Buttons: D-pad (Up/Down/Left/Right), A (right), B (left), Power
 *
 * Single-header library. Define PAGER_INPUT_IMPLEMENTATION in exactly ONE
 * .c file before including this header to get the implementation.
 *
 * Usage:
 *   #define PAGER_INPUT_IMPLEMENTATION
 *   #include "pager_input.h"
 */

#ifndef PAGER_INPUT_H
#define PAGER_INPUT_H

#include <stdint.h>

/* ── Button Definitions ─────────────────────────────────────────────── */

/* Bitmask positions for button state — matches pagerctl constants */
#define BTN_UP       (1 << 0)
#define BTN_DOWN     (1 << 1)
#define BTN_LEFT     (1 << 2)
#define BTN_RIGHT    (1 << 3)
#define BTN_A        (1 << 4)   /* Right button (confirm/action) */
#define BTN_B        (1 << 5)   /* Left button (cancel/back) */
#define BTN_POWER    (1 << 6)   /* Power button */
#define BTN_DPAD     (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)
#define BTN_ANY      (BTN_DPAD | BTN_A | BTN_B | BTN_POWER)

/* Legacy alias for backward compatibility with older games */
#define BTN_SELECT   BTN_A

/* ── Input Context ──────────────────────────────────────────────────── */

typedef struct {
    int      fd;             /* /dev/input/eventX file descriptor */
    uint8_t  held;           /* currently held buttons (bitmask) */
    uint8_t  pressed;        /* just pressed this frame (bitmask) */
    uint8_t  released;       /* just released this frame (bitmask) */
    uint8_t  _prev;          /* previous frame state (internal) */
} InputContext;

/* ── Function Prototypes ────────────────────────────────────────────── */

/*
 * input_init — Open the input device.
 * Pass NULL for device_path to auto-detect, or specify e.g. "/dev/input/event0".
 * Returns 0 on success, -1 on failure.
 */
int  input_init(InputContext *ctx, const char *device_path);

/*
 * input_poll — Read all pending events and update button states.
 * Call once per frame before checking button states.
 */
void input_poll(InputContext *ctx);

/* Button state queries (convenience wrappers around bitmask checks) */
int  input_held(const InputContext *ctx, uint8_t button);
int  input_pressed(const InputContext *ctx, uint8_t button);
int  input_released(const InputContext *ctx, uint8_t button);

/*
 * input_wait_any — Block until any button is pressed.
 * Returns the button bitmask of the pressed button.
 */
uint8_t input_wait_any(InputContext *ctx);

/*
 * input_cleanup — Close the input device.
 */
void input_cleanup(InputContext *ctx);

#endif /* PAGER_INPUT_H */

/* ════════════════════════════════════════════════════════════════════ */
/*                         IMPLEMENTATION                              */
/* ════════════════════════════════════════════════════════════════════ */

#ifdef PAGER_INPUT_IMPLEMENTATION
#undef PAGER_INPUT_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <linux/input.h>

/*
 * Fix macro collision: linux/input.h redefines BTN_A, BTN_B, BTN_LEFT,
 * BTN_RIGHT, BTN_SELECT as raw evdev keycodes (0x130, 0x131, 0x110, etc.)
 * which clobber our bitmask definitions. Save the linux keycodes we need
 * under private names, then restore our bitmask values.
 *
 * Pager hardware mapping (confirmed via evdev):
 *   Green button → keycode 305 (BTN_EAST)
 *   Red   button → keycode 304 (BTN_SOUTH)
 */
#define _EVCODE_BTN_SOUTH  0x130   /* 304 — Red button on Pager */
#define _EVCODE_BTN_EAST   0x131   /* 305 — Green button on Pager */

#undef BTN_A
#undef BTN_B
#undef BTN_LEFT
#undef BTN_RIGHT
#undef BTN_SELECT
#undef BTN_DPAD
#undef BTN_ANY

#define BTN_UP       (1 << 0)
#define BTN_DOWN     (1 << 1)
#define BTN_LEFT     (1 << 2)
#define BTN_RIGHT    (1 << 3)
#define BTN_A        (1 << 4)
#define BTN_B        (1 << 5)
#define BTN_POWER    (1 << 6)
#define BTN_DPAD     (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)
#define BTN_ANY      (BTN_DPAD | BTN_A | BTN_B | BTN_POWER)
#define BTN_SELECT   BTN_A

/* ── Key Code → Button Mapping ──────────────────────────────────────── */

static uint8_t _keycode_to_btn(uint16_t code) {
    switch (code) {
        case KEY_UP:            return BTN_UP;
        case KEY_DOWN:          return BTN_DOWN;
        case KEY_LEFT:          return BTN_LEFT;
        case KEY_RIGHT:         return BTN_RIGHT;
        /* A button — green button on Pager (keycode 305 / BTN_EAST) */
        case KEY_ENTER:         return BTN_A;
        case _EVCODE_BTN_EAST:  return BTN_A;
        /* B button — red button on Pager (keycode 304 / BTN_SOUTH) */
        case KEY_ESC:           return BTN_B;
        case KEY_BACKSPACE:     return BTN_B;
        case _EVCODE_BTN_SOUTH: return BTN_B;
        /* Power button */
        case KEY_POWER:         return BTN_POWER;
        default:                return 0;
    }
}

/* ── Auto-Detect Input Device ───────────────────────────────────────── */

static int _find_input_device(char *path, size_t path_size) {
    /*
     * Walk /dev/input/event* and look for a device that reports
     * KEY_UP / KEY_DOWN / KEY_ENTER (the D-pad + Select).
     * Uses EVIOCGBIT to check supported key codes.
     */
    DIR *dir = opendir("/dev/input");
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        snprintf(path, path_size, "/dev/input/%s", entry->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        /* Check if device supports EV_KEY */
        unsigned long evbits[2] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits) >= 0) {
            if (evbits[0] & (1UL << EV_KEY)) {
                /* Check for arrow keys */
                unsigned long keybits[KEY_MAX / (8 * sizeof(long)) + 1];
                memset(keybits, 0, sizeof(keybits));
                if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0) {
                    int has_up    = (keybits[KEY_UP / (8*sizeof(long))]    >> (KEY_UP    % (8*sizeof(long)))) & 1;
                    int has_down  = (keybits[KEY_DOWN / (8*sizeof(long))]  >> (KEY_DOWN  % (8*sizeof(long)))) & 1;
                    /* Check for KEY_ENTER or BTN_EAST (Pager green button = keycode 305) */
                    int has_enter = (keybits[KEY_ENTER / (8*sizeof(long))] >> (KEY_ENTER % (8*sizeof(long)))) & 1;
                    int has_btn_a = (keybits[_EVCODE_BTN_EAST / (8*sizeof(long))] >> (_EVCODE_BTN_EAST % (8*sizeof(long)))) & 1;

                    if (has_up && has_down && (has_enter || has_btn_a)) {
                        close(fd);
                        closedir(dir);
                        fprintf(stderr, "[input] Auto-detected device: %s\n", path);
                        return 0;
                    }
                }
            }
        }
        close(fd);
    }

    closedir(dir);
    return -1;
}

/* ── Lifecycle ──────────────────────────────────────────────────────── */

int input_init(InputContext *ctx, const char *device_path) {
    memset(ctx, 0, sizeof(InputContext));
    ctx->fd = -1;

    char path[280];
    if (device_path) {
        strncpy(path, device_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        if (_find_input_device(path, sizeof(path)) < 0) {
            /* Fallback to event0 */
            strncpy(path, "/dev/input/event0", sizeof(path));
            fprintf(stderr, "[input] Auto-detect failed, falling back to %s\n", path);
        }
    }

    ctx->fd = open(path, O_RDONLY | O_NONBLOCK);
    if (ctx->fd < 0) {
        perror("input_init: open");
        return -1;
    }

    /* Grab exclusive access — prevents native UI from reading button events */
    if (ioctl(ctx->fd, EVIOCGRAB, 1) < 0) {
        fprintf(stderr, "[input] Warning: EVIOCGRAB failed (non-fatal)\n");
    } else {
        fprintf(stderr, "[input] Grabbed exclusive input\n");
    }

    char name[256] = "Unknown";
    ioctl(ctx->fd, EVIOCGNAME(sizeof(name)), name);
    fprintf(stderr, "[input] Opened: %s (%s)\n", path, name);

    return 0;
}

void input_poll(InputContext *ctx) {
    ctx->_prev = ctx->held;

    /* Read all pending events without blocking */
    struct input_event ev;
    while (1) {
        ssize_t n = read(ctx->fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) break;

        if (ev.type == EV_KEY) {
            uint8_t btn = _keycode_to_btn(ev.code);
            if (btn) {
                if (ev.value == 1)       /* press */
                    ctx->held |= btn;
                else if (ev.value == 0)  /* release */
                    ctx->held &= ~btn;
                /* ev.value == 2 is auto-repeat, held stays set */
            }
        }
    }

    /* Compute edge-triggered states */
    ctx->pressed  = ctx->held & ~ctx->_prev;   /* newly pressed */
    ctx->released = ~ctx->held & ctx->_prev;    /* newly released */
}

int input_held(const InputContext *ctx, uint8_t button) {
    return (ctx->held & button) != 0;
}

int input_pressed(const InputContext *ctx, uint8_t button) {
    return (ctx->pressed & button) != 0;
}

int input_released(const InputContext *ctx, uint8_t button) {
    return (ctx->released & button) != 0;
}

uint8_t input_wait_any(InputContext *ctx) {
    struct pollfd pfd = { .fd = ctx->fd, .events = POLLIN };

    while (1) {
        poll(&pfd, 1, -1);  /* Block until input available */
        input_poll(ctx);
        if (ctx->pressed & BTN_ANY)
            return ctx->pressed;
    }
}

void input_cleanup(InputContext *ctx) {
    if (ctx->fd >= 0) {
        ioctl(ctx->fd, EVIOCGRAB, 0);  /* Release exclusive grab */
        close(ctx->fd);
        ctx->fd = -1;
    }
}

#endif /* PAGER_INPUT_IMPLEMENTATION */
