/*
 * null_buddy_render.c -- Rendering functions for NULL-BUDDY
 *
 * Included by null_buddy.c -- not a standalone compilation unit.
 * Contains: nb_draw_ambient, nb_draw_bubble, draw_stat_bar,
 *           nb_draw_credit, draw_buddy, nb_draw_malware, nb_render
 */

static void nb_draw_ambient(GfxContext *gfx, NBGame *g) {
    if (g->buddy.stage < 3 && g->buddy.prestige == 0) return;
    for (int i = 0; i < NB_AMB_MAX; i++) {
        if (g->amb[i].life > 0) {
            /* Fade out as life decreases */
            float alpha = g->amb[i].life / g->amb[i].max_life;
            uint16_t c = g->amb[i].color;
            if (alpha < 0.4f) {
                /* Dim the color by halving RGB components */
                int r = ((c >> 11) & 0x1F) >> 1;
                int gr = ((c >> 5) & 0x3F) >> 1;
                int b = (c & 0x1F) >> 1;
                c = (uint16_t)((r << 11) | (gr << 5) | b);
            }
            int px = (int)g->amb[i].x;
            int py = (int)g->amb[i].y;
            int sz = g->amb[i].size;
            if (sz <= 1)
                gfx_pixel(gfx, px, py, c);
            else if (sz == 2)
                gfx_rect_fill(gfx, px, py, 2, 2, c);
            else
                gfx_circle_fill(gfx, px, py, sz, c);
        }
    }
}

/* Draw chat bubble above the buddy */
static void nb_draw_bubble(GfxContext *gfx, NBGame *g, int bx, int by) {
    if (g->bubble_timer <= 0) return;
    int len = (int)strlen(g->bubble_msg);
    int tw = len * 16 + 12;  /* 2x scale text width + padding */
    int th = 24;
    int px = bx - tw / 2;
    int py = by - 60;
    if (px < 2) px = 2;
    if (px + tw > SCREEN_W - 2) px = SCREEN_W - 2 - tw;
    if (py < 24) py = 24;

    /* Bubble background */
    gfx_rect_fill(gfx, px, py, tw, th, NB_PANEL_BG);
    gfx_rect(gfx, px, py, tw, th, NB_DIM_CYAN);
    /* Tail triangle */
    gfx_line(gfx, bx - 4, py + th, bx, py + th + 6, NB_DIM_CYAN);
    gfx_line(gfx, bx + 4, py + th, bx, py + th + 6, NB_DIM_CYAN);
    /* Text */
    /* Fade: dim color when nearly expired */
    uint16_t tc = g->bubble_timer > 0.5f ? NB_CYAN : NB_DIM_CYAN;
    gfx_text_scaled(gfx, px + 6, py + 4, g->bubble_msg, tc, 2);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     DRAWING HELPERS                                 */
/* ════════════════════════════════════════════════════════════════════ */

static void draw_stat_bar(GfxContext *gfx, int x, int y, int w, int h,
                          int val, int max_val, uint16_t fg, uint16_t bg,
                          const char *label, float anim_t) {
    gfx_rect_fill(gfx, x, y, w, h, bg);
    int fill = (val * w) / max_val;
    if (fill > w) fill = w;
    /* Flash bar when critical (< 15%) */
    int critical = (val * 100 / max_val) < 15;
    if (critical) {
        float blink = sinf(anim_t * 8.0f);
        fg = blink > 0 ? NB_RED : fg;
    }
    if (fill > 0)
        gfx_rect_fill(gfx, x, y, fill, h, fg);
    gfx_rect(gfx, x, y, w, h, critical ? NB_RED : COLOR_WHITE);
    gfx_text_scaled(gfx, x + 2, y + 1, label, COLOR_WHITE, 1);
}

/* Draw "By Hexxed BitHeadz" with per-character rainbow cycling */
static void nb_draw_credit(GfxContext *gfx, int x, int y, float t, int scale) {
    static const uint16_t rainbow[] = {
        NB_GREEN, NB_CYAN, NB_BLUE, NB_PURPLE, NB_PINK, NB_ORANGE, NB_YELLOW
    };
    const char *str = "By Hexxed BitHeadz";
    int cw = 8 * scale;
    for (int i = 0; str[i]; i++) {
        if (str[i] == ' ') { x += cw; continue; }
        int ci = ((int)(t * 4.0f) + i) % 7;
        gfx_char_scaled(gfx, x, y, str[i], rainbow[ci], scale);
        x += cw;
    }
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  SPRITE-BASED BUDDY RENDERING                                      */
/*  Six cyberpunk creature sprites across 18 stages.                  */
/*  Green accent pixels recolored to stage-specific accent color.     */
/* ═══════════════════════════════════════════════════════════════════ */
#include "null_buddy_sprites.h"
static void draw_buddy(GfxContext *gfx, NBGame *g, int cx, int cy) {
    BuddyStats *b = &g->buddy;
    uint16_t acc = nb_buddy_color(b);
    uint16_t dim_acc = nb_dim_color(acc);
    int S = b->stage;

    /* Mood-based bounce */
    float bounce_amp = 3.0f;
    if (b->happiness > 80) bounce_amp = 5.0f;
    else if (b->energy < 20) bounce_amp = 1.0f;
    else if (b->hunger < 30) bounce_amp = 1.5f;
    float bounce = sinf(g->bounce_t * 3.0f) * bounce_amp;
    int by = cy + (int)bounce;

    /* Determine mood → pick sprite + scale */
    int mood_id = NB_MOOD_DEFAULT;
    if (b->energy < 25)       mood_id = NB_MOOD_SLEEPY_ID;
    else if (b->hunger < 30)  mood_id = NB_MOOD_HUNGRY_ID;
    else if (b->happiness < 30) mood_id = NB_MOOD_SAD_ID;
    else if (b->happiness > 75)  mood_id = NB_MOOD_HAPPY_ID;

    const Sprite *spr;
    int scale;

    if (mood_id != NB_MOOD_DEFAULT) {
        /* Pick mood sprite — changes every ~8 sec, but shifts when talking */
        int rand_val = (int)(g->anim_t * 0.125f);
        if (g->bubble_timer > 0) rand_val += 7;
        const Sprite *mood_spr = nb_get_mood_sprite(mood_id, rand_val);
        if (mood_spr) {
            spr = mood_spr;
        } else {
            /* Mood pool empty \u2014 fall back to default */
            spr = nb_get_sprite(S);
        }
    } else {
        spr = nb_get_sprite(S);
    }

    /* Use smoothly interpolated scale (rounded to nearest int, min 1) */
    scale = (int)(g->display_scale + 0.5f);
    if (scale < 1) scale = 1;

    /* P6+ : Prestige blank glitch pulse — briefly flash to blank sprite
     * Starts very subtle at P6, ramps up through P12 */
    int using_blank = 0;
    if (b->prestige >= 6) {
        float pulse = sinf(g->anim_t * 6.0f + b->prestige * 0.3f);
        /* P6=0.97 (very rare) → P12=0.79 (frequent) */
        float thresh = 0.97f - (b->prestige - 6) * 0.03f;
        if (thresh < 0.79f) thresh = 0.79f;
        if (pulse > thresh) {
            const Sprite *blank = nb_get_blank_sprite();
            if (blank) {
                spr = blank;
                scale = NB_MOOD_SPRITE_SCALE;
                using_blank = 1;
            }
        }
    }

    /* Blit position (centered at cx, by) */
    int sx = cx - (spr->width * scale) / 2;
    int sy = by - (spr->height * scale) / 2;

    /* Flip sprite horizontally when walking left */
    int flip = (g->wander_facing == -1) ? 1 : 0;

    /* Draw recolored sprite */
    /* P5+ : Chromatic aberration — RGB color fringe on some frames */
    if (b->prestige >= 5 && ((int)(g->anim_t * 8.0f)) % 4 == 0) {
        int split = 2 + b->prestige / 4;
        if (split > 5) split = 5;
        nb_blit_tint(gfx, spr, sx - split, sy, scale, 0xF800, flip);
        nb_blit_tint(gfx, spr, sx + split, sy, scale, 0x001F, flip);
    }
    /* P10+ : Power surge — rare full-white flash frame */
    if (b->prestige >= 10 && rand() % 30 == 0) {
        nb_blit_recolor(gfx, spr, sx, sy, scale, 0xFFFF, 0xBDF7, flip);
    } else if (b->prestige >= 7) {
        /* Glitch blit — random scanline displacement */
        int glitch_pct = 5 + (b->prestige - 7) * 3;
        if (glitch_pct > 30) glitch_pct = 30;
        nb_blit_glitch(gfx, spr, sx, sy, scale, acc, dim_acc, glitch_pct, flip);
    } else {
        nb_blit_recolor(gfx, spr, sx, sy, scale, acc, dim_acc, flip);
    }
}

/* ── Prestige malware overlays ─ cute but dangerous! ────────────── */
static void nb_draw_malware(GfxContext *gfx, NBGame *g, int cx, int cy) {
    int pres = g->buddy.prestige;
    if (pres <= 0) return;
    float t = g->anim_t;

    /* Sprite-aware bounds instead of old body_r circle */
    const Sprite *spr = nb_get_sprite(g->buddy.stage);
    int scale = (int)(g->display_scale + 0.5f);
    if (scale < 1) scale = 1;
    int half_w = (spr->width * scale) / 2;
    int half_h = (spr->height * scale) / 2;

    /* P3+ : Electric arcs — crackling lightning bolts (far from body) */
    if (pres >= 3) {
        for (int i = 0; i < 2; i++) {
            float a = t * 3.0f + i * 3.14159f;
            int sx = cx + (int)(cosf(a) * (half_w + 12));
            int sy = cy + (int)(sinf(a) * (half_h + 8));
            /* Jagged bolt with 3 segments */
            int zx = sx, zy = sy;
            for (int j = 0; j < 3; j++) {
                int nx = zx + (rand() % 16) - 8;
                int ny = zy + (rand() % 12) - 6;
                gfx_line(gfx, zx, zy, nx, ny, NB_CYAN);
                gfx_line(gfx, zx + 1, zy, nx + 1, ny, NB_BLUE);
                zx = nx; zy = ny;
            }
        }
    }

    /* P9+ : Horizontal slice displacement — body splits for a frame */
    if (pres >= 9) {
        int glitch_chance = 5 - (pres - 9);
        if (glitch_chance < 2) glitch_chance = 2;
        if (rand() % glitch_chance == 0) {
            int slice_y = cy - half_h / 2 + rand() % half_h;
            int shift = (rand() % 12) - 6;
            int slice_h = 2 + rand() % 4;
            gfx_rect_fill(gfx, cx - half_w + shift, slice_y,
                          half_w * 2, slice_h, nb_buddy_color(&g->buddy));
        }
    }

    /* P10+ : Color glitch — random wrong-colored patches flash on body */
    if (pres >= 10) {
        if (rand() % 4 == 0) {
            uint16_t glitch_colors[] = { NB_RED, 0xFFFF, NB_CYAN, NB_PURPLE };
            uint16_t gc = glitch_colors[rand() % 4];
            int gx = cx - half_w / 2 + rand() % half_w;
            int gy = cy - half_h / 2 + rand() % half_h;
            int gsz = 3 + rand() % 5;
            gfx_rect_fill(gfx, gx, gy, gsz, gsz, gc);
        }
    }

    /* P11+ : Sprite glitch double — a shifted ghost copy flickers */
    if (pres >= 11) {
        if (rand() % 3 == 0) {
            int tear_dx = (rand() % 10) - 5;
            int tear_dy = (rand() % 8) - 4;
            int ghost_sx = cx - half_w + tear_dx;
            int ghost_sy = cy - half_h + tear_dy;
            nb_blit_ghost(gfx, spr, ghost_sx, ghost_sy, scale, 2, 0);
        }
    }

    /* P12+ : Static noise pixels — body dissolving into data */
    if (pres >= 12) {
        int noise_n = 4 + (pres - 12) * 3;
        if (noise_n > 20) noise_n = 20;
        for (int i = 0; i < noise_n; i++) {
            int nx = cx - half_w + rand() % (half_w * 2);
            int ny = cy - half_h + rand() % (half_h * 2);
            uint16_t nc = (rand() % 2) ? 0xFFFF : NB_GREEN;
            gfx_pixel(gfx, nx, ny, nc);
            gfx_pixel(gfx, nx + 1, ny, nc);
        }
    }

    /* P13+ : Scanlines through body — CRT corruption */
    if (pres >= 13) {
        for (int sl = 0; sl < 3 + pres - 13; sl++) {
            if (sl > 6) break;
            int sly = cy - half_h + rand() % (half_h * 2);
            uint16_t slc = (rand() % 2) ? NB_DIM_GREEN : NB_DIM_CYAN;
            gfx_hline(gfx, cx - half_w, sly, half_w * 2, slc);
        }
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     RENDER                                          */
/* ════════════════════════════════════════════════════════════════════ */

static void nb_render(Engine *engine, void *userdata) {
    NBGame *g = (NBGame *)userdata;
    GfxContext *gfx = &engine->gfx;
    BuddyStats *b = &g->buddy;
    uint16_t acc = nb_buddy_color(b);

    gfx_clear(gfx, NB_BG);

    /* ── TITLE SCREEN ──────────────────────────────────────────────── */
    if (g->state == NB_STATE_TITLE) {
        /* Scanlines */
        for (int y = 0; y < SCREEN_H; y += 4)
            gfx_hline(gfx, 0, y, SCREEN_W, NB_GRID_LINE);

        gfx_text_centered_scaled(gfx, 16, "NULL-BUDDY", NB_GREEN, 4);
        gfx_text_centered(gfx, 60, "CYBER-TAMAGOTCHI", NB_CYAN);
        gfx_text_centered_scaled(gfx, 82, "WiFi Pineapple Pager", NB_DIM_GREEN, 1);

        /* Animated terminal text */
        gfx_text_centered_scaled(gfx, 110, TERMINAL_MSGS[g->term_idx], NB_GREEN, 1);

        float pulse = (sinf(g->anim_t * 4.0f) + 1.0f) * 0.5f;
        uint16_t btn_col = gfx_blend(NB_GREEN, NB_DIM_GREEN, (uint8_t)(pulse * 200));
        gfx_text_centered(gfx, 150, "[ PRESS A ]", btn_col);

        /* Credit */
        nb_draw_credit(gfx, SCREEN_W - 144, SCREEN_H - 12, g->anim_t, 1);
        return;
    }

    /* ── NAMING SCREEN ─────────────────────────────────────────────── */
    if (g->state == NB_STATE_NAMING) {
        gfx_text_centered(gfx, 8, "NAME YOUR BUDDY", NB_CYAN);
        gfx_text_centered_scaled(gfx, 34, "Up/Down: letter  L/R: move  A: ok", NB_DIM_GREEN, 1);

        /* Draw name with cursor */
        int nx = (SCREEN_W - NB_MAX_NAME * 20) / 2;
        for (int i = 0; i <= g->name_cursor; i++) {
            char ch[2] = { b->name[i] ? b->name[i] : '_', '\0' };
            uint16_t col = (i == g->name_cursor) ? NB_YELLOW : NB_GREEN;
            gfx_text(gfx, nx + i * 20, 80, ch, col);
        }

        /* Cursor underline */
        int ux = nx + g->name_cursor * 20;
        float blink = fmodf(g->anim_t, 0.6f) < 0.3f ? 1.0f : 0.0f;
        if (blink > 0)
            gfx_hline(gfx, ux, 100, 16, NB_YELLOW);

        /* Preview arrow hints */
        char up_ch[2] = { 'A' + (g->name_char + 1) % 26, '\0' };
        char dn_ch[2] = { 'A' + (g->name_char + 25) % 26, '\0' };
        gfx_text(gfx, ux + 2, 56, up_ch, NB_DIM_GREEN);
        gfx_text(gfx, ux + 2, 108, dn_ch, NB_DIM_GREEN);
        return;
    }

    /* ── CONFIRM NAME ──────────────────────────────────────────────── */
    if (g->state == NB_STATE_CONFIRM_NAME) {
        gfx_text_centered(gfx, 20, "Name your buddy:", NB_DIM_GREEN);
        gfx_text_centered_scaled(gfx, 55, b->name, NB_YELLOW, 3);

        gfx_text_centered(gfx, 120, "Is this right?", NB_CYAN);

        int bw = 80, bh = 28;
        int gap = 40;
        int bx_no  = SCREEN_W / 2 - bw - gap / 2;
        int bx_yes = SCREEN_W / 2 + gap / 2;
        int by = 150;

        /* NO button */
        if (g->confirm_cursor == 0) {
            gfx_rect_fill(gfx, bx_no, by, bw, bh, NB_MENU_SEL);
            gfx_rect(gfx, bx_no, by, bw, bh, NB_RED);
        } else {
            gfx_rect(gfx, bx_no, by, bw, bh, NB_DIM_GREEN);
        }
        gfx_text(gfx, bx_no + 24, by + 5, "NO", g->confirm_cursor == 0 ? NB_RED : NB_DIM_GREEN);

        /* YES button */
        if (g->confirm_cursor == 1) {
            gfx_rect_fill(gfx, bx_yes, by, bw, bh, NB_MENU_SEL);
            gfx_rect(gfx, bx_yes, by, bw, bh, NB_GREEN);
        } else {
            gfx_rect(gfx, bx_yes, by, bw, bh, NB_DIM_GREEN);
        }
        gfx_text(gfx, bx_yes + 16, by + 5, "YES", g->confirm_cursor == 1 ? NB_GREEN : NB_DIM_GREEN);

        gfx_text_centered_scaled(gfx, 195, "A: select  B: back", NB_DIM_GREEN, 1);
        return;
    }

    /* ── TUTORIAL ──────────────────────────────────────────────────── */
    if (g->state == NB_STATE_TUTORIAL) {
        /* Scanlines for style */
        for (int y = 0; y < SCREEN_H; y += 4)
            gfx_hline(gfx, 0, y, SCREEN_W, NB_GRID_LINE);

        gfx_text_centered_scaled(gfx, 4, "TUTORIAL", NB_CYAN, 3);

        int page = g->tutorial_page;
        int ty = 42;

        switch (page) {
        case 0:
            gfx_text_centered(gfx, ty,      "Welcome to NULL-BUDDY!", NB_GREEN);
            gfx_text_centered(gfx, ty + 30,  "Your cyber-pet lives", NB_DIM_GREEN);
            gfx_text_centered(gfx, ty + 55,  "on your Pager!", NB_DIM_GREEN);
            gfx_text_centered(gfx, ty + 90,  "Take care of it and", NB_CYAN);
            gfx_text_centered(gfx, ty + 115, "watch it level up!", NB_CYAN);
            break;
        case 1:
            gfx_text_centered(gfx, ty,      "TAKING CARE", NB_YELLOW);
            gfx_text_centered(gfx, ty + 30, "FEED: fills hunger", NB_GREEN);
            gfx_text_centered(gfx, ty + 55, "PET: boosts happiness", NB_PINK);
            gfx_text_centered(gfx, ty + 85, "Stats decay over time", NB_DIM_GREEN);
            gfx_text_centered(gfx, ty + 110,"Don't neglect buddy!", NB_RED);
            break;
        case 2:
            gfx_text_centered(gfx, ty,      "MINI-GAME", NB_YELLOW);
            gfx_text_centered(gfx, ty + 30, "PLAY to catch packets!", NB_GREEN);
            gfx_text_centered(gfx, ty + 55, "Green=good  Red=bad", NB_CYAN);
            gfx_text_centered(gfx, ty + 85, "Use L/R to move", NB_DIM_GREEN);
            gfx_text_centered(gfx, ty + 110,"Score earns XP!", NB_YELLOW);
            break;
        case 3:
            gfx_text_centered(gfx, ty,      "LEVELING UP", NB_YELLOW);
            gfx_text_centered(gfx, ty + 30, "Earn XP to level up!", NB_GREEN);
            gfx_text_centered(gfx, ty + 55, "18 stages in 3 tiers!", NB_CYAN);
            gfx_text_centered(gfx, ty + 80, "Your buddy looks unique", NB_CYAN);
            gfx_text_centered(gfx, ty + 105,"based on its name!", NB_CYAN);
            break;
        case 4:
            gfx_text_centered(gfx, ty,      "RECON FEED", NB_YELLOW);
            gfx_text_centered(gfx, ty + 30, "OPTIONS > RECON FEED", NB_GREEN);
            gfx_text_centered(gfx, ty + 55, "XP from SSIDs, Clients,", NB_CYAN);
            gfx_text_centered(gfx, ty + 80, "& Handshake captures!", NB_CYAN);
            gfx_text_centered(gfx, ty + 105,"SSID milestones = bonus!", NB_GREEN);
            break;
        case 5:
            gfx_text_centered(gfx, ty,      "PRESTIGE", NB_YELLOW);
            gfx_text_centered(gfx, ty + 30, "Hit max stage to unlock", NB_GREEN);
            gfx_text_centered(gfx, ty + 55, "PRESTIGE in STATS!", NB_CYAN);
            gfx_text_centered(gfx, ty + 80, "Resets rank, keeps stats", NB_CYAN);
            gfx_text_centered(gfx, ty + 105,"+ crazier visual effects!", NB_PINK);
            break;
        case 6:
            gfx_text_centered(gfx, ty,      "CONTROLS", NB_YELLOW);
            gfx_text_centered(gfx, ty + 30, "D-pad: Navigate", NB_GREEN);
            gfx_text_centered(gfx, ty + 55, "A: Confirm / Action", NB_GREEN);
            gfx_text_centered(gfx, ty + 80, "B: Back", NB_GREEN);
            gfx_text_centered(gfx, ty + 110,"Hold B: Quit game", NB_DIM_GREEN);
            break;
        }

        /* Page indicator dots */
        int dot_total = NB_TUTORIAL_PAGES * 12;
        int dot_x = (SCREEN_W - dot_total) / 2;
        for (int i = 0; i < NB_TUTORIAL_PAGES; i++) {
            uint16_t dcol = (i == page) ? NB_GREEN : NB_DIM_GREEN;
            gfx_rect_fill(gfx, dot_x + i * 12, SCREEN_H - 30, 6, 6, dcol);
        }

        /* Navigation hint */
        if (page < NB_TUTORIAL_PAGES - 1)
            gfx_text_centered_scaled(gfx, SCREEN_H - 16, "[A: Next  B: Skip]", NB_DIM_GREEN, 1);
        else
            gfx_text_centered_scaled(gfx, SCREEN_H - 16, "[A: Start!]", NB_GREEN, 1);
        return;
    }

    /* ── STATS SCREEN ──────────────────────────────────────────────── */
    if (g->state == NB_STATE_STATS) {
        gfx_text_centered(gfx, 2, "BUDDY STATS", NB_CYAN);
        gfx_rect_fill(gfx, 0, 22, SCREEN_W, 2, NB_DIM_CYAN);

        /*
         * Two-column layout at 2x scale (16px chars).
         * Left column: identity info.  Right column: counters.
         * 222px tall screen - 24px header - 20px footer = 178px for content.
         * 5 rows × 20px each side fits well.
         */
        int col1 = 8;                     /* left column x */
        int col2 = SCREEN_W / 2 + 8;      /* right column x (248) */
        int y = 28;
        int rh = 20;                       /* row height */

        /* Left column — identity */
        gfx_printf(gfx, col1, y, NB_GREEN, "%s", b->name);
        if (b->prestige > 0)
            gfx_printf(gfx, col1 + (int)strlen(b->name) * CHAR_W + 8, y, acc,
                       "%s +%d", STAGES[b->stage].name, b->prestige);
        else
            gfx_printf(gfx, col1 + (int)strlen(b->name) * CHAR_W + 8, y, acc,
                       "%s", STAGES[b->stage].name);
        y += rh + 4;

        gfx_printf(gfx, col1, y, NB_CYAN, "XP: %d", b->xp);
        y += rh;

        if (b->stage < NB_NUM_STAGES - 1) {
            int div = g->dev_mode ? 100 : 1;
            gfx_printf(gfx, col1, y, NB_DIM_GREEN, "Next: %d", STAGES[b->stage + 1].xp_threshold / div);
        } else
            gfx_text(gfx, col1, y, "MAX STAGE!", NB_YELLOW);
        y += rh;

        gfx_printf(gfx, col1, y, NB_GREEN, "Age: %d min", b->age_minutes);
        y += rh;

        /* Right column — counters */
        int y2 = 28 + rh + 4;

        gfx_printf(gfx, col2, y2, NB_BLUE,   "Fed:  %d", b->times_fed);    y2 += rh;
        gfx_printf(gfx, col2, y2, NB_PINK,   "Pet:  %d", b->times_pet);    y2 += rh;
        gfx_printf(gfx, col2, y2, NB_ORANGE, "Play: %d", b->times_played); y2 += rh;
        gfx_printf(gfx, col2, y2, NB_PURPLE, "Pkts: %d", b->total_packets); y2 += rh;
        gfx_printf(gfx, col2, y2, NB_YELLOW, "Best: %d", b->mg_high_score);

        /* Divider */
        gfx_vline(gfx, SCREEN_W / 2, 28, SCREEN_H - 52, NB_DIM_CYAN);

        if (b->stage >= NB_NUM_STAGES - 1)
            gfx_text_centered(gfx, SCREEN_H - 20, "[A] PRESTIGE  [B] Back", NB_YELLOW);
        else
            gfx_text_centered(gfx, SCREEN_H - 20, "[B] Back", NB_DIM_GREEN);
        return;
    }

    /* ── OPTIONS ───────────────────────────────────────────────────── */
    if (g->state == NB_STATE_OPTIONS) {
        /* Scanlines */
        for (int y = 0; y < SCREEN_H; y += 4)
            gfx_hline(gfx, 0, y, SCREEN_W, NB_GRID_LINE);

        gfx_text_centered_scaled(gfx, 10, "OPTIONS", NB_CYAN, 3);
        gfx_rect_fill(gfx, 0, 42, SCREEN_W, 2, NB_DIM_CYAN);

        static const char *opt_labels[] = { "RECON FEED", "RESET BUDDY", "TUTORIAL", "DEV MODE", "DEMO MODE" };
        static const uint16_t opt_colors[] = { NB_GREEN, NB_RED, NB_CYAN, NB_ORANGE, NB_PURPLE };

        int oy = 56;
        for (int i = 0; i < OPTIONS_COUNT; i++) {
            int my = oy + i * 30;
            int mx = SCREEN_W / 2 - 80;
            if (i == g->options_cursor) {
                gfx_rect_fill(gfx, mx - 6, my - 4, 220, 24, NB_MENU_SEL);
                gfx_rect(gfx, mx - 6, my - 4, 220, 24, opt_colors[i]);
                gfx_text(gfx, mx, my, ">", opt_colors[i]);
            }
            gfx_text(gfx, mx + 20, my, opt_labels[i],
                     i == g->options_cursor ? opt_colors[i] : NB_DIM_GREEN);
            /* Show dev mode status */
            if (i == 3) {
                const char *status = g->dev_mode ? "[ON]" : "[OFF]";
                uint16_t scol = g->dev_mode ? NB_YELLOW : NB_DIM_GREEN;
                gfx_text(gfx, mx + 180, my, status, scol);
            }
            /* Show demo mode status */
            if (i == 4) {
                const char *status = g->demo_mode ? "[ON]" : "[OFF]";
                uint16_t scol = g->demo_mode ? NB_YELLOW : NB_DIM_GREEN;
                gfx_text(gfx, mx + 196, my, status, scol);
            }
        }

        gfx_text_centered_scaled(gfx, SCREEN_H - 16, "[ B: Back ]", NB_DIM_GREEN, 1);
        return;
    }

    /* ── CONFIRM RESET ─────────────────────────────────────────────── */
    if (g->state == NB_STATE_CONFIRM_RESET) {
        /* Dim background */
        for (int y = 0; y < SCREEN_H; y += 2)
            gfx_hline(gfx, 0, y, SCREEN_W, NB_BG);

        /* Dialog box */
        int dw = 340, dh = 100;
        int dx = (SCREEN_W - dw) / 2;
        int dy = (SCREEN_H - dh) / 2;
        gfx_rect_fill(gfx, dx, dy, dw, dh, NB_DARK_BG);
        gfx_rect(gfx, dx, dy, dw, dh, NB_RED);
        gfx_rect(gfx, dx + 2, dy + 2, dw - 4, dh - 4, NB_DIM_PINK);

        gfx_text_centered(gfx, dy + 10, "DELETE BUDDY?", NB_RED);
        gfx_text_centered_scaled(gfx, dy + 34, "This cannot be undone!", NB_YELLOW, 1);

        /* No / Yes buttons */
        int btn_w = 80, btn_h = 24;
        int no_x  = SCREEN_W / 2 - btn_w - 20;
        int yes_x = SCREEN_W / 2 + 20;
        int btn_y = dy + dh - btn_h - 14;

        /* NO button */
        if (g->confirm_cursor == 0) {
            gfx_rect_fill(gfx, no_x, btn_y, btn_w, btn_h, NB_MENU_SEL);
            gfx_rect(gfx, no_x, btn_y, btn_w, btn_h, NB_GREEN);
        } else {
            gfx_rect(gfx, no_x, btn_y, btn_w, btn_h, NB_DIM_GREEN);
        }
        gfx_text(gfx, no_x + 24, btn_y + 4, "NO", g->confirm_cursor == 0 ? NB_GREEN : NB_DIM_GREEN);

        /* YES button */
        if (g->confirm_cursor == 1) {
            gfx_rect_fill(gfx, yes_x, btn_y, btn_w, btn_h, NB_MENU_SEL);
            gfx_rect(gfx, yes_x, btn_y, btn_w, btn_h, NB_RED);
        } else {
            gfx_rect(gfx, yes_x, btn_y, btn_w, btn_h, NB_DIM_PINK);
        }
        gfx_text(gfx, yes_x + 16, btn_y + 4, "YES", g->confirm_cursor == 1 ? NB_RED : NB_DIM_PINK);
        return;
    }

    /* ── MINI-GAME ─────────────────────────────────────────────────── */
    if (g->state == NB_STATE_PLAY) {
        /* HUD */
        gfx_rect_fill(gfx, 0, 0, SCREEN_W, 20, NB_HUD_BG);
        gfx_printf(gfx, 4, 2, NB_GREEN, "SCORE:%d", g->mg_score);
        gfx_printf(gfx, SCREEN_W / 2 - 24, 2, NB_YELLOW, "%.0fs",
                          g->mg_timer > 0 ? g->mg_timer : 0.0f);
        gfx_printf(gfx, SCREEN_W - 110, 2, NB_RED, "BAD:%d", g->mg_misses);
        gfx_hline(gfx, 0, 20, SCREEN_W, NB_CYAN);

        /* Packets */
        for (int i = 0; i < MG_MAX_PACKETS; i++) {
            MGPacket *p = &g->mg_packets[i];
            if (!p->active) continue;
            uint16_t col = p->good ? NB_GREEN : NB_RED;
            gfx_rect_fill(gfx, (int)p->x, (int)p->y, MG_PACKET_W, MG_PACKET_H, col);
            gfx_rect(gfx, (int)p->x, (int)p->y, MG_PACKET_W, MG_PACKET_H, COLOR_WHITE);
            const char *lbl = p->good ? "PKT" : "MAL";
            gfx_text_scaled(gfx, (int)p->x + 2, (int)p->y + 2, lbl, COLOR_BLACK, 1);
        }

        /* Catcher */
        int cy = SCREEN_H - MG_PLAYER_H - 4;
        int cx = (int)g->mg_player_x;
        gfx_rect_fill(gfx, cx, cy, MG_PLAYER_W, MG_PLAYER_H, NB_CYAN);
        gfx_rect(gfx, cx, cy, MG_PLAYER_W, MG_PLAYER_H, COLOR_WHITE);
        gfx_text_scaled(gfx, cx + 4, cy + 2, "RECV", COLOR_BLACK, 1);

        /* Particles */
        for (int i = 0; i < 16; i++) {
            if (g->particles[i].life > 0)
                gfx_rect_fill(gfx, (int)g->particles[i].x, (int)g->particles[i].y,
                               3, 3, g->particles[i].color);
        }
        return;
    }

    /* ── HOME SCREEN ───────────────────────────────────────────────── */

    /* Background grid */
    for (int y = 0; y < SCREEN_H; y += 16)
        gfx_hline(gfx, 0, y, SCREEN_W, NB_GRID_LINE);
    for (int x = 0; x < SCREEN_W; x += 16)
        gfx_vline(gfx, x, 0, SCREEN_H, NB_GRID_LINE);

    /* Matrix rain background — columns scale aggressively with prestige */
    /* Keep rain to left 2/3 of screen to avoid menu overlap */
    int rain_max_x = SCREEN_W - 160;  /* stop before menu at SCREEN_W - 150 */
    if (b->prestige > 0) {
        int cols = 4 + b->prestige * 5;
        if (cols > 60) cols = 60;
        for (int i = 0; i < cols; i++) {
            /* Spread columns across the rain zone only */
            int col_x = ((i * 137 + 29 + i * i * 7) % (rain_max_x / 8)) * 8;
            /* Each column drops chars at its own speed — more variation */
            float speed = 0.8f + (i % 7) * 0.35f + (i % 3) * 0.2f;
            float phase = g->anim_t * speed + i * 2.3f + (float)(i * 41 % 17);
            int head_y = (int)(fmodf(phase, 5.0f) * (float)SCREEN_H / 3.0f);
            /* Trail length grows fast with prestige */
            int trail = 4 + b->prestige * 3;
            if (trail > 22) trail = 22;
            for (int j = 0; j < trail; j++) {
                int cy = head_y - j * 10;
                if (cy < 0 || cy >= SCREEN_H) continue;
                /* Mix hex + symbols at higher prestige */
                int ci = (int)(g->anim_t * 6.0f + i * 3 + j * 7);
                char ch;
                if (b->prestige >= 8 && (ci % 5 == 0))
                    ch = "!@#$%^&*~"[ci % 9];
                else if (b->prestige >= 4)
                    ch = "0123456789ABCDEF"[ci % 16];
                else
                    ch = '0' + (ci % 10);
                /* Head char is bright, trail fades */
                uint16_t c;
                if (j == 0) {
                    if (b->prestige >= 10 && (i % 5 == 0))
                        c = NB_RED;
                    else if (b->prestige >= 6 && (i % 3 == 0))
                        c = NB_CYAN;
                    else
                        c = NB_GREEN;
                } else {
                    int dim = j * 2;
                    int r = 0;
                    int gg = (0x44 - dim * 6); if (gg < 0x08) gg = 0x08;
                    int bl = (0x1A - dim * 3); if (bl < 0x04) bl = 0x04;
                    c = RGB565(r, gg, bl);
                }
                gfx_char_scaled(gfx, col_x, cy, ch, c, 1);
            }
        }
    }

    /* P6+ : Screen edge glow — pulsing colored border */
    if (b->prestige >= 6) {
        float edge_pulse = (sinf(g->anim_t * 2.5f) + 1.0f) * 0.5f;
        int glow_w = 2 + (b->prestige - 6);
        if (glow_w > 6) glow_w = 6;
        uint16_t glow_arr[] = { NB_CYAN, NB_PURPLE, NB_PINK, NB_BLUE };
        uint16_t gc = glow_arr[((int)(g->anim_t * 1.5f)) % 4];
        if (edge_pulse < 0.4f) {
            int rr = ((gc >> 11) & 0x1F) >> 1;
            int gg = ((gc >> 5) & 0x3F) >> 1;
            int bb = (gc & 0x1F) >> 1;
            gc = (uint16_t)((rr << 11) | (gg << 5) | bb);
        }
        gfx_rect_fill(gfx, 0, 0, glow_w, SCREEN_H, gc);
        gfx_rect_fill(gfx, SCREEN_W - glow_w, 0, glow_w, SCREEN_H, gc);
        gfx_rect_fill(gfx, 0, SCREEN_H - glow_w, SCREEN_W, glow_w, gc);
        gfx_rect_fill(gfx, 0, 0, SCREEN_W, glow_w, gc);
    }

    /* Top bar: name + stage + prestige stars */
    gfx_rect_fill(gfx, 0, 0, SCREEN_W, 22, NB_HUD_BG);
    gfx_printf(gfx, 4, 3, acc, "%s", b->name);
    {
        int name_end = 4 + (int)strlen(b->name) * CHAR_W;
        /* Prestige stars — one star per prestige, max 5 */
        if (b->prestige > 0) {
            int stars = b->prestige;
            if (stars > 5) stars = 5;
            for (int i = 0; i < stars; i++) {
                gfx_char_scaled(gfx, name_end + i * 10, 4, '*', NB_YELLOW, 1);
            }
            name_end += stars * 10 + 2;
            if (b->prestige > 5) {
                gfx_printf_scaled(gfx, name_end, 6, NB_YELLOW, 1,
                                  "+%d", b->prestige - 5);
                name_end += 24;
            }
        }
        /* Stage label */
        gfx_printf_scaled(gfx, name_end + 4, 6, NB_DIM_GREEN, 1,
                          "[%s]", STAGES[b->stage].name);
    }
    {
        char xp_buf[20];
        snprintf(xp_buf, sizeof(xp_buf), "XP:%d", b->xp);
        int xp_w = (int)strlen(xp_buf) * CHAR_W;
        gfx_text(gfx, SCREEN_W - xp_w - 4, 3, xp_buf, NB_CYAN);
    }
    gfx_hline(gfx, 0, 22, SCREEN_W, acc);

    /* Stat bars (right side) */
    int bar_x = SCREEN_W - 170;
    int bar_w = 160;
    int bar_h = 14;
    draw_stat_bar(gfx, bar_x, 28, bar_w, bar_h, b->hunger,    NB_MAX_STAT, NB_HUNGER_BAR, NB_BAR_BG, "HNG", g->anim_t);
    draw_stat_bar(gfx, bar_x, 48, bar_w, bar_h, b->happiness, NB_MAX_STAT, NB_HAPPY_BAR,  NB_BAR_BG, "HAP", g->anim_t);
    draw_stat_bar(gfx, bar_x, 68, bar_w, bar_h, b->energy,    NB_MAX_STAT, NB_ENERGY_BAR, NB_BAR_BG, "ENG", g->anim_t);

    /* Ambient background particles (behind buddy) */
    nb_draw_ambient(gfx, g);

    /* P1+ : Sprite ghost trail (behind buddy) — dimmed copies of actual sprite */
    if (b->prestige > 0) {
        const Sprite *trail_spr = nb_get_sprite(b->stage);
        int trail_scale = (int)(g->display_scale + 0.5f);
        if (trail_scale < 1) trail_scale = 1;
        int ghosts = b->prestige < 3 ? 2 : 3;
        for (int ai = 0; ai < ghosts && ai < 6; ai++) {
            int idx = (g->afterimage_idx + 6 - ai) % 6;
            float aix = g->afterimages[idx].x;
            float aiy = g->afterimages[idx].y;
            float dist = fabsf(aix - g->wander_x) +
                         fabsf(aiy - sinf(g->bounce_t * 3.0f) * 3.0f);
            if (dist < 3.0f) continue;
            int ghost_cx = SCREEN_W / 3 - 20 + (int)aix;
            int ghost_cy = SCREEN_H / 2 + 16 + (int)aiy;
            int ghost_sx = ghost_cx - (trail_spr->width * trail_scale) / 2;
            int ghost_sy = ghost_cy - (trail_spr->height * trail_scale) / 2;
            nb_blit_ghost(gfx, trail_spr, ghost_sx, ghost_sy, trail_scale, ai + 2,
                         (g->wander_facing == -1) ? 1 : 0);
        }
    }

    /* Buddy (left-center area) — with wandering + fidget offsets */
    int fidget_dx = 0, fidget_dy = 0;
    nb_get_fidget_offset(g, &fidget_dx, &fidget_dy);
    int buddy_cx = SCREEN_W / 3 - 20 + (int)g->wander_x + fidget_dx;
    int buddy_cy = SCREEN_H / 2 + 16 + fidget_dy;

    /* P15+ : Screen shake on big events */
    if (g->shake_timer > 0) {
        buddy_cx += (rand() % 10) - 5;
        buddy_cy += (rand() % 8) - 4;
    }

    draw_buddy(gfx, g, buddy_cx, buddy_cy);
    nb_draw_malware(gfx, g, buddy_cx, buddy_cy);

    /* S12+ : Teleport visual effect */
    if (g->teleport_flash > 0 && b->stage >= 12) {
        float tf = g->teleport_flash;
        /* Static burst at old position — dissolving noise */
        int old_cx = SCREEN_W / 3 - 20 + (int)g->teleport_old_x;
        int old_cy = SCREEN_H / 2 + 16;
        int burst_r = (int)(20.0f + tf * 40.0f);
        int n_static = (int)(tf * 80.0f);
        for (int si = 0; si < n_static; si++) {
            int sx = old_cx + (rand() % (burst_r * 2 + 1)) - burst_r;
            int sy = old_cy + (rand() % (burst_r * 2 + 1)) - burst_r;
            uint16_t sc = (rand() % 2) ? NB_DIM_GREEN : NB_DIM_CYAN;
            gfx_pixel(gfx, sx, sy, sc);
            gfx_pixel(gfx, sx + 1, sy, sc);
        }
        /* Flash at new position — horizontal glitch burst */
        if (tf > 0.15f) {
            uint16_t tp_accent = nb_buddy_color(b);
            const Sprite *tp_spr = nb_get_sprite(b->stage);
            int tp_scale = nb_get_sprite_scale(b->stage);
            int tp_hw = (tp_spr->width * tp_scale) / 2;
            int tp_hh = (tp_spr->height * tp_scale) / 2;
            int n_lines = 4 + (int)(tf * 8.0f);
            for (int li = 0; li < n_lines; li++) {
                int ly = buddy_cy - tp_hh + rand() % (tp_hh * 2);
                gfx_hline(gfx, buddy_cx - tp_hw - 4, ly, tp_hw * 2 + 8, tp_accent);
            }
        }
    }

    /* P12+ : Pulsing sprite outline glow — rectangular, not circular */
    if (b->prestige >= 12) {
        const Sprite *gs = nb_get_sprite(b->stage);
        int gscale = nb_get_sprite_scale(b->stage);
        int ghw = (gs->width * gscale) / 2;
        int ghh = (gs->height * gscale) / 2;
        float glow_pulse = (sinf(g->anim_t * 4.0f) + 1.0f) * 0.5f;
        int pad = 3 + (int)(glow_pulse * 5.0f);
        uint16_t glow_arr[] = { NB_CYAN, NB_PINK, NB_PURPLE, NB_YELLOW };
        uint16_t gc = glow_arr[((int)(g->anim_t * 2.0f)) % 4];
        if (glow_pulse < 0.3f) {
            int rr = ((gc >> 11) & 0x1F) >> 1;
            int gg = ((gc >> 5) & 0x3F) >> 1;
            int bb = (gc & 0x1F) >> 1;
            gc = (uint16_t)((rr << 11) | (gg << 5) | bb);
        }
        gfx_rect(gfx, buddy_cx - ghw - pad, buddy_cy - ghh - pad,
                 ghw * 2 + pad * 2, ghh * 2 + pad * 2, gc);
        gfx_rect(gfx, buddy_cx - ghw - pad - 1, buddy_cy - ghh - pad - 1,
                 ghw * 2 + pad * 2 + 2, ghh * 2 + pad * 2 + 2, gc);
    }

    /* Sleep zzZ — floating when energy is low */
    if (b->energy < 20) {
        float zt = g->anim_t * 1.2f;
        int zx = buddy_cx + 12;
        int zy = buddy_cy - 30;
        /* Three Z's floating upward at different phases */
        for (int i = 0; i < 3; i++) {
            float phase = fmodf(zt + i * 1.2f, 3.5f);
            int dy = (int)(phase * 8.0f);
            int dx = i * 6;
            int scale = (i == 2) ? 2 : 1;
            float fade = 1.0f - phase / 3.5f;
            uint16_t zc = fade > 0.5f ? NB_CYAN : NB_DIM_CYAN;
            gfx_char_scaled(gfx, zx + dx, zy - dy, 'Z', zc, scale);
        }
    }

    /* Chat bubble (drawn above buddy) */
    nb_draw_bubble(gfx, g, buddy_cx, buddy_cy);

    /* Menu (right side, below bars) */
    static const char *menu_labels[] = { "FEED", "PET", "PLAY", "STATS", "OPTIONS" };
    static const uint16_t menu_colors[] = {
        RGB565(0x00, 0xCC, 0x44),   /* green */
        RGB565(0xFF, 0x88, 0xCC),   /* pink */
        RGB565(0x00, 0xCC, 0xFF),   /* cyan */
        RGB565(0xFF, 0xEE, 0x00),   /* yellow */
        RGB565(0xAA, 0x88, 0xFF),   /* purple */
    };
    int menu_x = SCREEN_W - 150;
    int menu_y = 88;
    for (int i = 0; i < MENU_COUNT; i++) {
        int my = menu_y + i * 22;
        if (i == g->menu_cursor) {
            gfx_rect_fill(gfx, menu_x - 6, my - 4, 140, 22, NB_MENU_SEL);
            gfx_rect(gfx, menu_x - 6, my - 4, 140, 22, acc);
            gfx_text(gfx, menu_x, my, ">", acc);
        }
        gfx_text(gfx, menu_x + 20, my, menu_labels[i],
                         i == g->menu_cursor ? menu_colors[i] : NB_DIM_GREEN);
    }

    /* Terminal scroll (bottom) — ticker + credit bar */
    gfx_rect_fill(gfx, 0, SCREEN_H - 20, SCREEN_W, 20, NB_DARK_BG);
    gfx_hline(gfx, 0, SCREEN_H - 20, SCREEN_W, NB_DIM_GREEN);
    {
        const char *tmsg = nb_mood_terminal_msg(g);
        uint16_t tcol = NB_DIM_GREEN;
        if (g->ssid_ticker_sticky > 0 && g->ssid_ticker[0] != '\0') {
            /* Flash between green, cyan, yellow for new SSID discovery */
            int phase = (int)(g->anim_t * 6.0f) % 3;
            static const uint16_t flash[] = { NB_GREEN, NB_CYAN, NB_YELLOW };
            tcol = flash[phase];
        }
        gfx_text_scaled(gfx, 4, SCREEN_H - 14, tmsg, tcol, 1);
    }
    nb_draw_credit(gfx, SCREEN_W - 152, SCREEN_H - 14, g->anim_t, 1);

    /* P12+ : Terminal text corruption */
    if (b->prestige >= 12 && rand() % 4 == 0) {
        int corrupt_x = 4 + rand() % (SCREEN_W - 40);
        char glitch_ch = "!@#$%^&*01"[rand() % 10];
        gfx_char_scaled(gfx, corrupt_x, SCREEN_H - 14, glitch_ch, NB_RED, 1);
    }

    /* Action message overlay */
    if (g->action_timer > 0) {
        int aw = (int)strlen(g->action_msg) * CHAR_W + 24;
        int ax = (SCREEN_W - aw) / 2;
        int ay = SCREEN_H / 2 + 50;
        gfx_rect_fill(gfx, ax, ay, aw, 22, NB_PANEL_BG);
        gfx_rect(gfx, ax, ay, aw, 22, acc);
        gfx_text(gfx, ax + 12, ay + 3, g->action_msg, NB_YELLOW);
    }

    /* Particles */
    for (int i = 0; i < 16; i++) {
        if (g->particles[i].life > 0)
            gfx_rect_fill(gfx, (int)g->particles[i].x, (int)g->particles[i].y,
                           3, 3, g->particles[i].color);
    }

    /* P10+ : CRT glitch interference */
    if (b->prestige >= 10 && rand() % 3 == 0) {
        int glitch_n = 1 + rand() % (b->prestige - 9);
        if (glitch_n > 5) glitch_n = 5;
        for (int gi = 0; gi < glitch_n; gi++) {
            int gy = rand() % SCREEN_H;
            int gw = 30 + rand() % 100;
            int gx = rand() % (SCREEN_W - 20);
            uint16_t glitch_c = (rand() % 3 == 0) ? NB_CYAN :
                                (rand() % 2) ? NB_DIM_GREEN : NB_PURPLE;
            gfx_hline(gfx, gx, gy, gw, glitch_c);
        }
    }

    /* Recon XP splash overlay */
    if (g->recon_splash_timer > 0) {
        int sw = 440, sh = 140;
        int sx = (SCREEN_W - sw) / 2;
        int sy = (SCREEN_H - sh) / 2;
        gfx_rect_fill(gfx, sx, sy, sw, sh, NB_PANEL_BG);
        gfx_rect(gfx, sx, sy, sw, sh, NB_CYAN);
        gfx_rect(gfx, sx + 1, sy + 1, sw - 2, sh - 2, NB_DIM_CYAN);
        gfx_text_centered(gfx, sy + 4, "RECON FEED", NB_CYAN);

        int ly = sy + 24;
        if (g->recon_ssid_delta > 0) {
            gfx_printf_scaled(gfx, sx + 16, ly, NB_GREEN, 1,
                              "+%d SSIDs", g->recon_ssid_delta);
            ly += 14;
        }
        if (g->recon_client_delta > 0) {
            gfx_printf_scaled(gfx, sx + 16, ly, NB_BLUE, 1,
                              "+%d Clients", g->recon_client_delta);
            ly += 14;
        }
        if (g->recon_hs_delta > 0) {
            gfx_printf_scaled(gfx, sx + 16, ly, NB_ORANGE, 1,
                              "+%d Handshakes!", g->recon_hs_delta);
            ly += 14;
        }
        if (g->recon_milestone_xp > 0) {
            gfx_printf_scaled(gfx, sx + 16, ly, NB_YELLOW, 1,
                              "SSID MILESTONE! +%d XP", g->recon_milestone_xp);
            ly += 14;
        }

        /* Total XP in big text */
        gfx_printf_scaled(gfx, sx + 16, ly + 4, NB_GREEN, 2,
                          "Total: +%d XP", g->recon_xp_gained);
        ly += 24;

        if (g->recon_ranks_gained > 0)
            gfx_printf(gfx, sx + 16, ly, NB_YELLOW,
                       "Ranked up %dx!", g->recon_ranks_gained);

        /* Fade hint */
        if (g->recon_splash_timer < 1.5f)
            gfx_text_centered_scaled(gfx, sy + sh - 12, "...", NB_DIM_GREEN, 1);
    }

    /* Prestige popup overlay */
    if (g->prestige_popup_timer > 0) {
        int pw = 440, ph = 60;
        int px = (SCREEN_W - pw) / 2;
        int py = (SCREEN_H - ph) / 2 - 20;
        gfx_rect_fill(gfx, px, py, pw, ph, NB_PANEL_BG);
        gfx_rect(gfx, px, py, pw, ph, NB_YELLOW);
        gfx_rect(gfx, px + 1, py + 1, pw - 2, ph - 2, NB_ORANGE);
        gfx_text_centered(gfx, py + 8, "MAX STAGE REACHED!", NB_YELLOW);
        float blink = sinf(g->anim_t * 6.0f);
        uint16_t msg_col = blink > 0 ? NB_GREEN : NB_CYAN;
        gfx_text_centered(gfx, py + 32, "Go to STATS and PRESTIGE!", msg_col);
    }
}