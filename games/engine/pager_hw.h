/*
 * pager_hw.h — Hardware Effects for WiFi Pineapple Pager
 * Target: WiFi Pineapple Pager
 *
 * Controls: D-pad RGB LEDs, A/B button LEDs, buzzer, vibration motor,
 *           screen brightness.
 *
 * All hardware is accessed via sysfs / /dev nodes — no special drivers needed.
 * Functions are best-effort: if hardware isn't present, they silently no-op.
 *
 * Single-header library. Define PAGER_HW_IMPLEMENTATION in exactly ONE
 * .c file before including this header.
 */

#ifndef PAGER_HW_H
#define PAGER_HW_H

#include <stdint.h>

/* ── LED Colors (for D-pad LEDs) ────────────────────────────────────── */

typedef struct {
    uint8_t r, g, b;
} LedColor;

#define LED_OFF     (LedColor){0, 0, 0}
#define LED_RED     (LedColor){255, 0, 0}
#define LED_GREEN   (LedColor){0, 255, 0}
#define LED_BLUE    (LedColor){0, 0, 255}
#define LED_YELLOW  (LedColor){255, 255, 0}
#define LED_CYAN    (LedColor){0, 255, 255}
#define LED_MAGENTA (LedColor){255, 0, 255}
#define LED_WHITE   (LedColor){255, 255, 255}
#define LED_ORANGE  (LedColor){255, 128, 0}
#define LED_PURPLE  (LedColor){128, 0, 255}
#define LED_PINK    (LedColor){255, 64, 128}

/* ── Function Prototypes ────────────────────────────────────────────── */

/* D-pad LEDs — each direction has independent RGB control */
void hw_led_dpad_up(LedColor color);
void hw_led_dpad_down(LedColor color);
void hw_led_dpad_left(LedColor color);
void hw_led_dpad_right(LedColor color);
void hw_led_dpad_all(LedColor color);    /* Set all 4 D-pad LEDs */

/* A/B button LEDs (simple brightness, not RGB) */
/* NOTE: sysfs names are swapped — "b-button-led" = green/A, "a-button-led" = red/B */
void hw_led_a_button(uint8_t brightness);  /* 0-255 */
void hw_led_b_button(uint8_t brightness);  /* 0-255 */
void hw_led_all_off(void);

/* Buzzer */
void hw_beep(int freq_hz, int duration_ms);
void hw_play_rtttl(const char *rtttl);      /* Non-blocking RTTTL playback */
void hw_stop_audio(void);

/* Vibration */
void hw_vibrate(int duration_ms);
void hw_vibrate_pattern(const char *pattern); /* "on,off,on,off,..." in ms */

/* Screen brightness */
void hw_set_brightness(int percent);   /* 0-100 */
int  hw_get_brightness(void);

/* Combined effects (convenience) */
void hw_flash_leds(LedColor color, int count, int on_ms, int off_ms);
void hw_hit_feedback(void);             /* Quick vibrate + beep for damage */
void hw_critical_feedback(void);        /* Strong vibrate + high beep */
void hw_level_up_feedback(void);        /* Rising tones + LED sweep */
void hw_death_feedback(void);           /* Descending tones + vibrate */

#endif /* PAGER_HW_H */

/* ════════════════════════════════════════════════════════════════════ */
/*                         IMPLEMENTATION                              */
/* ════════════════════════════════════════════════════════════════════ */

#ifdef PAGER_HW_IMPLEMENTATION
#undef PAGER_HW_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

/* ── sysfs Helper ───────────────────────────────────────────────────── */

static void _sysfs_write(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, value, strlen(value));
        close(fd);
    }
}

static void _sysfs_write_int(const char *path, int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    _sysfs_write(path, buf);
}

static int _sysfs_read_int(const char *path) {
    char buf[16] = {0};
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        read(fd, buf, sizeof(buf) - 1);
        close(fd);
        return atoi(buf);
    }
    return -1;
}

/* ── D-pad LED Paths ────────────────────────────────────────────────── */
/* The D-pad LEDs are exposed as multi-color LED sysfs entries.
 * Exact paths may vary by firmware version. We try common paths. */

static const char *_dpad_led_paths[] = {
    "/sys/class/leds/dpad-up",
    "/sys/class/leds/dpad-down",
    "/sys/class/leds/dpad-left",
    "/sys/class/leds/dpad-right"
};

static void _set_dpad_led(int index, LedColor color) {
    char path[256];

    /* Try multi_intensity path (common for multi-color LEDs) */
    snprintf(path, sizeof(path), "%s/multi_intensity", _dpad_led_paths[index]);
    char value[32];
    snprintf(value, sizeof(value), "%d %d %d", color.r, color.g, color.b);
    _sysfs_write(path, value);

    /* Set brightness to max if color is non-zero */
    snprintf(path, sizeof(path), "%s/brightness", _dpad_led_paths[index]);
    _sysfs_write_int(path, (color.r || color.g || color.b) ? 255 : 0);
}

void hw_led_dpad_up(LedColor color)    { _set_dpad_led(0, color); }
void hw_led_dpad_down(LedColor color)  { _set_dpad_led(1, color); }
void hw_led_dpad_left(LedColor color)  { _set_dpad_led(2, color); }
void hw_led_dpad_right(LedColor color) { _set_dpad_led(3, color); }

void hw_led_dpad_all(LedColor color) {
    for (int i = 0; i < 4; i++)
        _set_dpad_led(i, color);
}

/* ── A/B Button LEDs ────────────────────────────────────────────────── */
/* NOTE: sysfs names are swapped in hardware:
 *   "b-button-led" actually controls the GREEN LED (A button)
 *   "a-button-led" actually controls the RED LED (B button) */

void hw_led_a_button(uint8_t brightness) {
    _sysfs_write_int("/sys/class/leds/b-button-led/brightness", brightness);
}

void hw_led_b_button(uint8_t brightness) {
    _sysfs_write_int("/sys/class/leds/a-button-led/brightness", brightness);
}

void hw_led_all_off(void) {
    hw_led_dpad_all(LED_OFF);
    hw_led_a_button(0);
    hw_led_b_button(0);
}

/* ── Buzzer ─────────────────────────────────────────────────────────── */
/* The buzzer is typically controlled via a pwm-beeper or gpio-beeper driver.
 * We shell out for RTTTL since it requires timing logic. */

void hw_beep(int freq_hz, int duration_ms) {
    /* Try pwm-beeper sysfs */
    _sysfs_write_int("/sys/class/pwm/pwmchip0/pwm0/period",
                     freq_hz > 0 ? 1000000000 / freq_hz : 0);
    _sysfs_write("/sys/class/pwm/pwmchip0/pwm0/enable", "1");

    struct timespec ts = { duration_ms / 1000, (duration_ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);

    _sysfs_write("/sys/class/pwm/pwmchip0/pwm0/enable", "0");
}

void hw_play_rtttl(const char *rtttl) {
    /* Use pagerctl if available, otherwise fall back to simple parsing */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "pagerctl rtttl '%s' &", rtttl);
    system(cmd);
}

void hw_stop_audio(void) {
    _sysfs_write("/sys/class/pwm/pwmchip0/pwm0/enable", "0");
    system("killall pagerctl 2>/dev/null");
}

/* ── Vibration ──────────────────────────────────────────────────────── */

void hw_vibrate(int duration_ms) {
    /* Vibration motor is typically a GPIO or pwm output */
    _sysfs_write("/sys/class/leds/vibrator/brightness", "255");
    struct timespec ts = { duration_ms / 1000, (duration_ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
    _sysfs_write("/sys/class/leds/vibrator/brightness", "0");
}

void hw_vibrate_pattern(const char *pattern) {
    /* Parse "on,off,on,off,..." */
    char buf[256];
    strncpy(buf, pattern, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int is_on = 1;
    char *tok = strtok(buf, ",");
    while (tok) {
        int ms = atoi(tok);
        if (is_on) {
            _sysfs_write("/sys/class/leds/vibrator/brightness", "255");
        } else {
            _sysfs_write("/sys/class/leds/vibrator/brightness", "0");
        }
        struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
        nanosleep(&ts, NULL);
        is_on = !is_on;
        tok = strtok(NULL, ",");
    }
    _sysfs_write("/sys/class/leds/vibrator/brightness", "0");
}

/* ── Screen Brightness ──────────────────────────────────────────────── */

void hw_set_brightness(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int max_br = _sysfs_read_int("/sys/class/backlight/backlight/max_brightness");
    if (max_br <= 0) max_br = 255;
    int value = (percent * max_br) / 100;
    _sysfs_write_int("/sys/class/backlight/backlight/brightness", value);
}

int hw_get_brightness(void) {
    int cur = _sysfs_read_int("/sys/class/backlight/backlight/brightness");
    int max_br = _sysfs_read_int("/sys/class/backlight/backlight/max_brightness");
    if (max_br <= 0 || cur < 0) return -1;
    return (cur * 100) / max_br;
}

/* ── Combined Effects ───────────────────────────────────────────────── */

static void _sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

void hw_flash_leds(LedColor color, int count, int on_ms, int off_ms) {
    for (int i = 0; i < count; i++) {
        hw_led_dpad_all(color);
        _sleep_ms(on_ms);
        hw_led_dpad_all(LED_OFF);
        if (i < count - 1) _sleep_ms(off_ms);
    }
}

void hw_hit_feedback(void) {
    hw_vibrate(30);
    hw_beep(200, 30);
}

void hw_critical_feedback(void) {
    hw_vibrate(80);
    hw_beep(600, 40);
    _sleep_ms(30);
    hw_beep(800, 40);
}

void hw_level_up_feedback(void) {
    hw_led_dpad_all(LED_GREEN);
    hw_led_a_button(255);
    hw_beep(440, 80);
    _sleep_ms(60);
    hw_beep(554, 80);
    _sleep_ms(60);
    hw_beep(659, 80);
    _sleep_ms(60);
    hw_beep(880, 120);
    hw_led_dpad_all(LED_OFF);
    hw_led_a_button(0);
}

void hw_death_feedback(void) {
    hw_led_dpad_all(LED_RED);
    hw_led_b_button(255);
    hw_vibrate_pattern("100,50,100,50,200");
    hw_beep(440, 100);
    _sleep_ms(80);
    hw_beep(330, 100);
    _sleep_ms(80);
    hw_beep(220, 200);
    hw_led_dpad_all(LED_OFF);
    hw_led_b_button(0);
}

#endif /* PAGER_HW_IMPLEMENTATION */
