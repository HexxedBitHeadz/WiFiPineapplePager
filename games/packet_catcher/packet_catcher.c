/*
 * packet_catcher.c — WiFi-Themed Arcade Game for WiFi Pineapple Pager
 * Target: WiFi Pineapple Pager (480x222, D-pad + Select)
 *
 * Concept:
 *   Packets fall from the sky. Catch the good ones (data, auth, beacon)
 *   with your Pineapple antenna dish. Avoid the malware packets!
 *   Speed increases as you level up.
 *
 * Controls:
 *   D-pad Left/Right — move catcher
 *   Select           — start / pause
 *
 * Compile:
 *   gcc -O2 -o packet_catcher packet_catcher.c -I../engine -lm
 */

#define PAGER_ENGINE_IMPLEMENTATION
#include "pager_engine.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Configuration ──────────────────────────────────────────────────── */

#define CATCHER_W       48
#define CATCHER_H       18
#define CATCHER_Y       (SCREEN_H - CATCHER_H - 4)
#define CATCHER_SPEED   250.0f

#define MAX_PACKETS     20
#define PACKET_W        28
#define PACKET_H        18
#define BASE_FALL_SPEED 60.0f
#define SPEED_PER_LEVEL 15.0f
#define SPAWN_INTERVAL  0.8f    /* seconds between spawns */
#define SPAWN_FASTER    0.05f   /* seconds faster per level */
#define MIN_SPAWN       0.25f

#define POINTS_PER_LEVEL 100
#define MAX_LIVES        3

/* ── Packet Types ───────────────────────────────────────────────────── */

typedef enum {
    PKT_DATA,       /* +10 pts, green */
    PKT_BEACON,     /* +15 pts, cyan */
    PKT_AUTH,       /* +20 pts, yellow */
    PKT_DEAUTH,     /* -1 life, red */
    PKT_MALWARE,    /* -1 life, magenta */
    PKT_TYPE_COUNT
} PacketType;

static const char *pkt_labels[] = { "DATA", "BCN", "AUTH", "DEAU", "MALW" };
static const uint16_t pkt_colors[] = {
    COLOR_GREEN, COLOR_CYAN, COLOR_YELLOW, COLOR_RED, COLOR_MAGENTA
};
static const int pkt_scores[] = { 10, 15, 20, -1, -1 };  /* -1 = lose life */

/* ── Packet Entity ──────────────────────────────────────────────────── */

typedef struct {
    float      x, y;
    float      speed;
    PacketType type;
    int        active;
} Packet;

/* ── Game State ─────────────────────────────────────────────────────── */

typedef struct {
    float   catcher_x;
    Packet  packets[MAX_PACKETS];
    int     score;
    int     high_score;
    int     lives;
    int     level;
    float   spawn_timer;
    float   spawn_interval;
    float   fall_speed;

    /* Visual feedback */
    float   flash_timer;    /* screen flash on hit */
    uint16_t flash_color;

    /* Particles for caught packets */
    struct { float x, y, vx, vy, life; uint16_t color; } particles[32];

    enum { PC_TITLE, PC_PLAYING, PC_PAUSED, PC_GAMEOVER } state;
} CatcherGame;

/* ── Helpers ────────────────────────────────────────────────────────── */

static PacketType random_packet_type(int level) {
    int r = rand() % 100;
    /* Higher levels = more malware/deauth */
    int bad_chance = 15 + level * 3;
    if (bad_chance > 45) bad_chance = 45;

    if (r < bad_chance / 2)           return PKT_MALWARE;
    if (r < bad_chance)               return PKT_DEAUTH;
    if (r < bad_chance + 25)          return PKT_AUTH;
    if (r < bad_chance + 50)          return PKT_BEACON;
    return PKT_DATA;
}

static void spawn_packet(CatcherGame *g) {
    for (int i = 0; i < MAX_PACKETS; i++) {
        if (!g->packets[i].active) {
            g->packets[i].active = 1;
            g->packets[i].x = (float)(rand() % (SCREEN_W - PACKET_W));
            g->packets[i].y = -PACKET_H;
            g->packets[i].type = random_packet_type(g->level);
            g->packets[i].speed = g->fall_speed + (rand() % 30);
            return;
        }
    }
}

static void spawn_particles(CatcherGame *g, float x, float y, uint16_t color, int count) {
    for (int i = 0; i < 32 && count > 0; i++) {
        if (g->particles[i].life <= 0) {
            g->particles[i].x = x;
            g->particles[i].y = y;
            g->particles[i].vx = (rand() % 100 - 50) * 1.5f;
            g->particles[i].vy = -(rand() % 80 + 20) * 1.0f;
            g->particles[i].life = 0.5f + (rand() % 50) * 0.01f;
            g->particles[i].color = color;
            count--;
        }
    }
}

static void reset_catcher(CatcherGame *g) {
    g->catcher_x = SCREEN_W / 2.0f - CATCHER_W / 2.0f;
    g->score = 0;
    g->lives = MAX_LIVES;
    g->level = 1;
    g->spawn_timer = 0;
    g->spawn_interval = SPAWN_INTERVAL;
    g->fall_speed = BASE_FALL_SPEED;
    g->flash_timer = 0;
    memset(g->packets, 0, sizeof(g->packets));
    memset(g->particles, 0, sizeof(g->particles));
}

/* ── Callbacks ──────────────────────────────────────────────────────── */

static void catcher_init(Engine *engine, void *userdata) {
    (void)engine;
    CatcherGame *g = (CatcherGame *)userdata;
    srand((unsigned)time(NULL));
    memset(g, 0, sizeof(CatcherGame));
    g->state = PC_TITLE;
    g->high_score = 0;
    reset_catcher(g);
}

static void catcher_update(Engine *engine, float dt, void *userdata) {
    CatcherGame *g = (CatcherGame *)userdata;
    InputContext *inp = &engine->input;

    /* Update particles regardless of state */
    for (int i = 0; i < 32; i++) {
        if (g->particles[i].life > 0) {
            g->particles[i].x += g->particles[i].vx * dt;
            g->particles[i].y += g->particles[i].vy * dt;
            g->particles[i].vy += 200.0f * dt;  /* gravity */
            g->particles[i].life -= dt;
        }
    }

    if (g->flash_timer > 0) g->flash_timer -= dt;

    switch (g->state) {
    case PC_TITLE:
        if (input_pressed(inp, BTN_SELECT)) {
            reset_catcher(g);
            g->state = PC_PLAYING;
        }
        break;

    case PC_PAUSED:
        if (input_pressed(inp, BTN_SELECT))
            g->state = PC_PLAYING;
        break;

    case PC_GAMEOVER:
        if (input_pressed(inp, BTN_SELECT)) {
            reset_catcher(g);
            g->state = PC_PLAYING;
        }
        break;

    case PC_PLAYING:
        if (input_pressed(inp, BTN_SELECT)) {
            g->state = PC_PAUSED;
            break;
        }

        /* Move catcher */
        if (input_held(inp, BTN_LEFT))  g->catcher_x -= CATCHER_SPEED * dt;
        if (input_held(inp, BTN_RIGHT)) g->catcher_x += CATCHER_SPEED * dt;
        if (g->catcher_x < 0) g->catcher_x = 0;
        if (g->catcher_x > SCREEN_W - CATCHER_W) g->catcher_x = SCREEN_W - CATCHER_W;

        /* Spawn packets */
        g->spawn_timer += dt;
        if (g->spawn_timer >= g->spawn_interval) {
            g->spawn_timer -= g->spawn_interval;
            spawn_packet(g);
        }

        /* Update packets */
        for (int i = 0; i < MAX_PACKETS; i++) {
            Packet *p = &g->packets[i];
            if (!p->active) continue;

            p->y += p->speed * dt;

            /* Check catch */
            if (p->y + PACKET_H >= CATCHER_Y &&
                p->x + PACKET_W > g->catcher_x &&
                p->x < g->catcher_x + CATCHER_W) {

                p->active = 0;
                int pts = pkt_scores[p->type];

                if (pts > 0) {
                    g->score += pts;
                    spawn_particles(g, p->x + PACKET_W/2, CATCHER_Y, pkt_colors[p->type], 6);
                } else {
                    /* Bad packet — lose life */
                    g->lives--;
                    g->flash_timer = 0.15f;
                    g->flash_color = COLOR_RED;
                    spawn_particles(g, p->x + PACKET_W/2, CATCHER_Y, COLOR_RED, 10);

                    if (g->lives <= 0) {
                        g->state = PC_GAMEOVER;
                        if (g->score > g->high_score) g->high_score = g->score;
                    }
                }
            }

            /* Off screen — missed */
            if (p->y > SCREEN_H + PACKET_H) {
                p->active = 0;
            }
        }

        /* Level up */
        int new_level = g->score / POINTS_PER_LEVEL + 1;
        if (new_level > g->level) {
            g->level = new_level;
            g->fall_speed = BASE_FALL_SPEED + (g->level - 1) * SPEED_PER_LEVEL;
            g->spawn_interval = SPAWN_INTERVAL - (g->level - 1) * SPAWN_FASTER;
            if (g->spawn_interval < MIN_SPAWN) g->spawn_interval = MIN_SPAWN;
            g->flash_timer = 0.2f;
            g->flash_color = COLOR_HAK5_GREEN;
        }
        break;
    }
}

static void draw_packet(GfxContext *gfx, Packet *p) {
    uint16_t color = pkt_colors[p->type];
    int x = (int)p->x, y = (int)p->y;

    /* Packet body */
    gfx_rect_fill(gfx, x, y, PACKET_W, PACKET_H, color);
    gfx_rect(gfx, x, y, PACKET_W, PACKET_H, COLOR_WHITE);

    /* Label */
    const char *label = pkt_labels[p->type];
    int lx = x + (PACKET_W - (int)strlen(label) * 8) / 2;
    int ly = y + (PACKET_H - 8) / 2;
    gfx_text_scaled(gfx, lx, ly, label, COLOR_BLACK, 1);
}

static void catcher_render(Engine *engine, void *userdata) {
    CatcherGame *g = (CatcherGame *)userdata;
    GfxContext *gfx = &engine->gfx;

    /* Background */
    uint16_t bg = COLOR_HAK5_DARK;
    if (g->flash_timer > 0)
        bg = gfx_blend(g->flash_color, COLOR_HAK5_DARK, (uint8_t)(g->flash_timer * 255 / 0.2f));
    gfx_clear(gfx, bg);

    if (g->state == PC_TITLE) {
        gfx_text_centered(gfx, 10, "=== PACKET CATCHER ===", COLOR_HAK5_GREEN);
        gfx_text_centered(gfx, 38, "WiFi Pineapple Pager Edition", COLOR_TERMINAL);
        gfx_text_centered(gfx, 70, "Catch DATA, BCN, AUTH!", COLOR_WHITE);
        gfx_text_centered(gfx, 92, "Avoid DEAU and MALW!", COLOR_RED);
        gfx_text_centered(gfx, 120, "Left/Right to move", COLOR_GRAY);
        gfx_text_centered(gfx, 142, "Select to start/pause", COLOR_GRAY);
        if (g->high_score > 0)
            gfx_printf(gfx, (SCREEN_W - 14 * CHAR_W) / 2, 170, COLOR_YELLOW, "High Score: %d", g->high_score);
        gfx_text_centered(gfx, 198, "[ PRESS SELECT ]", COLOR_HAK5_GREEN);
        return;
    }

    if (g->state == PC_GAMEOVER) {
        gfx_text_centered(gfx, 20, "SIGNAL LOST", COLOR_RED);
        gfx_printf(gfx, (SCREEN_W - 12 * CHAR_W) / 2, 55, COLOR_WHITE, "Score: %d", g->score);
        gfx_printf(gfx, (SCREEN_W - 10 * CHAR_W) / 2, 80, COLOR_GRAY, "Level: %d", g->level);
        if (g->score >= g->high_score && g->score > 0)
            gfx_text_centered(gfx, 110, "** NEW HIGH SCORE **", COLOR_YELLOW);
        gfx_text_centered(gfx, 170, "[ SELECT to retry ]", COLOR_HAK5_GREEN);

        /* Still draw particles */
        for (int i = 0; i < 32; i++) {
            if (g->particles[i].life > 0) {
                gfx_rect_fill(gfx, (int)g->particles[i].x, (int)g->particles[i].y,
                             3, 3, g->particles[i].color);
            }
        }
        return;
    }

    /* HUD */
    gfx_rect_fill(gfx, 0, 0, SCREEN_W, 22, COLOR_HAK5_BLUE);
    gfx_printf(gfx, 4, 3, COLOR_WHITE, "SCORE: %d", g->score);
    gfx_printf(gfx, SCREEN_W/2 - 3 * CHAR_W, 3, COLOR_YELLOW, "LVL: %d", g->level);

    /* Lives as hearts */
    for (int i = 0; i < g->lives; i++) {
        int hx = SCREEN_W - 18 - i * 14;
        gfx_rect_fill(gfx, hx, 5, 12, 12, COLOR_RED);
    }

    gfx_hline(gfx, 0, 22, SCREEN_W, COLOR_HAK5_GREEN);

    /* Packets */
    for (int i = 0; i < MAX_PACKETS; i++) {
        if (g->packets[i].active)
            draw_packet(gfx, &g->packets[i]);
    }

    /* Catcher (antenna dish) */
    int cx = (int)g->catcher_x;
    gfx_rect_fill(gfx, cx, CATCHER_Y, CATCHER_W, CATCHER_H, COLOR_HAK5_GREEN);
    gfx_rect(gfx, cx, CATCHER_Y, CATCHER_W, CATCHER_H, COLOR_WHITE);
    /* Antenna */
    gfx_vline(gfx, cx + CATCHER_W/2, CATCHER_Y - 6, 6, COLOR_WHITE);
    gfx_hline(gfx, cx + CATCHER_W/2 - 3, CATCHER_Y - 6, 7, COLOR_CYAN);
    /* Label */
    gfx_text_scaled(gfx, cx + 4, CATCHER_Y + 5, "CATCH", COLOR_BLACK, 1);

    /* Particles */
    for (int i = 0; i < 32; i++) {
        if (g->particles[i].life > 0) {
            gfx_rect_fill(gfx, (int)g->particles[i].x, (int)g->particles[i].y,
                         3, 3, g->particles[i].color);
        }
    }

    /* Pause overlay */
    if (g->state == PC_PAUSED) {
        gfx_rect_fill(gfx, 140, 80, 200, 60, COLOR_HAK5_DARK);
        gfx_rect(gfx, 140, 80, 200, 60, COLOR_HAK5_GREEN);
        gfx_text_centered(gfx, 95, "PAUSED", COLOR_YELLOW);
        gfx_text_centered(gfx, 117, "Select to resume", COLOR_WHITE);
    }
}

static void catcher_cleanup(Engine *engine, void *userdata) {
    (void)engine;
    (void)userdata;
}

/* ── Main ───────────────────────────────────────────────────────────── */

int main(void) {
    CatcherGame game;
    Engine engine;

    if (engine_create(&engine, catcher_init, catcher_update, catcher_render, catcher_cleanup, &game) < 0) {
        fprintf(stderr, "Failed to create engine\n");
        return 1;
    }

    engine.target_fps = 30;
    engine_run(&engine);
    engine_destroy(&engine);

    return 0;
}
