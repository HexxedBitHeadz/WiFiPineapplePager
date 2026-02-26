/*
 * snake.c — Classic Snake Game for WiFi Pineapple Pager
 * Target: WiFi Pineapple Pager (480x222, D-pad + Select)
 *
 * Controls:
 *   D-pad   — change direction
 *   Select  — pause / start
 *
 * Compile:
 *   gcc -O2 -o snake snake.c -I../engine -lm
 *
 * Cross-compile:
 *   mipsel-linux-musl-gcc -O2 -o snake snake.c -I../engine -lm
 */

#define PAGER_ENGINE_IMPLEMENTATION
#include "pager_engine.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Game Configuration ─────────────────────────────────────────────── */

#define TILE_SIZE     10
#define GRID_W        (SCREEN_W / TILE_SIZE)     /* 48 tiles */
#define GRID_H        ((SCREEN_H - 24) / TILE_SIZE)  /* top 24px for HUD */
#define GRID_Y_OFFSET 24                         /* pixels reserved for score bar */
#define MAX_SNAKE     (GRID_W * GRID_H)
#define INITIAL_LEN   4
#define INITIAL_SPEED 0.12f  /* seconds per move (lower = faster) */
#define SPEED_UP      0.002f /* speed increase per food eaten */
#define MIN_SPEED     0.04f  /* fastest allowed */

/* ── Direction ──────────────────────────────────────────────────────── */

typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Direction;

/* ── Game State ─────────────────────────────────────────────────────── */

typedef struct {
    /* Snake body — ring buffer */
    int      snake_x[MAX_SNAKE];
    int      snake_y[MAX_SNAKE];
    int      head;
    int      length;
    Direction dir;
    Direction next_dir;  /* buffered input */

    /* Food */
    int      food_x;
    int      food_y;

    /* Scoring */
    int      score;
    int      high_score;

    /* Timing */
    float    move_timer;
    float    speed;

    /* State machine */
    enum { STATE_TITLE, STATE_PLAYING, STATE_PAUSED, STATE_GAMEOVER } state;
} SnakeGame;

/* ── Helpers ────────────────────────────────────────────────────────── */

static void place_food(SnakeGame *g) {
    int attempts = 0;
    while (attempts++ < 1000) {
        g->food_x = rand() % GRID_W;
        g->food_y = rand() % GRID_H;

        /* Make sure food doesn't land on snake */
        int collision = 0;
        for (int i = 0; i < g->length; i++) {
            int idx = (g->head - i + MAX_SNAKE) % MAX_SNAKE;
            if (g->snake_x[idx] == g->food_x && g->snake_y[idx] == g->food_y) {
                collision = 1;
                break;
            }
        }
        if (!collision) break;
    }
}

static void reset_game(SnakeGame *g) {
    g->length = INITIAL_LEN;
    g->head = INITIAL_LEN - 1;
    g->dir = DIR_RIGHT;
    g->next_dir = DIR_RIGHT;
    g->score = 0;
    g->move_timer = 0.0f;
    g->speed = INITIAL_SPEED;

    /* Place snake in center */
    int start_x = GRID_W / 2 - INITIAL_LEN / 2;
    int start_y = GRID_H / 2;
    for (int i = 0; i < INITIAL_LEN; i++) {
        g->snake_x[i] = start_x + i;
        g->snake_y[i] = start_y;
    }

    place_food(g);
}

/* ── Callbacks ──────────────────────────────────────────────────────── */

static void game_init(Engine *engine, void *userdata) {
    (void)engine;
    SnakeGame *g = (SnakeGame *)userdata;
    srand((unsigned)time(NULL));
    memset(g, 0, sizeof(SnakeGame));
    g->state = STATE_TITLE;
    g->high_score = 0;
    reset_game(g);
}

static void game_update(Engine *engine, float dt, void *userdata) {
    SnakeGame *g = (SnakeGame *)userdata;
    InputContext *inp = &engine->input;

    switch (g->state) {
    case STATE_TITLE:
        if (input_pressed(inp, BTN_SELECT))
            g->state = STATE_PLAYING;
        break;

    case STATE_PAUSED:
        if (input_pressed(inp, BTN_SELECT))
            g->state = STATE_PLAYING;
        break;

    case STATE_GAMEOVER:
        if (input_pressed(inp, BTN_SELECT)) {
            reset_game(g);
            g->state = STATE_PLAYING;
        }
        break;

    case STATE_PLAYING:
        /* Pause */
        if (input_pressed(inp, BTN_SELECT)) {
            g->state = STATE_PAUSED;
            break;
        }

        /* Buffer direction input (prevent 180° turns) */
        if (input_pressed(inp, BTN_UP)    && g->dir != DIR_DOWN)  g->next_dir = DIR_UP;
        if (input_pressed(inp, BTN_DOWN)  && g->dir != DIR_UP)    g->next_dir = DIR_DOWN;
        if (input_pressed(inp, BTN_LEFT)  && g->dir != DIR_RIGHT) g->next_dir = DIR_LEFT;
        if (input_pressed(inp, BTN_RIGHT) && g->dir != DIR_LEFT)  g->next_dir = DIR_RIGHT;

        /* Move on timer */
        g->move_timer += dt;
        if (g->move_timer < g->speed)
            break;
        g->move_timer -= g->speed;

        /* Apply buffered direction */
        g->dir = g->next_dir;

        /* Calculate new head position */
        int hx = g->snake_x[g->head];
        int hy = g->snake_y[g->head];
        switch (g->dir) {
            case DIR_UP:    hy--; break;
            case DIR_DOWN:  hy++; break;
            case DIR_LEFT:  hx--; break;
            case DIR_RIGHT: hx++; break;
        }

        /* Wall collision */
        if (hx < 0 || hx >= GRID_W || hy < 0 || hy >= GRID_H) {
            g->state = STATE_GAMEOVER;
            if (g->score > g->high_score) g->high_score = g->score;
            break;
        }

        /* Self collision */
        for (int i = 0; i < g->length; i++) {
            int idx = (g->head - i + MAX_SNAKE) % MAX_SNAKE;
            if (g->snake_x[idx] == hx && g->snake_y[idx] == hy) {
                g->state = STATE_GAMEOVER;
                if (g->score > g->high_score) g->high_score = g->score;
                return;
            }
        }

        /* Advance head */
        g->head = (g->head + 1) % MAX_SNAKE;
        g->snake_x[g->head] = hx;
        g->snake_y[g->head] = hy;

        /* Check food */
        if (hx == g->food_x && hy == g->food_y) {
            g->length++;
            g->score += 10;
            g->speed -= SPEED_UP;
            if (g->speed < MIN_SPEED) g->speed = MIN_SPEED;
            place_food(g);
        }
        break;
    }
}

static void game_render(Engine *engine, void *userdata) {
    SnakeGame *g = (SnakeGame *)userdata;
    GfxContext *gfx = &engine->gfx;

    gfx_clear(gfx, COLOR_HAK5_DARK);

    switch (g->state) {
    case STATE_TITLE:
        gfx_text_centered(gfx, 20, "=== SNAKE ===", COLOR_HAK5_GREEN);
        gfx_text_centered(gfx, 50, "WiFi Pineapple Pager Edition", COLOR_TERMINAL);
        gfx_text_centered(gfx, 90, "D-Pad to move", COLOR_WHITE);
        gfx_text_centered(gfx, 115, "Select to start/pause", COLOR_WHITE);
        gfx_text_centered(gfx, 170, "[ PRESS SELECT ]", COLOR_YELLOW);
        break;

    case STATE_PAUSED:
        /* Draw game underneath */
        goto draw_game;
    case STATE_PLAYING:
    draw_game: {
        /* HUD bar */
        gfx_rect_fill(gfx, 0, 0, SCREEN_W, GRID_Y_OFFSET - 2, COLOR_HAK5_BLUE);
        gfx_printf(gfx, 4, 4, COLOR_WHITE, "SCORE: %d", g->score);
        gfx_printf(gfx, SCREEN_W - 8 * CHAR_W, 4, COLOR_GRAY, "HI: %d", g->high_score);
        gfx_hline(gfx, 0, GRID_Y_OFFSET - 1, SCREEN_W, COLOR_HAK5_GREEN);

        /* Grid border */
        int gx = 0, gy = GRID_Y_OFFSET;
        int gw = GRID_W * TILE_SIZE, gh = GRID_H * TILE_SIZE;
        gfx_rect(gfx, gx, gy, gw, gh, COLOR_DARK_GRAY);

        /* Food */
        int fx = g->food_x * TILE_SIZE + 1;
        int fy = g->food_y * TILE_SIZE + GRID_Y_OFFSET + 1;
        gfx_rect_fill(gfx, fx, fy, TILE_SIZE - 2, TILE_SIZE - 2, COLOR_RED);
        gfx_rect_fill(gfx, fx + 2, fy + 2, TILE_SIZE - 6, TILE_SIZE - 6, COLOR_ORANGE);

        /* Snake */
        for (int i = 0; i < g->length; i++) {
            int idx = (g->head - i + MAX_SNAKE) % MAX_SNAKE;
            int sx = g->snake_x[idx] * TILE_SIZE + 1;
            int sy = g->snake_y[idx] * TILE_SIZE + GRID_Y_OFFSET + 1;
            uint16_t color = (i == 0) ? COLOR_WHITE : COLOR_HAK5_GREEN;
            gfx_rect_fill(gfx, sx, sy, TILE_SIZE - 2, TILE_SIZE - 2, color);
        }

        /* Pause overlay */
        if (g->state == STATE_PAUSED) {
            /* Semi-transparent overlay effect */
            for (int y = 80; y < 140; y++)
                gfx_hline(gfx, 140, y, 200, COLOR_HAK5_DARK);
            gfx_rect(gfx, 140, 80, 200, 60, COLOR_HAK5_GREEN);
            gfx_text_centered(gfx, 95, "PAUSED", COLOR_YELLOW);
            gfx_text_centered(gfx, 115, "Select to resume", COLOR_WHITE);
        }
        break;
    }

    case STATE_GAMEOVER:
        gfx_text_centered(gfx, 30, "GAME OVER", COLOR_RED);
        gfx_text_centered(gfx, 65, "Score:", COLOR_WHITE);
        gfx_printf(gfx, (SCREEN_W + 6 * CHAR_W) / 2, 65, COLOR_WHITE, " %d", g->score);
        if (g->score >= g->high_score)
            gfx_text_centered(gfx, 95, "** NEW HIGH SCORE **", COLOR_YELLOW);
        gfx_printf(gfx, (SCREEN_W - 14 * CHAR_W) / 2, 125, COLOR_GRAY, "High Score: %d", g->high_score);
        gfx_text_centered(gfx, 175, "[ SELECT to retry ]", COLOR_HAK5_GREEN);
        break;
    }
}

static void game_cleanup(Engine *engine, void *userdata) {
    (void)engine;
    (void)userdata;
}

/* ── Main ───────────────────────────────────────────────────────────── */

int main(void) {
    SnakeGame game;
    Engine engine;

    if (engine_create(&engine, game_init, game_update, game_render, game_cleanup, &game) < 0) {
        fprintf(stderr, "Failed to create engine\n");
        return 1;
    }

    engine.target_fps = 30;
    engine_run(&engine);
    engine_destroy(&engine);

    return 0;
}
