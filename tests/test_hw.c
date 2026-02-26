/*
 * test_hw.c — Input + display test for WiFi Pineapple Pager
 * Shows which engine buttons are detected in real-time.
 */
#define PAGER_ENGINE_IMPLEMENTATION
#include "../games/engine/pager_engine.h"
#include <string.h>

typedef struct {
    char last_event[128];
    int  btn_count;
} TestState;

void test_init(Engine *e, void *data) {
    (void)e;
    TestState *ts = (TestState *)data;
    strcpy(ts->last_event, "(none yet)");
    ts->btn_count = 0;
}

void test_update(Engine *e, float dt, void *data) {
    (void)dt;
    TestState *ts = (TestState *)data;
    InputContext *inp = &e->input;

    if (input_pressed(inp, BTN_UP))    { strcpy(ts->last_event, "BTN_UP    PRESSED");  ts->btn_count++; }
    if (input_pressed(inp, BTN_DOWN))  { strcpy(ts->last_event, "BTN_DOWN  PRESSED");  ts->btn_count++; }
    if (input_pressed(inp, BTN_LEFT))  { strcpy(ts->last_event, "BTN_LEFT  PRESSED");  ts->btn_count++; }
    if (input_pressed(inp, BTN_RIGHT)) { strcpy(ts->last_event, "BTN_RIGHT PRESSED");  ts->btn_count++; }
    if (input_pressed(inp, BTN_A))     { strcpy(ts->last_event, "BTN_A     PRESSED");  ts->btn_count++; }
    if (input_pressed(inp, BTN_B))     { strcpy(ts->last_event, "BTN_B     PRESSED");  ts->btn_count++; }
    if (input_pressed(inp, BTN_POWER)) { strcpy(ts->last_event, "BTN_POWER PRESSED");  ts->btn_count++; }

    if (input_held(inp, BTN_B) && input_held(inp, BTN_A))
        e->running = 0;  /* Hold A+B to quit */
}

void test_render(Engine *e, void *data) {
    TestState *ts = (TestState *)data;
    GfxContext *g = &e->gfx;
    InputContext *inp = &e->input;

    gfx_clear(g, COLOR_HAK5_DARK);

    gfx_text_centered(g, 10, "BUTTON TEST", COLOR_HAK5_GREEN);
    gfx_text(g, 10, 35, "Hold A+B to quit", COLOR_GRAY);

    /* Show current held state */
    gfx_text(g, 10, 60, "HELD:", COLOR_WHITE);
    int y = 60;
    if (input_held(inp, BTN_UP))    gfx_text(g, 100, y, "UP",    COLOR_GREEN);
    if (input_held(inp, BTN_DOWN))  gfx_text(g, 140, y, "DOWN",  COLOR_GREEN);
    if (input_held(inp, BTN_LEFT))  gfx_text(g, 200, y, "LEFT",  COLOR_GREEN);
    if (input_held(inp, BTN_RIGHT)) gfx_text(g, 260, y, "RIGHT", COLOR_GREEN);
    if (input_held(inp, BTN_A))     gfx_text(g, 330, y, "A",     COLOR_GREEN);
    if (input_held(inp, BTN_B))     gfx_text(g, 360, y, "B",     COLOR_RED);

    /* Raw held bitmask */
    gfx_printf(g, 10, 90, COLOR_CYAN, "held=0x%02X  prev=0x%02X  pressed=0x%02X",
               inp->held, inp->_prev, inp->pressed);

    /* Last event */
    gfx_printf(g, 10, 120, COLOR_YELLOW, "Last: %s", ts->last_event);
    gfx_printf(g, 10, 145, COLOR_GRAY, "Total presses: %d", ts->btn_count);

    /* Visual D-pad */
    int cx = 240, cy = 185;
    uint16_t up_c    = input_held(inp, BTN_UP)    ? COLOR_GREEN : COLOR_DARK_GRAY;
    uint16_t down_c  = input_held(inp, BTN_DOWN)  ? COLOR_GREEN : COLOR_DARK_GRAY;
    uint16_t left_c  = input_held(inp, BTN_LEFT)  ? COLOR_GREEN : COLOR_DARK_GRAY;
    uint16_t right_c = input_held(inp, BTN_RIGHT) ? COLOR_GREEN : COLOR_DARK_GRAY;
    uint16_t a_c     = input_held(inp, BTN_A)     ? COLOR_GREEN : COLOR_DARK_GRAY;
    uint16_t b_c     = input_held(inp, BTN_B)     ? COLOR_RED   : COLOR_DARK_GRAY;

    gfx_rect_fill(g, cx - 8,  cy - 30, 16, 16, up_c);    /* Up */
    gfx_rect_fill(g, cx - 8,  cy + 14, 16, 16, down_c);  /* Down */
    gfx_rect_fill(g, cx - 30, cy - 8,  16, 16, left_c);  /* Left */
    gfx_rect_fill(g, cx + 14, cy - 8,  16, 16, right_c); /* Right */
    gfx_rect_fill(g, cx + 60, cy - 8,  16, 16, a_c);     /* A */
    gfx_rect_fill(g, cx + 90, cy - 8,  16, 16, b_c);     /* B */

    gfx_text(g, cx - 4, cy - 27, "U", COLOR_WHITE);
    gfx_text(g, cx - 4, cy + 17, "D", COLOR_WHITE);
    gfx_text(g, cx - 27, cy - 4, "L", COLOR_WHITE);
    gfx_text(g, cx + 17, cy - 4, "R", COLOR_WHITE);
    gfx_text(g, cx + 63, cy - 4, "A", COLOR_WHITE);
    gfx_text(g, cx + 93, cy - 4, "B", COLOR_WHITE);
}

int main(void) {
    TestState ts;
    Engine engine;
    engine_create(&engine, test_init, test_update, test_render, NULL, &ts);
    engine_run(&engine);
    engine_destroy(&engine);
    return 0;
}