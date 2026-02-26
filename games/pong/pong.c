/*
 * pong.c — Pong Game for WiFi Pineapple Pager
 * Target: WiFi Pineapple Pager (480x222, D-pad + Select)
 *
 * Player (left) vs AI (right).
 *
 * Controls:
 *   D-pad Up/Down — move paddle
 *   Select        — start / pause
 *
 * Compile:
 *   gcc -O2 -o pong pong.c -I../engine -lm
 */

#define PAGER_ENGINE_IMPLEMENTATION
#include "pager_engine.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ── Constants ──────────────────────────────────────────────────────── */

#define PADDLE_W     6
#define PADDLE_H     40
#define PADDLE_SPEED 180.0f   /* pixels per second */
#define BALL_SIZE    6
#define BALL_SPEED   160.0f
#define BALL_ACCEL   8.0f     /* speed increase per hit */
#define MAX_BALL_SPEED 400.0f
#define AI_SPEED     140.0f
#define WINNING_SCORE 11

#define COURT_TOP    24       /* HUD height */
#define COURT_BOTTOM SCREEN_H

/* ── Game State ─────────────────────────────────────────────────────── */

typedef struct {
    /* Paddles */
    float p1_y;   /* player (left) */
    float p2_y;   /* AI (right) */

    /* Ball */
    float ball_x, ball_y;
    float ball_vx, ball_vy;
    float ball_speed;

    /* Score */
    int   score1, score2;

    /* State */
    enum { PONG_TITLE, PONG_SERVING, PONG_PLAYING, PONG_PAUSED, PONG_WIN } state;
    int   serving_player;  /* 1 or 2 */
    float serve_timer;
} PongGame;

/* ── Helpers ────────────────────────────────────────────────────────── */

static void serve_ball(PongGame *g) {
    g->ball_x = SCREEN_W / 2.0f;
    g->ball_y = (COURT_TOP + COURT_BOTTOM) / 2.0f;
    g->ball_speed = BALL_SPEED;

    float angle = ((rand() % 60) - 30) * 3.14159f / 180.0f;
    float dir = (g->serving_player == 1) ? 1.0f : -1.0f;
    g->ball_vx = dir * cosf(angle) * g->ball_speed;
    g->ball_vy = sinf(angle) * g->ball_speed;
}

static void reset_pong(PongGame *g) {
    g->p1_y = (COURT_TOP + COURT_BOTTOM) / 2.0f - PADDLE_H / 2.0f;
    g->p2_y = g->p1_y;
    g->score1 = 0;
    g->score2 = 0;
    g->serving_player = 1;
    g->serve_timer = 0;
    serve_ball(g);
}

/* ── Callbacks ──────────────────────────────────────────────────────── */

static void pong_init(Engine *engine, void *userdata) {
    (void)engine;
    PongGame *g = (PongGame *)userdata;
    srand((unsigned)time(NULL));
    memset(g, 0, sizeof(PongGame));
    g->state = PONG_TITLE;
    reset_pong(g);
}

static void pong_update(Engine *engine, float dt, void *userdata) {
    PongGame *g = (PongGame *)userdata;
    InputContext *inp = &engine->input;

    switch (g->state) {
    case PONG_TITLE:
        if (input_pressed(inp, BTN_SELECT)) {
            reset_pong(g);
            g->state = PONG_SERVING;
            g->serve_timer = 1.5f;
        }
        break;

    case PONG_PAUSED:
        if (input_pressed(inp, BTN_SELECT))
            g->state = PONG_PLAYING;
        break;

    case PONG_WIN:
        if (input_pressed(inp, BTN_SELECT)) {
            reset_pong(g);
            g->state = PONG_SERVING;
            g->serve_timer = 1.5f;
        }
        break;

    case PONG_SERVING:
        g->serve_timer -= dt;
        if (g->serve_timer <= 0) {
            serve_ball(g);
            g->state = PONG_PLAYING;
        }
        /* Allow paddle movement during serve countdown */
        if (input_held(inp, BTN_UP))   g->p1_y -= PADDLE_SPEED * dt;
        if (input_held(inp, BTN_DOWN)) g->p1_y += PADDLE_SPEED * dt;
        break;

    case PONG_PLAYING:
        if (input_pressed(inp, BTN_SELECT)) {
            g->state = PONG_PAUSED;
            break;
        }

        /* Player paddle */
        if (input_held(inp, BTN_UP))   g->p1_y -= PADDLE_SPEED * dt;
        if (input_held(inp, BTN_DOWN)) g->p1_y += PADDLE_SPEED * dt;

        /* Clamp paddle positions */
        if (g->p1_y < COURT_TOP) g->p1_y = COURT_TOP;
        if (g->p1_y > COURT_BOTTOM - PADDLE_H) g->p1_y = COURT_BOTTOM - PADDLE_H;

        /* AI paddle — track ball with slight delay */
        float ai_target = g->ball_y - PADDLE_H / 2.0f;
        float ai_diff = ai_target - g->p2_y;
        float ai_move = AI_SPEED * dt;
        if (ai_diff > ai_move)       g->p2_y += ai_move;
        else if (ai_diff < -ai_move) g->p2_y -= ai_move;
        else                         g->p2_y = ai_target;

        if (g->p2_y < COURT_TOP) g->p2_y = COURT_TOP;
        if (g->p2_y > COURT_BOTTOM - PADDLE_H) g->p2_y = COURT_BOTTOM - PADDLE_H;

        /* Ball movement */
        g->ball_x += g->ball_vx * dt;
        g->ball_y += g->ball_vy * dt;

        /* Top/bottom bounce */
        if (g->ball_y <= COURT_TOP) {
            g->ball_y = COURT_TOP;
            g->ball_vy = -g->ball_vy;
        }
        if (g->ball_y + BALL_SIZE >= COURT_BOTTOM) {
            g->ball_y = COURT_BOTTOM - BALL_SIZE;
            g->ball_vy = -g->ball_vy;
        }

        /* Left paddle collision */
        float p1_left = 12.0f;
        if (g->ball_x <= p1_left + PADDLE_W &&
            g->ball_y + BALL_SIZE >= g->p1_y &&
            g->ball_y <= g->p1_y + PADDLE_H &&
            g->ball_vx < 0) {
            g->ball_vx = -g->ball_vx;
            g->ball_x = p1_left + PADDLE_W;
            /* Adjust angle based on hit position */
            float hit = (g->ball_y + BALL_SIZE/2 - g->p1_y) / PADDLE_H;
            g->ball_vy = (hit - 0.5f) * 2.0f * g->ball_speed;
            /* Speed up */
            g->ball_speed += BALL_ACCEL;
            if (g->ball_speed > MAX_BALL_SPEED) g->ball_speed = MAX_BALL_SPEED;
        }

        /* Right paddle collision */
        float p2_left = SCREEN_W - 12.0f - PADDLE_W;
        if (g->ball_x + BALL_SIZE >= p2_left &&
            g->ball_y + BALL_SIZE >= g->p2_y &&
            g->ball_y <= g->p2_y + PADDLE_H &&
            g->ball_vx > 0) {
            g->ball_vx = -g->ball_vx;
            g->ball_x = p2_left - BALL_SIZE;
            float hit = (g->ball_y + BALL_SIZE/2 - g->p2_y) / PADDLE_H;
            g->ball_vy = (hit - 0.5f) * 2.0f * g->ball_speed;
            g->ball_speed += BALL_ACCEL;
            if (g->ball_speed > MAX_BALL_SPEED) g->ball_speed = MAX_BALL_SPEED;
        }

        /* Scoring */
        if (g->ball_x < 0) {
            g->score2++;
            g->serving_player = 1;
            if (g->score2 >= WINNING_SCORE) {
                g->state = PONG_WIN;
            } else {
                g->state = PONG_SERVING;
                g->serve_timer = 1.0f;
            }
        }
        if (g->ball_x > SCREEN_W) {
            g->score1++;
            g->serving_player = 2;
            if (g->score1 >= WINNING_SCORE) {
                g->state = PONG_WIN;
            } else {
                g->state = PONG_SERVING;
                g->serve_timer = 1.0f;
            }
        }
        break;
    }
}

static void pong_render(Engine *engine, void *userdata) {
    PongGame *g = (PongGame *)userdata;
    GfxContext *gfx = &engine->gfx;

    gfx_clear(gfx, COLOR_BLACK);

    if (g->state == PONG_TITLE) {
        gfx_text_centered(gfx, 15, "=== PONG ===", COLOR_HAK5_GREEN);
        gfx_text_centered(gfx, 45, "WiFi Pineapple Pager Edition", COLOR_TERMINAL);
        gfx_text_centered(gfx, 85, "Up/Down to move paddle", COLOR_WHITE);
        gfx_text_centered(gfx, 110, "Select to start/pause", COLOR_WHITE);
        gfx_text_centered(gfx, 150, "First to 11 wins!", COLOR_YELLOW);
        gfx_text_centered(gfx, 185, "[ PRESS SELECT ]", COLOR_HAK5_GREEN);
        return;
    }

    if (g->state == PONG_WIN) {
        const char *winner = (g->score1 >= WINNING_SCORE) ? "YOU WIN!" : "AI WINS!";
        uint16_t color = (g->score1 >= WINNING_SCORE) ? COLOR_HAK5_GREEN : COLOR_RED;
        gfx_text_centered(gfx, 40, winner, color);
        gfx_printf(gfx, (SCREEN_W - 12 * CHAR_W) / 2, 80, COLOR_WHITE, "%d  -  %d", g->score1, g->score2);
        gfx_text_centered(gfx, 150, "[ SELECT to play again ]", COLOR_GRAY);
        return;
    }

    /* HUD */
    gfx_rect_fill(gfx, 0, 0, SCREEN_W, COURT_TOP - 1, COLOR_HAK5_DARK);
    gfx_printf(gfx, 80, 4, COLOR_WHITE, "P1: %d", g->score1);
    gfx_printf(gfx, SCREEN_W - 8 * CHAR_W, 4, COLOR_WHITE, "AI: %d", g->score2);
    gfx_hline(gfx, 0, COURT_TOP - 1, SCREEN_W, COLOR_DARK_GRAY);

    /* Center dashed line */
    for (int y = COURT_TOP; y < COURT_BOTTOM; y += 8) {
        gfx_vline(gfx, SCREEN_W / 2, y, 4, COLOR_DARK_GRAY);
    }

    /* Paddles */
    gfx_rect_fill(gfx, 12, (int)g->p1_y, PADDLE_W, PADDLE_H, COLOR_HAK5_GREEN);
    gfx_rect_fill(gfx, SCREEN_W - 12 - PADDLE_W, (int)g->p2_y, PADDLE_W, PADDLE_H, COLOR_CYAN);

    /* Ball */
    gfx_rect_fill(gfx, (int)g->ball_x, (int)g->ball_y, BALL_SIZE, BALL_SIZE, COLOR_WHITE);

    /* Pause overlay */
    if (g->state == PONG_PAUSED) {
        gfx_rect_fill(gfx, 160, 80, 160, 50, COLOR_HAK5_DARK);
        gfx_rect(gfx, 160, 80, 160, 50, COLOR_HAK5_GREEN);
        gfx_text_centered(gfx, 90, "PAUSED", COLOR_YELLOW);
        gfx_text_centered(gfx, 112, "Select to resume", COLOR_WHITE);
    }

    /* Serve countdown */
    if (g->state == PONG_SERVING && g->serve_timer > 0) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", (int)ceilf(g->serve_timer));
        gfx_text_centered(gfx, (COURT_TOP + COURT_BOTTOM)/2 - CHAR_H/2, buf, COLOR_YELLOW);
    }
}

static void pong_cleanup(Engine *engine, void *userdata) {
    (void)engine;
    (void)userdata;
}

/* ── Main ───────────────────────────────────────────────────────────── */

int main(void) {
    PongGame game;
    Engine engine;

    if (engine_create(&engine, pong_init, pong_update, pong_render, pong_cleanup, &game) < 0) {
        fprintf(stderr, "Failed to create engine\n");
        return 1;
    }

    engine.target_fps = 60;  /* Pong benefits from higher FPS */
    engine_run(&engine);
    engine_destroy(&engine);

    return 0;
}
