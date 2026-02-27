/*
 * NETRUNNER 2084 — Cyberpunk RPG for WiFi Pineapple Pager
 *
 * A turn-based RPG using ALL Pager hardware:
 *   - 7 buttons: D-pad (move), A (confirm), B (cancel/inventory), Power (pause)
 *   - D-pad RGB LEDs change by game state (green=safe, red=combat, blue=hacking)
 *   - A/B button LEDs flash on actions
 *   - Buzzer plays combat SFX and level-up jingles
 *   - Vibration motor for damage feedback
 *   - Screen brightness dims during stealth/hacking
 *
 * Build: see games/Makefile for cross-compilation instructions
 */

#define FONT_SCALE 1   /* Keep 8x8 text — RPG needs dense info panels */
#define PAGER_ENGINE_IMPLEMENTATION
#include "../engine/pager_engine.h"
#define PAGER_HW_IMPLEMENTATION
#include "../engine/pager_hw.h"
#include "game_data.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════ */
/*                     SCREEN LAYOUT CONSTANTS                         */
/* ════════════════════════════════════════════════════════════════════ */

#define HUD_H        24
#define MAP_VIEW_Y   HUD_H
#define MAP_VIEW_ROWS 8
#define MAP_VIEW_PX  (MAP_VIEW_ROWS * TILE_SIZE)    /* 160 */
#define MSG_BAR_Y    (MAP_VIEW_Y + MAP_VIEW_PX)     /* 178 */
#define MSG_BAR_H    (SCREEN_H - MSG_BAR_Y)         /* 44 */

/* ════════════════════════════════════════════════════════════════════ */
/*                     CYBERPUNK COLOR PALETTE                         */
/* ════════════════════════════════════════════════════════════════════ */

#define CYB_BG          COLOR_BLACK
#define CYB_GREEN       RGB565(0x00, 0xFF, 0x66)     /* neon green */
#define CYB_BLUE        RGB565(0x00, 0x99, 0xFF)     /* neon blue */
#define CYB_PINK        RGB565(0xFF, 0x00, 0x88)     /* neon pink */
#define CYB_CYAN        RGB565(0x00, 0xFF, 0xDD)     /* neon cyan */
#define CYB_YELLOW      RGB565(0xFF, 0xEE, 0x00)     /* neon yellow */
#define CYB_PURPLE      RGB565(0xBB, 0x00, 0xFF)     /* neon purple */
#define CYB_ORANGE      RGB565(0xFF, 0x88, 0x00)     /* neon orange */
#define CYB_RED         RGB565(0xFF, 0x22, 0x33)     /* neon red */
#define CYB_DIM_GREEN   RGB565(0x00, 0x55, 0x22)
#define CYB_DIM_BLUE    RGB565(0x00, 0x22, 0x55)
#define CYB_DIM_PURPLE  RGB565(0x22, 0x00, 0x44)
#define CYB_DARK_BG     RGB565(0x06, 0x06, 0x12)
#define CYB_HUD_BG      RGB565(0x0C, 0x0C, 0x22)
#define CYB_MSG_BG      RGB565(0x08, 0x0A, 0x18)
#define CYB_SEL_BG      RGB565(0x18, 0x18, 0x38)
#define CYB_HP_BAR      RGB565(0xEE, 0x11, 0x22)
#define CYB_HP_BG       RGB565(0x33, 0x08, 0x0A)
#define CYB_EN_BAR      RGB565(0x22, 0x77, 0xFF)
#define CYB_EN_BG       RGB565(0x08, 0x1A, 0x33)
#define CYB_WALL        RGB565(0x28, 0x28, 0x3A)
#define CYB_FLOOR       RGB565(0x0E, 0x0E, 0x1A)
#define CYB_DOOR        RGB565(0x88, 0x66, 0x22)
#define CYB_HAZARD_1    RGB565(0x66, 0x00, 0x44)
#define CYB_HAZARD_2    RGB565(0x44, 0x00, 0x66)
#define CYB_CHEST       RGB565(0xCC, 0xAA, 0x22)
#define CYB_HEAL        RGB565(0xFF, 0x44, 0x44)
#define CYB_TERMINAL    RGB565(0x00, 0xCC, 0x44)
#define CYB_VENDOR      RGB565(0x44, 0x88, 0xFF)
#define CYB_STAIRS      RGB565(0xCC, 0xCC, 0xCC)
#define CYB_NPC_COL     RGB565(0xFF, 0xCC, 0x44)
#define CYB_EXIT        RGB565(0x44, 0xFF, 0x44)

/* ════════════════════════════════════════════════════════════════════ */
/*                         GAME STATE                                  */
/* ════════════════════════════════════════════════════════════════════ */

typedef enum {
    STATE_TITLE,
    STATE_EXPLORE,
    STATE_COMBAT,
    STATE_INVENTORY,
    STATE_DIALOGUE,
    STATE_SHOP,
    STATE_TERMINAL,
    STATE_PAUSE,
    STATE_GAME_OVER,
    STATE_VICTORY,
} GameState;

typedef struct {
    GameState state;
    Player    player;

    /* Camera (vertical scroll for map) */
    int cam_y;

    /* Messages (bottom bar) */
    char msg[2][MAX_MSG + 1];
    int  msg_timer;          /* frames remaining */

    /* Combat */
    CombatEnemy enemy;
    CombatPhase combat_phase;
    int  combat_cursor;      /* 0=attack 1=abilities 2=items 3=run */
    int  combat_sub;         /* sub-menu cursor */
    int  combat_sub_count;   /* items in sub-menu */
    char combat_msg[MAX_MSG + 1];
    int  combat_msg_timer;
    int  player_turn_first;
    int  enemy_turn_pending;  /* 1 = enemy hasn't acted yet this round */

    /* Dialogue */
    int  dlg_npc_idx;
    int  dlg_line;
    int  dlg_map;

    /* Shop */
    const int *shop_list;
    int  shop_count;
    int  shop_cursor;

    /* Terminal */
    int  term_idx;
    int  term_line;

    /* Inventory */
    int  inv_tab;            /* 0=items 1=equip 2=stats */
    int  inv_cursor;

    /* Pause / Title */
    int  pause_cursor;
    int  title_cursor;

    /* Tracking flags */
    int  terminals_used[16];
    int  npc_gifted[32];
    int  step_count;

    /* Save system */
    int  save_available;     /* cached: save file exists? */

    /* Animation */
    float anim_t;
    int   shake_frames;
    int   flash_color;
    int   flash_frames;
} Game;

/* ════════════════════════════════════════════════════════════════════ */
/*                     FORWARD DECLARATIONS                            */
/* ════════════════════════════════════════════════════════════════════ */

static void set_msg(Game *g, const char *l1, const char *l2);
static void start_combat(Game *g, int tmpl_idx);
static void change_map(Game *g, int idx);
static int  player_has_ability(const Player *p, int ab_id);
static void player_learn_ability(Game *g, int ab_id);
static int  player_has_key(const Player *p, int item_id);
static void player_add_item(Game *g, int item_id);
static int  player_total_atk(const Player *p);
static int  player_total_def(const Player *p);
static void update_hw_leds(const Game *g);
static void sfx_combat_start(void);
static void sfx_hit(void);
static void sfx_player_hit(void);
static void sfx_level_up(void);
static void sfx_victory(void);
static void sfx_death(void);
static void sfx_boss(void);
static void sfx_heal(void);
static void sfx_chest(void);
static void sfx_door(void);
static void sfx_select(void);

/* ════════════════════════════════════════════════════════════════════ */
/*                     SAVE SYSTEM                                     */
/* ════════════════════════════════════════════════════════════════════ */

#define SAVE_MAGIC   0x4E455452  /* "NETR" */
#define SAVE_VERSION 1
#define SAVE_PATH    "/root/games/netrunner/save.dat"

typedef struct {
    uint32_t magic;
    uint32_t version;
    Player   player;
    int      terminals_used[16];
    int      npc_gifted[32];
    int      enemy_active[MAX_MAPS][MAX_ENEMIES_MAP];
    int      step_count;
} SaveData;

static int save_exists(void) {
    FILE *f = fopen(SAVE_PATH, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static int save_game(const Game *g) {
    SaveData sd;
    memset(&sd, 0, sizeof(sd));
    sd.magic   = SAVE_MAGIC;
    sd.version = SAVE_VERSION;
    sd.player  = g->player;
    memcpy(sd.terminals_used, g->terminals_used, sizeof(sd.terminals_used));
    memcpy(sd.npc_gifted, g->npc_gifted, sizeof(sd.npc_gifted));
    sd.step_count = g->step_count;

    /* Capture enemy alive/dead states from all maps */
    for (int m = 0; m < MAX_MAPS; m++)
        for (int e = 0; e < MAX_ENEMIES_MAP; e++)
            sd.enemy_active[m][e] = MAPS[m].enemies[e].active;

    FILE *f = fopen(SAVE_PATH, "wb");
    if (!f) return 0;
    size_t written = fwrite(&sd, sizeof(sd), 1, f);
    fclose(f);
    return written == 1;
}

static int load_game(Game *g) {
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f) return 0;

    SaveData sd;
    size_t n = fread(&sd, sizeof(sd), 1, f);
    fclose(f);

    if (n != 1) return 0;
    if (sd.magic != SAVE_MAGIC || sd.version != SAVE_VERSION) return 0;

    g->player = sd.player;
    memcpy(g->terminals_used, sd.terminals_used, sizeof(g->terminals_used));
    memcpy(g->npc_gifted, sd.npc_gifted, sizeof(g->npc_gifted));
    g->step_count = sd.step_count;

    /* Restore enemy alive/dead states */
    for (int m = 0; m < MAX_MAPS; m++)
        for (int e = 0; e < MAX_ENEMIES_MAP; e++)
            MAPS[m].enemies[e].active = sd.enemy_active[m][e];

    /* Enter exploration at the saved position */
    g->state = STATE_EXPLORE;
    g->cam_y = 0;
    g->msg_timer = 0;
    return 1;
}

static void delete_save(void) {
    remove(SAVE_PATH);
}

static void reset_all_maps(void) {
    for (int m = 0; m < MAX_MAPS; m++)
        for (int e = 0; e < MAPS[m].enemy_count; e++)
            MAPS[m].enemies[e].active = 1;
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     UTILITY HELPERS                                 */
/* ════════════════════════════════════════════════════════════════════ */

static void set_msg(Game *g, const char *l1, const char *l2) {
    if (l1) strncpy(g->msg[0], l1, MAX_MSG);
    else    g->msg[0][0] = '\0';
    if (l2) strncpy(g->msg[1], l2, MAX_MSG);
    else    g->msg[1][0] = '\0';
    g->msg_timer = 90; /* ~3 seconds at 30fps */
}

static int player_has_ability(const Player *p, int ab_id) {
    for (int i = 0; i < p->ability_count; i++)
        if (p->abilities[i] == ab_id) return 1;
    return 0;
}

static void player_learn_ability(Game *g, int ab_id) {
    Player *p = &g->player;
    if (ab_id < 0 || player_has_ability(p, ab_id)) return;
    if (p->ability_count < MAX_ABILITIES) {
        p->abilities[p->ability_count++] = ab_id;
        char buf[MAX_MSG];
        snprintf(buf, MAX_MSG, "Learned: %s!", ALL_ABILITIES[ab_id].name);
        set_msg(g, buf, ALL_ABILITIES[ab_id].desc);
    }
}

static int player_has_key(const Player *p, int item_id) {
    for (int i = 0; i < p->inv_count; i++)
        if (p->inventory[i] == item_id && ALL_ITEMS[item_id].type == ITEM_KEY)
            return 1;
    return 0;
}

static void player_add_item(Game *g, int item_id) {
    Player *p = &g->player;
    if (item_id < 0 || p->inv_count >= MAX_INVENTORY) return;
    p->inventory[p->inv_count++] = item_id;
}

static int player_total_atk(const Player *p) {
    int a = p->atk;
    if (p->weapon >= 0) a += ALL_ITEMS[p->weapon].value;
    if (p->atk_buff_turns > 0) a += 5;
    return a;
}

static int player_total_def(const Player *p) {
    int d = p->def;
    if (p->armor >= 0) d += ALL_ITEMS[p->armor].value;
    if (p->def_buff_turns > 0) d += 5;
    return d;
}

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int rng(int lo, int hi) {
    if (lo >= hi) return lo;
    return lo + (rand() % (hi - lo + 1));
}

/* Get shop items for a given map index */
static const int *shop_for_map(int map, int *out_count) {
    const int *list;
    switch (map) {
        case 0:  list = SHOP_ITEMS_SLUMS;   break;
        case 1:  list = SHOP_ITEMS_MARKET;  break;
        case 2:
        case 3:  list = SHOP_ITEMS_TOWER;   break;
        default: list = SHOP_ITEMS_ENDGAME; break;
    }
    int n = 0;
    while (list[n] != -1) n++;
    *out_count = n;
    return list;
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     SOUND EFFECTS (BUZZER)                          */
/* ════════════════════════════════════════════════════════════════════ */

static void sfx_select(void)       { hw_beep(800, 30); }
static void sfx_hit(void)          { hw_beep(200, 60); hw_vibrate(40); }
static void sfx_player_hit(void)   { hw_beep(150, 80); hw_vibrate(80); }
static void sfx_heal(void)         { hw_beep(600, 50); hw_beep(900, 80); }
static void sfx_chest(void)        { hw_beep(500, 40); hw_beep(700, 40); hw_beep(1000, 60); }
static void sfx_door(void)         { hw_beep(300, 100); }
static void sfx_combat_start(void) { hw_beep(300, 80); hw_beep(200, 80); hw_beep(400, 120); }
static void sfx_boss(void)         { hw_beep(100, 200); hw_beep(80, 200); hw_beep(60, 300); }
static void sfx_death(void)        { hw_death_feedback(); }
static void sfx_level_up(void)     { hw_level_up_feedback(); }

static void sfx_victory(void) {
    hw_beep(523, 100); hw_beep(659, 100); hw_beep(784, 100);
    hw_beep(1047, 200);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     LED STATE MANAGEMENT                            */
/* ════════════════════════════════════════════════════════════════════ */

static void update_hw_leds(const Game *g) {
    switch (g->state) {
        case STATE_EXPLORE:
            hw_led_dpad_all(LED_GREEN);
            hw_led_a_button(20);
            hw_led_b_button(20);
            break;
        case STATE_COMBAT:
            hw_led_dpad_all(LED_RED);
            hw_led_a_button(80);
            hw_led_b_button(40);
            break;
        case STATE_TERMINAL:
            hw_led_dpad_all(LED_BLUE);
            hw_led_a_button(60);
            hw_led_b_button(0);
            break;
        case STATE_SHOP:
        case STATE_DIALOGUE:
            hw_led_dpad_all(LED_CYAN);
            hw_led_a_button(40);
            hw_led_b_button(40);
            break;
        case STATE_INVENTORY:
        case STATE_PAUSE:
            hw_led_dpad_all(LED_PURPLE);
            hw_led_a_button(30);
            hw_led_b_button(30);
            break;
        case STATE_TITLE:
            /* Pulsing handled in render */
            break;
        case STATE_GAME_OVER:
            hw_led_dpad_all(LED_RED);
            hw_led_a_button(0);
            hw_led_b_button(255);
            break;
        case STATE_VICTORY:
            hw_led_dpad_all(LED_YELLOW);
            hw_led_a_button(255);
            hw_led_b_button(0);
            break;
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     MAP TRANSITION                                  */
/* ════════════════════════════════════════════════════════════════════ */

static void change_map(Game *g, int idx) {
    if (idx < 0 || idx >= MAX_MAPS) return;
    g->player.current_map = idx;
    g->player.x = MAPS[idx].player_start_x;
    g->player.y = MAPS[idx].player_start_y;
    g->cam_y = 0;
    g->step_count = 0;

    /* Reset map enemies to alive (for revisiting) */
    /* Actually, keep them dead — we init once at game start */

    char buf[MAX_MSG];
    snprintf(buf, MAX_MSG, "-- %s --", MAPS[idx].name);
    set_msg(g, buf, NULL);
    update_hw_leds(g);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     COMBAT SYSTEM                                   */
/* ════════════════════════════════════════════════════════════════════ */

static void start_combat(Game *g, int tmpl_idx) {
    const EnemyTemplate *t = &ENEMY_TEMPLATES[tmpl_idx];
    CombatEnemy *e = &g->enemy;

    strncpy(e->name, t->name, MAX_NAME);
    e->hp = t->max_hp;
    e->max_hp = t->max_hp;
    e->atk = t->atk;
    e->def = t->def;
    e->speed = t->speed;
    e->exp_reward = t->exp_reward;
    e->credit_reward = t->credit_reward;
    e->loot_item = t->loot_item;
    e->loot_chance = t->loot_chance;
    e->color = t->color;
    e->stunned = 0;
    e->def_debuff_turns = 0;
    e->dot_damage = 0;
    e->dot_turns = 0;
    e->is_boss = (tmpl_idx == ENEMY_BOSS_KIRA || tmpl_idx == ENEMY_BOSS_ARIA);

    g->state = STATE_COMBAT;
    g->combat_phase = COMBAT_MENU;
    g->combat_cursor = 0;
    g->combat_sub = 0;
    g->combat_msg[0] = '\0';
    g->combat_msg_timer = 0;
    g->enemy_turn_pending = 0;

    g->player_turn_first = (g->player.speed >= e->speed);

    /* Reset player combat buffs */
    g->player.atk_buff_turns = 0;
    g->player.def_buff_turns = 0;
    g->player.shield_active = 0;

    if (e->is_boss) {
        sfx_boss();
        hw_vibrate(200);
    } else {
        sfx_combat_start();
    }
    update_hw_leds(g);
}

/* Returns damage dealt */
static int do_player_attack(Game *g, int ability_idx) {
    Player *p = &g->player;
    CombatEnemy *e = &g->enemy;
    const Ability *ab = &ALL_ABILITIES[ability_idx];
    int dmg = 0;

    /* Deduct energy */
    p->energy -= ab->energy_cost;
    if (p->energy < 0) p->energy = 0;

    /* Calculate damage */
    if (ab->base_damage > 0) {
        int atk = player_total_atk(p) + ab->base_damage;
        int def = e->def;
        if (ab->effect == FX_PIERCE || e->def_debuff_turns > 0) def = 0;
        dmg = atk - def / 2 + rng(-2, 2);
        if (dmg < 1) dmg = 1;
        e->hp -= dmg;
        if (e->hp < 0) e->hp = 0;
    }

    /* Apply heal */
    if (ab->heal_amount > 0) {
        int heal = ab->heal_amount;
        if (ab->effect == FX_DRAIN && dmg > 0) {
            heal = dmg / 2;
        }
        p->hp = clamp(p->hp + heal, 0, p->max_hp);
    }

    /* Apply effects */
    switch (ab->effect) {
        case FX_STUN:
            e->stunned = 1;
            break;
        case FX_BUFF_ATK:
            p->atk_buff_turns = 3;
            break;
        case FX_DEBUFF_DEF:
            e->def_debuff_turns = 3;
            break;
        case FX_DOT:
            e->dot_damage = ab->base_damage / 2;
            e->dot_turns = 3;
            break;
        case FX_SHIELD:
            p->shield_active = 1;
            break;
        default:
            break;
    }

    /* Flash A button LED */
    hw_led_a_button(255);

    return dmg;
}

static int do_enemy_attack(Game *g) {
    Player *p = &g->player;
    CombatEnemy *e = &g->enemy;

    if (e->stunned) {
        e->stunned = 0;
        snprintf(g->combat_msg, MAX_MSG, "%s is stunned!", e->name);
        return 0;
    }

    int atk = e->atk + rng(-1, 3);
    int def = player_total_def(p);
    int dmg = atk - def / 2 + rng(-1, 1);
    if (dmg < 1) dmg = 1;

    if (p->shield_active) {
        p->shield_active = 0;
        snprintf(g->combat_msg, MAX_MSG, "Aegis Shield absorbs the hit!");
        return 0;
    }

    p->hp -= dmg;
    if (p->hp < 0) p->hp = 0;

    snprintf(g->combat_msg, MAX_MSG, "%s: %d dmg!", e->name, dmg);
    sfx_player_hit();
    g->shake_frames = 6;

    /* Red screen flash when player takes damage */
    g->flash_color = (int)CYB_RED;
    g->flash_frames = 3;

    /* Flash B button LED (damage indicator) */
    hw_led_b_button(255);

    return dmg;
}

static void process_turn_end(Game *g) {
    CombatEnemy *e = &g->enemy;
    Player *p = &g->player;

    /* DOT damage on enemy */
    if (e->dot_turns > 0) {
        e->hp -= e->dot_damage;
        if (e->hp < 0) e->hp = 0;
        e->dot_turns--;
    }

    /* Buff countdown */
    if (p->atk_buff_turns > 0) p->atk_buff_turns--;
    if (p->def_buff_turns > 0) p->def_buff_turns--;
    if (e->def_debuff_turns > 0) e->def_debuff_turns--;

    /* Check victory */
    if (e->hp <= 0) {
        g->combat_phase = COMBAT_VICTORY;
        return;
    }

    /* Check defeat */
    if (p->hp <= 0) {
        g->combat_phase = COMBAT_DEFEAT;
        return;
    }
}

static void combat_victory(Game *g) {
    Player *p = &g->player;
    CombatEnemy *e = &g->enemy;

    p->exp += e->exp_reward;
    p->credits += e->credit_reward;

    snprintf(g->combat_msg, MAX_MSG, "+%d EXP  +%d Credits", e->exp_reward, e->credit_reward);
    g->combat_msg_timer = 90;

    /* Loot drop */
    if (e->loot_item >= 0 && rng(1, 100) <= e->loot_chance) {
        player_add_item(g, e->loot_item);
    }

    /* Level up check */
    while (p->exp >= p->exp_next) {
        p->exp -= p->exp_next;
        player_level_up(p);
        sfx_level_up();
        hw_vibrate(100);
    }

    sfx_victory();

    /* Mark map enemy as dead */
    MapData *m = &MAPS[p->current_map];
    for (int i = 0; i < m->enemy_count; i++) {
        if (m->enemies[i].active &&
            m->enemies[i].x == p->x && m->enemies[i].y == p->y) {
            m->enemies[i].active = 0;
            break;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     TILE RENDERING                                  */
/* ════════════════════════════════════════════════════════════════════ */

static uint16_t tile_color(char tile, int frame) {
    switch (tile) {
        case TILE_WALL:     return CYB_WALL;
        case TILE_FLOOR:    return CYB_FLOOR;
        case TILE_DOOR:     return CYB_DOOR;
        case TILE_TERMINAL: return (frame & 16) ? CYB_TERMINAL : CYB_DIM_GREEN;
        case TILE_CHEST:    return CYB_CHEST;
        case TILE_STAIRS:   return CYB_STAIRS;
        case TILE_VENDOR:   return (frame & 16) ? CYB_VENDOR : CYB_DIM_BLUE;
        case TILE_HEAL:     return CYB_HEAL;
        case TILE_SPAWN:    return CYB_FLOOR;
        case TILE_HAZARD:   return (frame & 8) ? CYB_HAZARD_1 : CYB_HAZARD_2;
        case TILE_NPC:      return CYB_FLOOR;
        case TILE_BOSS:     return CYB_FLOOR;
        case TILE_EXIT:     return CYB_EXIT;
        default:            return CYB_FLOOR;
    }
}

/* Check if a terminal at map/x/y has been used */
static int is_terminal_used(const Game *g, int map, int tx, int ty) {
    for (int t = 0; t < (int)NUM_TERMINALS; t++) {
        if (TERMINALS[t].map == map && TERMINALS[t].x == tx && TERMINALS[t].y == ty)
            return g->terminals_used[t];
    }
    return 0;
}

static void draw_tile(GfxContext *gfx, int sx, int sy, char tile, int frame) {
    uint16_t col = tile_color(tile, frame);
    gfx_rect_fill(gfx, sx, sy, TILE_SIZE, TILE_SIZE, col);

    /* Draw tile detail */
    switch (tile) {
        case TILE_WALL:
            /* Brick pattern */
            gfx_hline(gfx, sx, sy + TILE_SIZE / 2, TILE_SIZE,
                      RGB565(0x1A, 0x1A, 0x28));
            gfx_vline(gfx, sx + TILE_SIZE / 2, sy, TILE_SIZE / 2,
                      RGB565(0x1A, 0x1A, 0x28));
            gfx_vline(gfx, sx + TILE_SIZE / 4, sy + TILE_SIZE / 2,
                      TILE_SIZE / 2, RGB565(0x1A, 0x1A, 0x28));
            break;
        case TILE_TERMINAL:
            gfx_rect(gfx, sx + 4, sy + 3, 12, 10, CYB_GREEN);
            gfx_text(gfx, sx + 6, sy + 5, ">", CYB_GREEN);
            break;
        case TILE_CHEST:
            gfx_rect(gfx, sx + 4, sy + 6, 12, 8, RGB565(0xAA, 0x88, 0x00));
            gfx_hline(gfx, sx + 4, sy + 9, 12, CYB_YELLOW);
            break;
        case TILE_STAIRS:
            gfx_text(gfx, sx + 6, sy + 6, ">", COLOR_WHITE);
            break;
        case TILE_VENDOR:
            gfx_rect(gfx, sx + 3, sy + 2, 14, 12, CYB_DIM_BLUE);
            gfx_text(gfx, sx + 6, sy + 5, "$", CYB_BLUE);
            break;
        case TILE_HEAL:
            gfx_rect_fill(gfx, sx + 8, sy + 4, 4, 12, CYB_HEAL);
            gfx_rect_fill(gfx, sx + 4, sy + 8, 12, 4, CYB_HEAL);
            break;
        case TILE_HAZARD:
            gfx_text(gfx, sx + 6, sy + 6, "~", CYB_PURPLE);
            break;
        case TILE_EXIT:
            gfx_rect(gfx, sx + 6, sy + 2, 8, 16, CYB_GREEN);
            gfx_text(gfx, sx + 6, sy + 6, "<", CYB_GREEN);
            break;
        default:
            break;
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     HP / ENERGY BARS                                */
/* ════════════════════════════════════════════════════════════════════ */

static void draw_bar(GfxContext *gfx, int x, int y, int w, int h,
                     int cur, int max, uint16_t fg, uint16_t bg) {
    gfx_rect_fill(gfx, x, y, w, h, bg);
    if (max > 0) {
        int fill = (cur * w) / max;
        if (fill > w) fill = w;
        if (fill > 0)
            gfx_rect_fill(gfx, x, y, fill, h, fg);
    }
    gfx_rect(gfx, x, y, w, h, COLOR_DARK_GRAY);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     HUD RENDERING                                   */
/* ════════════════════════════════════════════════════════════════════ */

static void render_hud(GfxContext *gfx, const Game *g) {
    const Player *p = &g->player;

    gfx_rect_fill(gfx, 0, 0, SCREEN_W, HUD_H, CYB_HUD_BG);
    gfx_hline(gfx, 0, HUD_H - 1, SCREEN_W, CYB_DIM_BLUE);

    /* Left: Level + HP/EN bars — scale 2 for labels */
    gfx_printf_scaled(gfx, 4, 4, CYB_GREEN, 2, "L%d", p->level);

    gfx_text(gfx, 44, 2, "HP", CYB_RED);
    draw_bar(gfx, 60, 2, 60, 8, p->hp, p->max_hp, CYB_HP_BAR, CYB_HP_BG);
    gfx_printf(gfx, 62, 2, COLOR_WHITE, "%d", p->hp);

    gfx_text(gfx, 128, 2, "EN", CYB_BLUE);
    draw_bar(gfx, 144, 2, 50, 8, p->energy, p->max_energy, CYB_EN_BAR, CYB_EN_BG);
    gfx_printf(gfx, 146, 2, COLOR_WHITE, "%d", p->energy);

    /* EXP bar — second row */
    gfx_text(gfx, 44, 12, "EXP", CYB_DIM_GREEN);
    draw_bar(gfx, 70, 12, 80, 6, p->exp, p->exp_next, CYB_GREEN, CYB_DARK_BG);

    /* Right: Credits + Map name — scale 2 */
    gfx_printf_scaled(gfx, 206, 4, CYB_YELLOW, 2, "$%d", p->credits);

    const char *map_name = MAPS[p->current_map].name;
    int name_len = strlen(map_name);
    gfx_text_scaled(gfx, SCREEN_W - name_len * 16 - 4, 4, map_name, CYB_CYAN, 2);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     MESSAGE BAR                                     */
/* ════════════════════════════════════════════════════════════════════ */

static void render_msg_bar(GfxContext *gfx, const Game *g) {
    gfx_rect_fill(gfx, 0, MSG_BAR_Y, SCREEN_W, MSG_BAR_H, CYB_MSG_BG);
    gfx_hline(gfx, 0, MSG_BAR_Y, SCREEN_W, CYB_DIM_BLUE);

    if (g->msg_timer > 0 || g->msg[0][0]) {
        gfx_text_centered_scaled(gfx, MSG_BAR_Y + 4, g->msg[0], CYB_CYAN, 2);
        if (g->msg[1][0])
            gfx_text_centered_scaled(gfx, MSG_BAR_Y + 22, g->msg[1], CYB_GREEN, 2);
    } else {
        gfx_text_centered_scaled(gfx, MSG_BAR_Y + 10,
                 "[A] Interact  [B] Inventory", CYB_DIM_GREEN, 2);
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     MAP EXPLORATION RENDERING                       */
/* ════════════════════════════════════════════════════════════════════ */

static void render_explore(GfxContext *gfx, Game *g) {
    const Player *p = &g->player;
    const MapData *m = &MAPS[p->current_map];
    int frame = (int)(g->anim_t * 30);

    /* Update camera to follow player vertically */
    int target_cam = p->y - MAP_VIEW_ROWS / 2;
    if (target_cam < 0) target_cam = 0;
    if (target_cam > MAP_H - MAP_VIEW_ROWS) target_cam = MAP_H - MAP_VIEW_ROWS;
    g->cam_y = target_cam;

    /* Draw tiles */
    for (int row = 0; row < MAP_VIEW_ROWS; row++) {
        int map_row = row + g->cam_y;
        if (map_row < 0 || map_row >= MAP_H) continue;
        for (int col = 0; col < MAP_W; col++) {
            char tile = m->tiles[map_row][col];
            int sx = col * TILE_SIZE;
            int sy = MAP_VIEW_Y + row * TILE_SIZE;

            /* Apply screen shake */
            if (g->shake_frames > 0) {
                sx += rng(-2, 2);
                sy += rng(-2, 2);
            }

            draw_tile(gfx, sx, sy, tile, frame);

            /* Dim used terminals — override blink with static color */
            if (tile == TILE_TERMINAL && is_terminal_used(g, p->current_map, col, map_row)) {
                gfx_rect_fill(gfx, sx, sy, TILE_SIZE, TILE_SIZE, CYB_DIM_GREEN);
                gfx_rect(gfx, sx + 4, sy + 3, 12, 10, CYB_DIM_GREEN);
                gfx_text(gfx, sx + 6, sy + 5, ">", CYB_DIM_GREEN);
            }
        }
    }

    /* Draw enemies on map */
    for (int i = 0; i < m->enemy_count; i++) {
        const MapEnemy *me = &m->enemies[i];
        if (!me->active) continue;
        int ey = me->y - g->cam_y;
        if (ey < 0 || ey >= MAP_VIEW_ROWS) continue;
        int sx = me->x * TILE_SIZE + 4;
        int sy = MAP_VIEW_Y + ey * TILE_SIZE + 4;
        const EnemyTemplate *et = &ENEMY_TEMPLATES[me->enemy_template];
        gfx_rect_fill(gfx, sx, sy, 12, 12, et->color);
        gfx_char(gfx, sx + 2, sy + 2, et->glyph, CYB_BG);
    }

    /* Draw NPCs */
    for (int i = 0; i < m->npc_count; i++) {
        const NPC *npc = &m->npcs[i];
        if (npc->name[0] == '\0') continue;
        int ny = npc->y - g->cam_y;
        if (ny < 0 || ny >= MAP_VIEW_ROWS) continue;
        int sx = npc->x * TILE_SIZE + 2;
        int sy = MAP_VIEW_Y + ny * TILE_SIZE + 2;
        gfx_rect_fill(gfx, sx + 4, sy, 8, 6, CYB_NPC_COL);  /* head */
        gfx_rect_fill(gfx, sx + 2, sy + 6, 12, 8, CYB_NPC_COL); /* body */
        gfx_char(gfx, sx + 4, sy + 4, npc->name[0], CYB_BG);
    }

    /* Draw player */
    {
        int py_screen = p->y - g->cam_y;
        if (py_screen >= 0 && py_screen < MAP_VIEW_ROWS) {
            int sx = p->x * TILE_SIZE;
            int sy = MAP_VIEW_Y + py_screen * TILE_SIZE;
            /* Player glyph: @ in a green square */
            gfx_rect_fill(gfx, sx + 2, sy + 2, 16, 16, CYB_DIM_GREEN);
            gfx_char(gfx, sx + 6, sy + 6, '@', CYB_GREEN);
        }
    }

    /* HUD on top */
    render_hud(gfx, g);

    /* Message bar on bottom */
    render_msg_bar(gfx, g);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     COMBAT RENDERING                                */
/* ════════════════════════════════════════════════════════════════════ */

static void draw_enemy_art(GfxContext *gfx, const CombatEnemy *e, int frame) {
    int cx = 340;  /* Shifted right to make room for stats panel on left */
    int cy = 78;
    uint16_t c = e->color;
    int pulse = (frame & 16) ? 2 : 0;

    if (e->is_boss) {
        /* Larger boss art */
        gfx_rect_fill(gfx, cx - 30 - pulse, cy - 25 - pulse,
                       60 + pulse * 2, 50 + pulse * 2, c);
        gfx_rect_fill(gfx, cx - 20, cy - 15, 40, 30, CYB_BG);
        gfx_rect_fill(gfx, cx - 15, cy - 10, 12, 5, CYB_RED);
        gfx_rect_fill(gfx, cx + 3,  cy - 10, 12, 5, CYB_RED);
        gfx_printf(gfx, cx - 16, cy + 4, c, "%c", e->name[0]);
        /* Decorative lines */
        gfx_line(gfx, cx - 40, cy - 30, cx - 30, cy - 25, c);
        gfx_line(gfx, cx + 40, cy - 30, cx + 30, cy - 25, c);
        gfx_line(gfx, cx - 40, cy + 30, cx - 30, cy + 25, c);
        gfx_line(gfx, cx + 40, cy + 30, cx + 30, cy + 25, c);
    } else {
        /* Normal enemy art */
        gfx_rect_fill(gfx, cx - 18 - pulse, cy - 18 - pulse,
                       36 + pulse * 2, 36 + pulse * 2, c);
        gfx_rect_fill(gfx, cx - 10, cy - 10, 20, 20, CYB_BG);
        gfx_rect_fill(gfx, cx - 6, cy - 6, 5, 3, CYB_RED);
        gfx_rect_fill(gfx, cx + 1, cy - 6, 5, 3, CYB_RED);
        gfx_char(gfx, cx - 4, cy + 2,
                 e->name[0], c);
    }
}

static void render_combat(GfxContext *gfx, Game *g) {
    GfxContext *ctx = gfx;
    CombatEnemy *e = &g->enemy;
    Player *p = &g->player;
    int frame = (int)(g->anim_t * 30);

    gfx_clear(ctx, CYB_DARK_BG);

    /* ── Enemy name + HP bar (top, 34px header) ── */
    gfx_rect_fill(ctx, 0, 0, SCREEN_W, 34, CYB_HUD_BG);
    gfx_text_scaled(ctx, 4, 2, e->name, e->color, 2);
    gfx_printf(ctx, 4, 20, COLOR_WHITE, "HP %d/%d", e->hp, e->max_hp);
    draw_bar(ctx, 100, 20, 200, 8, e->hp, e->max_hp, CYB_HP_BAR, CYB_HP_BG);
    if (e->is_boss)
        gfx_text_scaled(ctx, SCREEN_W - 72, 2, "BOSS", CYB_RED, 2);
    if (e->stunned)
        gfx_text(ctx, SCREEN_W - 64, 22, "STUNNED", CYB_YELLOW);
    if (e->dot_turns > 0)
        gfx_text(ctx, SCREEN_W - 80, 22, "INFECTED", CYB_PURPLE);

    gfx_hline(ctx, 0, 34, SCREEN_W, CYB_DIM_BLUE);

    /* ── Player stats panel (left side of middle zone) ── */
    {
        int px = 8, py = 40;
        /* Vertical separator between stats panel and enemy art */
        gfx_vline(ctx, 220, 35, 87, CYB_DIM_BLUE);

        /* ATK / DEF / SPD readout */
        gfx_text_scaled(ctx, px, py, "ATK", CYB_ORANGE, 2);
        gfx_printf_scaled(ctx, px + 50, py, CYB_GREEN, 2, "%d", p->atk);
        py += 16;
        gfx_text_scaled(ctx, px, py, "DEF", CYB_BLUE, 2);
        gfx_printf_scaled(ctx, px + 50, py, CYB_GREEN, 2, "%d", p->def);
        py += 16;
        gfx_text_scaled(ctx, px, py, "SPD", CYB_CYAN, 2);
        gfx_printf_scaled(ctx, px + 50, py, CYB_GREEN, 2, "%d", p->speed);

        /* Equipped gear */
        py += 20;
        if (p->weapon >= 0)
            gfx_printf(ctx, px, py, CYB_ORANGE, "[%s]", ALL_ITEMS[p->weapon].name);
        py += 10;
        if (p->armor >= 0)
            gfx_printf(ctx, px, py, CYB_BLUE, "[%s]", ALL_ITEMS[p->armor].name);

        /* Active buffs */
        py += 12;
        if (p->atk_buff_turns > 0)
            gfx_text(ctx, px, py, "ATK+", CYB_ORANGE);
        if (p->def_buff_turns > 0)
            gfx_text(ctx, px + (p->atk_buff_turns > 0 ? 40 : 0), py, "DEF+", CYB_BLUE);
        if (p->shield_active)
            gfx_text(ctx, px + (p->atk_buff_turns > 0 ? 40 : 0) + (p->def_buff_turns > 0 ? 40 : 0), py, "SHIELD", CYB_CYAN);
    }

    /* ── Enemy art (right side) ── */
    draw_enemy_art(ctx, e, frame);

    /* ── Divider ── */
    gfx_hline(ctx, 0, 122, SCREEN_W, CYB_DIM_BLUE);

    /* ── Player stats ── */
    gfx_printf_scaled(ctx, 4, 125, CYB_GREEN, 2, "%s  L%d", p->name, p->level);

    /* HP bar — flash when at 20% or below */
    int hp_low = (p->max_hp > 0 && p->hp * 5 <= p->max_hp);
    uint16_t hp_bar_col = (hp_low && (frame & 4)) ? CYB_YELLOW : CYB_HP_BAR;
    uint16_t hp_lbl_col = (hp_low && (frame & 4)) ? CYB_YELLOW : CYB_RED;
    gfx_text(ctx, 4, 144, "HP", hp_lbl_col);
    draw_bar(ctx, 20, 144, 100, 7, p->hp, p->max_hp, hp_bar_col, CYB_HP_BG);
    gfx_printf(ctx, 22, 144, COLOR_WHITE, "%d/%d", p->hp, p->max_hp);

    /* EN bar — flash when at 20% or below */
    int en_low = (p->max_energy > 0 && p->energy * 5 <= p->max_energy);
    uint16_t en_bar_col = (en_low && (frame & 4)) ? CYB_YELLOW : CYB_EN_BAR;
    uint16_t en_lbl_col = (en_low && (frame & 4)) ? CYB_YELLOW : CYB_BLUE;
    gfx_text(ctx, 130, 144, "EN", en_lbl_col);
    draw_bar(ctx, 146, 144, 80, 7, p->energy, p->max_energy, en_bar_col, CYB_EN_BG);
    gfx_printf(ctx, 148, 144, COLOR_WHITE, "%d/%d", p->energy, p->max_energy);

    /* ── Combat menu or sub-menu ── */
    int menu_y = 158;

    if (g->combat_phase == COMBAT_MENU) {
        const char *opts[] = { "Attack", "Abilities", "Items", "Run" };
        /* 2x2 grid: row 0 = Attack,Abilities  row 1 = Items,Run */
        for (int i = 0; i < 4; i++) {
            int col = i % 2;
            int row = i / 2;
            int ox = 8 + col * 200;
            int oy = menu_y + row * 24;
            if (i == g->combat_cursor) {
                gfx_rect_fill(ctx, ox - 2, oy - 1, 190, 20, CYB_SEL_BG);
                gfx_text_scaled(ctx, ox, oy, ">", CYB_GREEN, 2);
            }
            gfx_text_scaled(ctx, ox + 18, oy, opts[i],
                     i == g->combat_cursor ? CYB_GREEN : CYB_DIM_GREEN, 2);
        }
    } else if (g->combat_phase == COMBAT_ABILITY) {
        /* Sub-menu overlays player bar area for more room */
        int sm_y = 124;
        gfx_rect_fill(ctx, 0, 122, SCREEN_W, SCREEN_H - 122, CYB_DARK_BG);
        gfx_hline(ctx, 0, 122, SCREEN_W, CYB_DIM_BLUE);
        gfx_text_scaled(ctx, 4, sm_y, "Abilities [B=Back]:", CYB_CYAN, 2);
        sm_y += 18;
        int ab_count = p->ability_count - 1; /* skip abilities[0] = Attack */
        int sel_ab_id = -1;
        /* Scroll window: keep cursor visible in 3-item window */
        int ab_scroll = 0;
        if (g->combat_sub >= 3) ab_scroll = g->combat_sub - 2;
        if (ab_scroll > ab_count - 3) ab_scroll = ab_count - 3;
        if (ab_scroll < 0) ab_scroll = 0;
        /* Up arrow indicator */
        if (ab_scroll > 0)
            gfx_text_scaled(ctx, SCREEN_W / 2 - 4, sm_y - 2, "^", CYB_DIM_GREEN, 1);
        int ab_vis = 0;
        for (int i = ab_scroll; i < ab_count && ab_vis < 3; i++, ab_vis++) {
            int ab_id = p->abilities[i + 1];  /* +1 to skip basic attack */
            const Ability *ab = &ALL_ABILITIES[ab_id];
            int ox = 8;
            int oy = sm_y + ab_vis * 18;
            int is_sel = (i == g->combat_sub);
            uint16_t col = (p->energy >= ab->energy_cost) ? CYB_GREEN : CYB_DIM_GREEN;
            if (is_sel) {
                gfx_rect_fill(ctx, ox - 2, oy - 1, SCREEN_W - 12, 17, CYB_SEL_BG);
                gfx_text_scaled(ctx, ox, oy, ">", col, 2);
                sel_ab_id = ab_id;
            }
            gfx_printf_scaled(ctx, ox + 18, oy, col, 2, "%s [%dEN]",
                       ab->name, ab->energy_cost);
        }
        /* Down arrow indicator */
        if (ab_scroll + 3 < ab_count)
            gfx_text_scaled(ctx, SCREEN_W / 2 - 4, sm_y + 3 * 18, "v", CYB_DIM_GREEN, 1);
        /* Show selected ability description */
        if (sel_ab_id >= 0) {
            const Ability *sab = &ALL_ABILITIES[sel_ab_id];
            int desc_sc = ((int)strlen(sab->desc) <= 29) ? 2 : 1;
            int desc_y = sm_y + 56;
            gfx_text_scaled(ctx, 8, desc_y, sab->desc, CYB_CYAN, desc_sc);
        }
    } else if (g->combat_phase == COMBAT_ITEM) {
        /* Sub-menu overlays player bar area for more room */
        int sm_y = 124;
        gfx_rect_fill(ctx, 0, 122, SCREEN_W, SCREEN_H - 122, CYB_DARK_BG);
        gfx_hline(ctx, 0, 122, SCREEN_W, CYB_DIM_BLUE);
        gfx_text_scaled(ctx, 4, sm_y, "Items [B=Back]:", CYB_CYAN, 2);
        sm_y += 18;
        /* Build filtered consumable list first */
        int ci_indices[MAX_INVENTORY];
        int ci_count = 0;
        for (int i = 0; i < p->inv_count; i++) {
            if (ALL_ITEMS[p->inventory[i]].type == ITEM_CONSUMABLE)
                ci_indices[ci_count++] = i;
        }
        int sel_item_id = -1;
        /* Scroll window */
        int ci_scroll = 0;
        if (g->combat_sub >= 3) ci_scroll = g->combat_sub - 2;
        if (ci_scroll > ci_count - 3) ci_scroll = ci_count - 3;
        if (ci_scroll < 0) ci_scroll = 0;
        /* Up arrow indicator */
        if (ci_scroll > 0)
            gfx_text_scaled(ctx, SCREEN_W / 2 - 4, sm_y - 2, "^", CYB_DIM_GREEN, 1);
        int ci_vis = 0;
        for (int i = ci_scroll; i < ci_count && ci_vis < 3; i++, ci_vis++) {
            int inv_idx = ci_indices[i];
            int ox = 8;
            int oy = sm_y + ci_vis * 18;
            int is_sel = (i == g->combat_sub);
            uint16_t col = is_sel ? CYB_GREEN : CYB_DIM_GREEN;
            if (is_sel) {
                gfx_rect_fill(ctx, ox - 2, oy - 1, SCREEN_W - 12, 17, CYB_SEL_BG);
                gfx_text_scaled(ctx, ox, oy, ">", col, 2);
                sel_item_id = p->inventory[inv_idx];
            }
            gfx_text_scaled(ctx, ox + 18, oy, ALL_ITEMS[p->inventory[inv_idx]].name, col, 2);
        }
        /* Down arrow indicator */
        if (ci_scroll + 3 < ci_count)
            gfx_text_scaled(ctx, SCREEN_W / 2 - 4, sm_y + 3 * 18, "v", CYB_DIM_GREEN, 1);
        if (ci_count == 0)
            gfx_text_scaled(ctx, 16, sm_y, "No usable items!", CYB_DIM_GREEN, 2);
        /* Show selected item description */
        if (sel_item_id >= 0) {
            const Item *si = &ALL_ITEMS[sel_item_id];
            int desc_sc = ((int)strlen(si->desc) <= 29) ? 2 : 1;
            int desc_y = sm_y + 56;
            gfx_text_scaled(ctx, 8, desc_y, si->desc, CYB_CYAN, desc_sc);
        }
    }

    /* ── Combat result messages ── */
    if (g->combat_phase == COMBAT_RESULT || g->combat_phase == COMBAT_VICTORY ||
        g->combat_phase == COMBAT_DEFEAT) {
        gfx_rect_fill(ctx, 0, 184, SCREEN_W, 38, CYB_MSG_BG);
        gfx_hline(ctx, 0, 184, SCREEN_W, CYB_DIM_BLUE);
        /* Auto-scale: use scale 2 if msg fits (29 chars), else scale 1 */
        int msg_scale = ((int)strlen(g->combat_msg) <= 29) ? 2 : 1;
        int msg_y = (msg_scale == 2) ? 186 : 190;
        gfx_text_scaled(ctx, 4, msg_y, g->combat_msg, CYB_CYAN, msg_scale);

        if (g->combat_phase == COMBAT_VICTORY) {
            gfx_text_scaled(ctx, 4, 204, "[A] Continue", CYB_GREEN, 2);
        } else if (g->combat_phase == COMBAT_DEFEAT) {
            gfx_text_scaled(ctx, 4, 204, "SYSTEM CRASH - [A] Retry", CYB_RED, 2);
        } else {
            gfx_text_scaled(ctx, 4, 204, "[A] Continue", CYB_DIM_GREEN, 2);
        }
    }

    /* ── Message bar (when in menu) ── */
    if (g->combat_phase == COMBAT_MENU && g->combat_msg[0]) {
        gfx_rect_fill(ctx, 0, 184, SCREEN_W, 38, CYB_MSG_BG);
        gfx_hline(ctx, 0, 184, SCREEN_W, CYB_DIM_BLUE);
        int msg_scale = ((int)strlen(g->combat_msg) <= 29) ? 2 : 1;
        int msg_y = (msg_scale == 2) ? 192 : 196;
        gfx_text_scaled(ctx, 4, msg_y, g->combat_msg, CYB_CYAN, msg_scale);
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     INVENTORY RENDERING                             */
/* ════════════════════════════════════════════════════════════════════ */

static void render_inventory(GfxContext *gfx, const Game *g) {
    const Player *p = &g->player;

    gfx_clear(gfx, CYB_DARK_BG);

    /* Tab bar — scale 2 */
    const char *tabs[] = { "ITEMS", "EQUIP", "STATS" };
    for (int i = 0; i < 3; i++) {
        int tx = 4 + i * 120;
        uint16_t bg = (i == g->inv_tab) ? CYB_SEL_BG : CYB_HUD_BG;
        uint16_t fg = (i == g->inv_tab) ? CYB_GREEN : CYB_DIM_GREEN;
        gfx_rect_fill(gfx, tx, 0, 112, 20, bg);
        gfx_text_scaled(gfx, tx + 4, 2, tabs[i], fg, 2);
    }
    gfx_hline(gfx, 0, 21, SCREEN_W, CYB_DIM_BLUE);
    gfx_text(gfx, SCREEN_W - 80, 7, "[B] Close", CYB_DIM_GREEN);

    int y = 26;

    if (g->inv_tab == 0) {
        /* ITEMS tab */
        gfx_text_scaled(gfx, 4, y, "Consumables & Key Items:", CYB_CYAN, 2);
        y += 22;
        if (p->inv_count == 0) {
            gfx_text_scaled(gfx, 16, y, "Empty", CYB_DIM_GREEN, 2);
        } else {
            int vis = 8;
            int scroll = 0;
            if (p->inv_count > vis && g->inv_cursor >= vis) {
                scroll = g->inv_cursor - vis + 1;
                if (scroll > p->inv_count - vis) scroll = p->inv_count - vis;
            }
            if (scroll > 0)
                gfx_text_scaled(gfx, SCREEN_W - 24, y - 2, "^", CYB_DIM_GREEN, 2);
            for (int i = 0; i < vis && (i + scroll) < p->inv_count; i++) {
                int idx = i + scroll;
                int is_sel = (idx == g->inv_cursor);
                if (is_sel) {
                    gfx_rect_fill(gfx, 2, y - 1, SCREEN_W - 4, 18, CYB_SEL_BG);
                    gfx_text_scaled(gfx, 4, y, ">", CYB_GREEN, 2);
                }
                const Item *item = &ALL_ITEMS[p->inventory[idx]];
                uint16_t col = is_sel ? CYB_GREEN : CYB_DIM_GREEN;
                gfx_printf_scaled(gfx, 22, y, col, 2, "%s", item->name);
                if (item->type == ITEM_KEY)
                    gfx_text_scaled(gfx, 300, y, "[KEY]", CYB_YELLOW, 2);
                y += 19;
            }
            if (scroll + vis < p->inv_count)
                gfx_text_scaled(gfx, SCREEN_W - 24, y - 2, "v", CYB_DIM_GREEN, 2);
        }
        /* Description of selected item */
        if (p->inv_count > 0 && g->inv_cursor < p->inv_count) {
            gfx_rect_fill(gfx, 0, 182, SCREEN_W, 40, CYB_MSG_BG);
            gfx_hline(gfx, 0, 182, SCREEN_W, CYB_DIM_BLUE);
            const Item *sel = &ALL_ITEMS[p->inventory[g->inv_cursor]];
            int desc_sc = ((int)strlen(sel->desc) <= 29) ? 2 : 1;
            int desc_y = (desc_sc == 2) ? 184 : 188;
            gfx_text_scaled(gfx, 4, desc_y, sel->desc, CYB_CYAN, desc_sc);
            if (sel->type == ITEM_CONSUMABLE)
                gfx_text_scaled(gfx, 4, 204, "[A] Use", CYB_GREEN, 2);
            else if (sel->type == ITEM_WEAPON || sel->type == ITEM_ARMOR || sel->type == ITEM_IMPLANT)
                gfx_text_scaled(gfx, 4, 204, "[A] Equip", CYB_GREEN, 2);
        }
    } else if (g->inv_tab == 1) {
        /* EQUIP tab */
        gfx_text_scaled(gfx, 4, y, "Equipment:", CYB_CYAN, 2); y += 22;

        gfx_text_scaled(gfx, 4, y, "DECK:", CYB_ORANGE, 2);
        if (p->weapon >= 0)
            gfx_printf_scaled(gfx, 92, y, CYB_GREEN, 2, "%s (ATK +%d)",
                       ALL_ITEMS[p->weapon].name, ALL_ITEMS[p->weapon].value);
        else
            gfx_text_scaled(gfx, 92, y, "None", CYB_DIM_GREEN, 2);
        y += 22;

        gfx_text_scaled(gfx, 4, y, "FIREWALL:", CYB_BLUE, 2);
        if (p->armor >= 0)
            gfx_printf_scaled(gfx, 156, y, CYB_GREEN, 2, "%s (DEF +%d)",
                       ALL_ITEMS[p->armor].name, ALL_ITEMS[p->armor].value);
        else
            gfx_text_scaled(gfx, 156, y, "None", CYB_DIM_GREEN, 2);
        y += 22;

        gfx_text_scaled(gfx, 4, y, "IMPLANT:", CYB_PURPLE, 2);
        if (p->implant >= 0)
            gfx_printf_scaled(gfx, 140, y, CYB_GREEN, 2, "%s",
                       ALL_ITEMS[p->implant].name);
        else
            gfx_text_scaled(gfx, 140, y, "None", CYB_DIM_GREEN, 2);
        y += 28;

        gfx_text_scaled(gfx, 4, y, "Total ATK:", CYB_ORANGE, 2);
        gfx_printf_scaled(gfx, 180, y, CYB_GREEN, 2, "%d", player_total_atk(p));
        y += 22;
        gfx_text_scaled(gfx, 4, y, "Total DEF:", CYB_BLUE, 2);
        gfx_printf_scaled(gfx, 180, y, CYB_GREEN, 2, "%d", player_total_def(p));
    } else {
        /* STATS tab — two columns */
        gfx_text_scaled(gfx, 4, y, "Character Stats:", CYB_CYAN, 2); y += 24;

        /* Left column */
        gfx_printf_scaled(gfx, 4, y, CYB_GREEN, 2, "Level:  %d", p->level);
        gfx_printf_scaled(gfx, 250, y, CYB_GREEN, 2, "EXP: %d/%d", p->exp, p->exp_next);
        y += 22;
        gfx_printf_scaled(gfx, 4, y, CYB_RED, 2, "HP:     %d/%d", p->hp, p->max_hp);
        gfx_printf_scaled(gfx, 250, y, CYB_ORANGE, 2, "ATK: %d(+%d)",
                   p->atk, p->weapon >= 0 ? ALL_ITEMS[p->weapon].value : 0);
        y += 22;
        gfx_printf_scaled(gfx, 4, y, CYB_BLUE, 2, "Energy: %d/%d", p->energy, p->max_energy);
        gfx_printf_scaled(gfx, 250, y, CYB_BLUE, 2, "DEF: %d(+%d)",
                   p->def, p->armor >= 0 ? ALL_ITEMS[p->armor].value : 0);
        y += 22;
        gfx_printf_scaled(gfx, 4, y, CYB_CYAN, 2, "Speed:  %d", p->speed);
        gfx_printf_scaled(gfx, 250, y, CYB_YELLOW, 2, "Credits: %d", p->credits);
        y += 28;

        gfx_text_scaled(gfx, 4, y, "Known Abilities:", CYB_CYAN, 2); y += 22;
        {
            int vis = 3;
            int scroll = g->inv_cursor;
            int max_scroll = p->ability_count > vis ? p->ability_count - vis : 0;
            if (scroll > max_scroll) scroll = max_scroll;
            if (scroll < 0) scroll = 0;
            int ab_y = y;
            for (int i = scroll; i < p->ability_count && i < scroll + vis; i++) {
                gfx_printf_scaled(gfx, 12, ab_y, CYB_GREEN, 2, "- %s",
                           ALL_ABILITIES[p->abilities[i]].name);
                ab_y += 20;
            }
            if (scroll > 0)
                gfx_text(gfx, SCREEN_W - 16, y - 2, "^", CYB_DIM_GREEN);
            if (scroll + vis < p->ability_count)
                gfx_text(gfx, SCREEN_W - 16, ab_y - 6, "v", CYB_DIM_GREEN);
        }
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     DIALOGUE RENDERING                              */
/* ════════════════════════════════════════════════════════════════════ */

static void render_dialogue(GfxContext *gfx, const Game *g) {
    const MapData *m = &MAPS[g->dlg_map];
    if (g->dlg_npc_idx >= m->npc_count) return;
    const NPC *npc = &m->npcs[g->dlg_npc_idx];

    /* Darken map */
    gfx_rect_fill(gfx, 0, 70, SCREEN_W, 152, CYB_DARK_BG);

    /* Dialogue box — taller for bigger text */
    int bx = 10, by = 76, bw = SCREEN_W - 20, bh = 140;
    gfx_rect_fill(gfx, bx, by, bw, bh, CYB_MSG_BG);
    gfx_rect(gfx, bx, by, bw, bh, CYB_CYAN);

    /* NPC name — scale 2 */
    gfx_text_scaled(gfx, bx + 8, by + 6, npc->name, CYB_NPC_COL, 2);
    gfx_hline(gfx, bx + 4, by + 26, bw - 8, CYB_DIM_BLUE);

    /* Dialogue text — scale 2, one line at a time */
    if (g->dlg_line < npc->lines_count) {
        gfx_text_scaled(gfx, bx + 8, by + 34, npc->lines[g->dlg_line], CYB_GREEN, 2);
        if (g->dlg_line + 1 < npc->lines_count)
            gfx_text_scaled(gfx, bx + 8, by + 58, npc->lines[g->dlg_line + 1], CYB_DIM_GREEN, 2);
    }

    /* Prompt — scale 2 */
    if (g->dlg_line + 2 < npc->lines_count)
        gfx_text_scaled(gfx, bx + 8, by + bh - 24, "[A] Next", CYB_DIM_GREEN, 2);
    else
        gfx_text_scaled(gfx, bx + 8, by + bh - 24, "[A] Close", CYB_DIM_GREEN, 2);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     SHOP RENDERING                                  */
/* ════════════════════════════════════════════════════════════════════ */

static void render_shop(GfxContext *gfx, const Game *g) {
    const Player *p = &g->player;

    gfx_clear(gfx, CYB_DARK_BG);

    gfx_rect_fill(gfx, 0, 0, SCREEN_W, 22, CYB_HUD_BG);
    gfx_text_scaled(gfx, 4, 3, "VENDOR TERMINAL", CYB_VENDOR, 2);
    gfx_printf_scaled(gfx, 256, 3, CYB_YELLOW, 2, "$%d", p->credits);
    gfx_hline(gfx, 0, 22, SCREEN_W, CYB_DIM_BLUE);

    int y = 28;
    for (int i = 0; i < g->shop_count && i < 7; i++) {
        int item_id = g->shop_list[i];
        const Item *item = &ALL_ITEMS[item_id];
        int is_sel = (i == g->shop_cursor);
        int can_buy = (p->credits >= item->price && p->inv_count < MAX_INVENTORY);

        if (is_sel) {
            gfx_rect_fill(gfx, 2, y - 1, SCREEN_W - 4, 19, CYB_SEL_BG);
            gfx_text_scaled(gfx, 4, y, ">", CYB_GREEN, 2);
        }

        uint16_t name_col = can_buy ? (is_sel ? CYB_GREEN : CYB_DIM_GREEN) :
                            RGB565(0x55, 0x22, 0x22);
        gfx_printf_scaled(gfx, 22, y, name_col, 2, "%s", item->name);
        gfx_printf_scaled(gfx, 300, y, name_col, 2, "$%d", item->price);

        /* Show type indicator */
        const char *type_label = "";
        uint16_t type_col = CYB_DIM_GREEN;
        switch (item->type) {
            case ITEM_CONSUMABLE: type_label = "USE"; type_col = CYB_GREEN; break;
            case ITEM_WEAPON:     type_label = "DECK"; type_col = CYB_ORANGE; break;
            case ITEM_ARMOR:      type_label = "FW"; type_col = CYB_BLUE; break;
            case ITEM_IMPLANT:    type_label = "IMP"; type_col = CYB_PURPLE; break;
            default: break;
        }
        gfx_text_scaled(gfx, SCREEN_W - 72, y, type_label, type_col, 2);

        y += 22;
    }

    /* Description / help */
    gfx_rect_fill(gfx, 0, 182, SCREEN_W, 40, CYB_MSG_BG);
    gfx_hline(gfx, 0, 182, SCREEN_W, CYB_DIM_BLUE);
    if (g->shop_count > 0) {
        const Item *sel = &ALL_ITEMS[g->shop_list[g->shop_cursor]];
        gfx_text_scaled(gfx, 4, 184, sel->desc, CYB_CYAN, 2);
    }
    gfx_text_scaled(gfx, 4, 204, "[A] Buy  [B] Leave", CYB_DIM_GREEN, 2);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     TERMINAL RENDERING                              */
/* ════════════════════════════════════════════════════════════════════ */

static void render_terminal_screen(GfxContext *gfx, const Game *g) {
    gfx_clear(gfx, CYB_BG);

    /* Scanline effect — drawn FIRST so text renders on top cleanly */
    for (int y = 10; y < SCREEN_H - 10; y += 3) {
        gfx_hline(gfx, 12, y, SCREEN_W - 24, RGB565(0x00, 0x11, 0x00));
    }

    /* Terminal border effect */
    gfx_rect(gfx, 10, 10, SCREEN_W - 20, SCREEN_H - 20, CYB_GREEN);
    gfx_rect(gfx, 12, 12, SCREEN_W - 24, SCREEN_H - 24, CYB_DIM_GREEN);

    gfx_text_scaled(gfx, 20, 16, "==[ TERMINAL ACCESS ]==", CYB_GREEN, 2);
    gfx_hline(gfx, 20, 36, SCREEN_W - 40, CYB_DIM_GREEN);

    if (g->term_idx >= 0 && g->term_idx < (int)NUM_TERMINALS) {
        const TerminalData *td = &TERMINALS[g->term_idx];
        int y = 44;
        for (int i = 0; i <= g->term_line && i < 4; i++) {
            /* Auto-scale: scale 2 if fits (28 chars), else scale 1 */
            int sc = ((int)strlen(td->lines[i]) <= 28) ? 2 : 1;
            int ly = (sc == 2) ? y : y + 4;
            gfx_text_scaled(gfx, 20, ly, td->lines[i], CYB_GREEN, sc);
            y += 22;
        }

        if (g->term_line < 3)
            gfx_text_scaled(gfx, 20, SCREEN_H - 36, "[A] Continue...", CYB_DIM_GREEN, 2);
        else
            gfx_text_scaled(gfx, 20, SCREEN_H - 36, "[A] Disconnect", CYB_DIM_GREEN, 2);
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     TITLE SCREEN                                    */
/* ════════════════════════════════════════════════════════════════════ */

static void render_title(GfxContext *gfx, const Game *g) {
    int frame = (int)(g->anim_t * 30);
    gfx_clear(gfx, CYB_BG);

    /* Decorative lines */
    for (int i = 0; i < 480; i += 20) {
        gfx_vline(gfx, i, 0, SCREEN_H, RGB565(0x04, 0x04, 0x0C));
    }

    /* Title — scale 2 (16px chars) */
    gfx_text_centered_scaled(gfx, 6, "N E T R U N N E R", CYB_GREEN, 2);
    gfx_text_centered_scaled(gfx, 26, "2 0 8 4", CYB_CYAN, 2);

    /* Subtitle */
    gfx_text_centered(gfx, 48, "A Cyberpunk RPG for WiFi Pineapple Pager", CYB_DIM_GREEN);

    /* Decorative divider */
    gfx_hline(gfx, 60, 60, SCREEN_W - 120, CYB_DIM_GREEN);
    gfx_hline(gfx, 80, 62, SCREEN_W - 160, CYB_DIM_BLUE);

    /* Menu — scale 2 (dynamic: 3 options with save, 2 without) */
    int my = 76;
    const char *opts[3];
    int opt_count = 0;
    if (g->save_available) opts[opt_count++] = "CONTINUE";
    opts[opt_count++] = "NEW GAME";
    opts[opt_count++] = "QUIT";

    for (int i = 0; i < opt_count; i++) {
        int is_sel = (i == g->title_cursor);
        int cw2 = 16;  /* scale-2 char width */
        int text_w = (int)strlen(opts[i]) * cw2;
        int ox = SCREEN_W / 2 - text_w / 2 - cw2;  /* room for "> " */
        int oy = my + i * 22;
        if (is_sel) {
            gfx_rect_fill(gfx, ox - 4, oy - 2, text_w + cw2 * 2 + 8, 20, CYB_SEL_BG);
            uint16_t blink_col = (frame & 8) ? CYB_GREEN : CYB_CYAN;
            gfx_text_scaled(gfx, ox, oy, "> ", blink_col, 2);
            gfx_text_scaled(gfx, ox + cw2 * 2, oy, opts[i], blink_col, 2);
        } else {
            gfx_text_scaled(gfx, ox + cw2 * 2, oy, opts[i], CYB_DIM_GREEN, 2);
        }
    }

    /* Controls */
    gfx_text_centered(gfx, 172, "[D-PAD] Move  [A] Select", CYB_DIM_GREEN);

    /* Credit — scale 2 */
    gfx_text_centered_scaled(gfx, 192, "by Hexxed BitHeadz", CYB_DIM_BLUE, 2);

    /* Version */
    gfx_text(gfx, SCREEN_W - 50, SCREEN_H - 10, "v1.0.0", CYB_DIM_GREEN);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     PAUSE SCREEN                                    */
/* ════════════════════════════════════════════════════════════════════ */

static void render_pause(GfxContext *gfx, const Game *g) {
    /* Semi-transparent overlay */
    gfx_rect_fill(gfx, 60, 30, 360, 160, CYB_DARK_BG);
    gfx_rect(gfx, 60, 30, 360, 160, CYB_CYAN);

    gfx_text_centered_scaled(gfx, 42, "== PAUSED ==", CYB_CYAN, 2);

    const char *opts[] = { "Resume", "Heal Station", "Quit to Title" };
    for (int i = 0; i < 3; i++) {
        int oy = 80 + i * 28;
        int is_sel = (i == g->pause_cursor);
        if (is_sel) {
            gfx_text_scaled(gfx, 90, oy, "> ", CYB_GREEN, 2);
            gfx_text_scaled(gfx, 122, oy, opts[i], CYB_GREEN, 2);
        } else {
            gfx_text_scaled(gfx, 122, oy, opts[i], CYB_DIM_GREEN, 2);
        }
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     GAME OVER / VICTORY SCREENS                     */
/* ════════════════════════════════════════════════════════════════════ */

static void render_game_over(GfxContext *gfx, const Game *g) {
    (void)g;
    gfx_clear(gfx, CYB_BG);

    gfx_text_centered_scaled(gfx, 20, "SYSTEM CRASH", CYB_RED, 2);
    gfx_text_centered(gfx, 44, "========================", CYB_DIM_GREEN);

    gfx_text_centered_scaled(gfx, 62, "> FATAL: HP_UNDERFLOW", CYB_RED, 2);
    gfx_text_centered(gfx, 90, "> Neural link severed", CYB_DIM_GREEN);
    gfx_text_centered(gfx, 106, "> Connection terminated", CYB_DIM_GREEN);

    gfx_text_centered_scaled(gfx, 130, "The Net doesn't forgive.", CYB_CYAN, 2);
    gfx_text_centered_scaled(gfx, 166, "[A] Try Again", CYB_GREEN, 2);
    gfx_text_centered_scaled(gfx, 192, "[B] Quit", CYB_DIM_GREEN, 2);
}

static void render_victory_screen(GfxContext *gfx, const Game *g) {
    int frame = (int)(g->anim_t * 30);
    gfx_clear(gfx, CYB_BG);

    uint16_t title_col = (frame & 8) ? CYB_GREEN : CYB_CYAN;
    gfx_text_centered_scaled(gfx, 10, "MISSION COMPLETE", title_col, 2);
    gfx_text_centered(gfx, 34, "================================", CYB_DIM_GREEN);

    gfx_text_centered(gfx, 50, "> ARIA neutralized", CYB_GREEN);
    gfx_text_centered(gfx, 64, "> Megacorp data extracted", CYB_GREEN);
    gfx_text_centered(gfx, 78, "> Evidence uploaded to Net", CYB_GREEN);

    gfx_text_centered_scaled(gfx, 100, "The corporations fell.", CYB_CYAN, 2);
    gfx_text_centered_scaled(gfx, 124, "The Net is free.", CYB_CYAN, 2);
    gfx_text_centered_scaled(gfx, 148, "You are NETRUNNER.", CYB_GREEN, 2);

    gfx_printf(gfx, 120, 174, CYB_YELLOW, "Final Level: %d  Credits: %d",
               g->player.level, g->player.credits);

    gfx_text_centered(gfx, 192, "[A] New Game  [B] Quit", CYB_DIM_GREEN);
    gfx_text_centered(gfx, 208, "Thanks for playing!", CYB_DIM_BLUE);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     INPUT HANDLING                                  */
/* ════════════════════════════════════════════════════════════════════ */

static void handle_title_input(Game *g, const InputContext *inp) {
    int opt_count = g->save_available ? 3 : 2;

    if (input_pressed(inp, BTN_UP) && g->title_cursor > 0) {
        g->title_cursor--;
        sfx_select();
    }
    if (input_pressed(inp, BTN_DOWN) && g->title_cursor < opt_count - 1) {
        g->title_cursor++;
        sfx_select();
    }
    if (input_pressed(inp, BTN_A)) {
        sfx_select();
        /* Map cursor to action: with save 0=Continue 1=New 2=Quit
           without save 0=New 1=Quit  (shift index by 1) */
        int action = g->save_available ? g->title_cursor : g->title_cursor + 1;

        switch (action) {
            case 0: /* Continue saved game */
                if (load_game(g)) {
                    char buf[MAX_MSG];
                    snprintf(buf, MAX_MSG, "-- %s --",
                             MAPS[g->player.current_map].name);
                    set_msg(g, buf, NULL);
                    update_hw_leds(g);
                }
                break;
            case 1: /* New Game */
                delete_save();
                reset_all_maps();
                player_init(&g->player);
                memset(g->terminals_used, 0, sizeof(g->terminals_used));
                memset(g->npc_gifted, 0, sizeof(g->npc_gifted));
                g->state = STATE_EXPLORE;
                change_map(g, 0);
                update_hw_leds(g);
                break;
            case 2: /* Quit — engine will handle in game_update */
                break;
        }
    }
}

static char get_tile_at(const Game *g, int x, int y) {
    const MapData *m = &MAPS[g->player.current_map];
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return TILE_WALL;
    return m->tiles[y][x];
}

static int is_walkable(char tile) {
    return tile != TILE_WALL;
}

static void handle_explore_input(Engine *engine, Game *g) {
    const InputContext *inp = &engine->input;
    Player *p = &g->player;
    const MapData *m = &MAPS[p->current_map];

    /* Decrement message timer */
    if (g->msg_timer > 0) g->msg_timer--;
    if (g->msg_timer == 0 && g->msg[0][0]) {
        g->msg[0][0] = '\0';
        g->msg[1][0] = '\0';
    }
    if (g->shake_frames > 0) g->shake_frames--;

    /* Movement */
    int dx = 0, dy = 0;
    if (input_pressed(inp, BTN_UP))    dy = -1;
    if (input_pressed(inp, BTN_DOWN))  dy = 1;
    if (input_pressed(inp, BTN_LEFT))  dx = -1;
    if (input_pressed(inp, BTN_RIGHT)) dx = 1;

    if (dx != 0 || dy != 0) {
        int nx = p->x + dx;
        int ny = p->y + dy;
        char tile = get_tile_at(g, nx, ny);

        if (is_walkable(tile)) {
            p->x = nx;
            p->y = ny;
            g->step_count++;

            /* Check tile effects */
            switch (tile) {
                case TILE_STAIRS:
                    if (m->next_map >= 0) {
                        /* Check for key requirements */
                        if (m->next_map == 4 && !player_has_key(p, ITEM_SERVER_KEY)) {
                            set_msg(g, "Door locked!", "Need: Server Room Key");
                            p->x -= dx;
                            p->y -= dy;
                        } else if (m->next_map == 5 && !player_has_key(p, ITEM_ROOT_CARD)) {
                            set_msg(g, "Access denied!", "Need: Root Access Card");
                            p->x -= dx;
                            p->y -= dy;
                        } else {
                            change_map(g, m->next_map);
                        }
                    }
                    break;

                case TILE_EXIT:
                    if (m->prev_map >= 0) {
                        change_map(g, m->prev_map);
                    }
                    break;

                case TILE_HEAL:
                    p->hp = p->max_hp;
                    p->energy = p->max_energy;
                    sfx_heal();
                    set_msg(g, "Heal station: Fully restored!", NULL);
                    break;

                case TILE_HAZARD:
                    p->hp -= 3;
                    if (p->hp <= 0) {
                        p->hp = 0;
                        g->state = STATE_GAME_OVER;
                        sfx_death();
                        update_hw_leds(g);
                    } else {
                        hw_vibrate(30);
                        set_msg(g, "EMP field! -3 HP", NULL);
                    }
                    break;

                case TILE_CHEST: {
                    sfx_chest();
                    /* Give random loot based on current map */
                    int loot_table[][3] = {
                        { ITEM_HEALTH_PATCH, ITEM_ENERGY_CELL, ITEM_STIM_PACK },
                        { ITEM_HEALTH_PATCH_P, ITEM_ENERGY_CELL_P, ITEM_MODDED_DECK },
                        { ITEM_HEALTH_PATCH_P, ITEM_FULL_RESTORE, ITEM_MILITARY_DECK },
                        { ITEM_FULL_RESTORE, ITEM_MILITARY_DECK, ITEM_ADAPTIVE_FW },
                        { ITEM_FULL_RESTORE, ITEM_QUANTUM_DECK, ITEM_QUANTUM_FW },
                        { ITEM_FULL_RESTORE, ITEM_VOID_DECK, ITEM_PHANTOM_FW },
                    };
                    int map_idx = clamp(p->current_map, 0, 5);
                    int loot_id = loot_table[map_idx][rng(0, 2)];
                    player_add_item(g, loot_id);
                    char buf[MAX_MSG];
                    snprintf(buf, MAX_MSG, "Found: %s", ALL_ITEMS[loot_id].name);
                    set_msg(g, "Opened data cache!", buf);
                    /* Replace tile with floor so it can't be re-opened */
                    /* (Modifying the tile data — it's not const) */
                    MAPS[p->current_map].tiles[ny][nx] = TILE_FLOOR;
                    break;
                }
                default:
                    break;
            }

            /* Check for NPC collision */
            for (int i = 0; i < m->npc_count; i++) {
                if (m->npcs[i].name[0] &&
                    m->npcs[i].x == nx && m->npcs[i].y == ny) {
                    /* Walk back and start dialogue */
                    p->x -= dx;
                    p->y -= dy;
                    g->state = STATE_DIALOGUE;
                    g->dlg_npc_idx = i;
                    g->dlg_line = 0;
                    g->dlg_map = p->current_map;
                    sfx_select();
                    update_hw_leds(g);
                    return;
                }
            }

            /* Check for map enemy collision */
            for (int i = 0; i < m->enemy_count; i++) {
                if (m->enemies[i].active &&
                    m->enemies[i].x == nx && m->enemies[i].y == ny) {
                    start_combat(g, m->enemies[i].enemy_template);
                    return;
                }
            }

            /* Random encounter check */
            if (g->step_count > 3 && m->random_enemy_count > 0) {
                if (rng(1, 100) <= m->encounter_rate) {
                    int rnd_idx = m->random_enemies[rng(0, m->random_enemy_count - 1)];
                    start_combat(g, rnd_idx);
                    g->step_count = 0;
                    return;
                }
            }
        }
    }

    /* A button — interact with adjacent tiles */
    if (input_pressed(inp, BTN_A)) {
        /* Check facing tile (try all 4 directions for adjacent interactables) */
        int dirs[][2] = {{0,-1},{0,1},{-1,0},{1,0},{0,0}};
        for (int d = 0; d < 5; d++) {
            int tx = p->x + dirs[d][0];
            int ty = p->y + dirs[d][1];
            char tile = get_tile_at(g, tx, ty);

            if (tile == TILE_TERMINAL) {
                /* Find matching terminal */
                for (int t = 0; t < (int)NUM_TERMINALS; t++) {
                    if (TERMINALS[t].map == p->current_map &&
                        TERMINALS[t].x == tx && TERMINALS[t].y == ty) {
                        if (g->terminals_used[t]) {
                            set_msg(g, "> Already extracted data.", NULL);
                        } else {
                            g->state = STATE_TERMINAL;
                            g->term_idx = t;
                            g->term_line = 0;
                            hw_set_brightness(40); /* dim for hacking */
                            update_hw_leds(g);
                        }
                        break;
                    }
                }
                break;
            }
            if (tile == TILE_VENDOR) {
                int cnt;
                const int *items = shop_for_map(p->current_map, &cnt);
                g->state = STATE_SHOP;
                g->shop_list = items;
                g->shop_count = cnt;
                g->shop_cursor = 0;
                sfx_select();
                update_hw_leds(g);
                break;
            }
        }
    }

    /* B button — open inventory */
    if (input_pressed(inp, BTN_B)) {
        g->state = STATE_INVENTORY;
        g->inv_tab = 0;
        g->inv_cursor = 0;
        sfx_select();
        update_hw_leds(g);
    }

    /* Power button — pause */
    if (input_pressed(inp, BTN_POWER)) {
        g->state = STATE_PAUSE;
        g->pause_cursor = 0;
        update_hw_leds(g);
    }
}

static void handle_combat_input(Game *g, const InputContext *inp) {
    Player *p = &g->player;
    CombatEnemy *e = &g->enemy;

    if (g->combat_phase == COMBAT_MENU) {
        /* Navigate 2x2 grid: 0=Attack 1=Abilities 2=Items 3=Run */
        if (input_pressed(inp, BTN_LEFT)) {
            g->combat_cursor ^= 1;  /* toggle column */
            sfx_select();
        }
        if (input_pressed(inp, BTN_RIGHT)) {
            g->combat_cursor ^= 1;  /* toggle column */
            sfx_select();
        }
        if (input_pressed(inp, BTN_UP)) {
            g->combat_cursor ^= 2;  /* toggle row */
            sfx_select();
        }
        if (input_pressed(inp, BTN_DOWN)) {
            g->combat_cursor ^= 2;  /* toggle row */
            sfx_select();
        }

        if (input_pressed(inp, BTN_A)) {
            switch (g->combat_cursor) {
                case 0: { /* Attack (use basic Ping) */
                    int dmg = do_player_attack(g, p->abilities[0]);
                    sfx_hit();
                    snprintf(g->combat_msg, MAX_MSG, "Attack: %d dmg!", dmg);
                    g->combat_phase = COMBAT_RESULT;
                    g->combat_msg_timer = 45;

                    /* Queue enemy turn for after player sees their result */
                    if (e->hp > 0) {
                        g->enemy_turn_pending = 1;
                    } else {
                        process_turn_end(g);
                    }
                    break;
                }
                case 1: /* Abilities sub-menu */
                    g->combat_phase = COMBAT_ABILITY;
                    g->combat_sub = 0;
                    break;
                case 2: /* Items sub-menu */
                    g->combat_phase = COMBAT_ITEM;
                    g->combat_sub = 0;
                    break;
                case 3: { /* Run */
                    int escape = rng(1, 100);
                    if (escape <= 40 + p->speed * 3 && !e->is_boss) {
                        g->combat_phase = COMBAT_RUN;
                        snprintf(g->combat_msg, MAX_MSG, "Escaped!");
                    } else {
                        snprintf(g->combat_msg, MAX_MSG, "Can't escape!");
                        g->combat_phase = COMBAT_RESULT;
                        g->combat_msg_timer = 30;
                        g->enemy_turn_pending = 1;
                    }
                    break;
                }
            }
        }
    } else if (g->combat_phase == COMBAT_ABILITY) {
        int ab_count = p->ability_count - 1; /* skip abilities[0] = Attack */
        if (ab_count < 1) {
            snprintf(g->combat_msg, MAX_MSG, "No abilities learned yet!");
            g->combat_msg_timer = 30;
            g->combat_phase = COMBAT_MENU;
            return;
        }
        if (input_pressed(inp, BTN_UP)) {
            g->combat_sub = (g->combat_sub + ab_count - 1) % ab_count;
            sfx_select();
        }
        if (input_pressed(inp, BTN_DOWN)) {
            g->combat_sub = (g->combat_sub + 1) % ab_count;
            sfx_select();
        }
        if (input_pressed(inp, BTN_B)) {
            g->combat_phase = COMBAT_MENU;
        }
        if (input_pressed(inp, BTN_A)) {
            int ab_id = p->abilities[g->combat_sub + 1]; /* +1 to skip basic attack */
            const Ability *ab = &ALL_ABILITIES[ab_id];
            if (p->energy >= ab->energy_cost) {
                int dmg = do_player_attack(g, ab_id);
                sfx_hit();
                if (ab->base_damage > 0)
                    snprintf(g->combat_msg, MAX_MSG, "%s: %d dmg!", ab->name, dmg);
                else if (ab->heal_amount > 0)
                    snprintf(g->combat_msg, MAX_MSG, "%s: +%d HP!", ab->name, ab->heal_amount);
                else
                    snprintf(g->combat_msg, MAX_MSG, "%s activated!", ab->name);

                g->combat_phase = COMBAT_RESULT;
                g->combat_msg_timer = 45;
                if (e->hp > 0) {
                    g->enemy_turn_pending = 1;
                } else {
                    process_turn_end(g);
                }
            } else {
                snprintf(g->combat_msg, MAX_MSG, "Not enough Energy!");
                g->combat_msg_timer = 30;
            }
        }
    } else if (g->combat_phase == COMBAT_ITEM) {
        /* Count usable items */
        int usable_indices[MAX_INVENTORY];
        int usable_count = 0;
        for (int i = 0; i < p->inv_count; i++) {
            if (ALL_ITEMS[p->inventory[i]].type == ITEM_CONSUMABLE)
                usable_indices[usable_count++] = i;
        }

        if (input_pressed(inp, BTN_UP) && usable_count > 0) {
            g->combat_sub = (g->combat_sub + usable_count - 1) % usable_count;
            sfx_select();
        }
        if (input_pressed(inp, BTN_DOWN) && usable_count > 0) {
            g->combat_sub = (g->combat_sub + 1) % usable_count;
            sfx_select();
        }
        if (input_pressed(inp, BTN_B)) {
            g->combat_phase = COMBAT_MENU;
        }
        if (input_pressed(inp, BTN_A) && usable_count > 0 &&
            g->combat_sub < usable_count) {
            int inv_idx = usable_indices[g->combat_sub];
            int item_id = p->inventory[inv_idx];
            const Item *item = &ALL_ITEMS[item_id];

            /* Apply consumable */
            if (item_id == ITEM_FULL_RESTORE) {
                p->hp = p->max_hp;
                p->energy = p->max_energy;
            } else if (item_id == ITEM_STIM_PACK) {
                p->atk_buff_turns = 3;
                p->def_buff_turns = 3;
            } else if (item->value > 0 && (item_id == ITEM_ENERGY_CELL ||
                       item_id == ITEM_ENERGY_CELL_P)) {
                p->energy = clamp(p->energy + item->value, 0, p->max_energy);
            } else {
                p->hp = clamp(p->hp + item->value, 0, p->max_hp);
            }

            sfx_heal();
            snprintf(g->combat_msg, MAX_MSG, "Used %s!", item->name);

            /* Remove from inventory */
            for (int j = inv_idx; j < p->inv_count - 1; j++)
                p->inventory[j] = p->inventory[j + 1];
            p->inv_count--;

            g->combat_phase = COMBAT_RESULT;
            g->combat_msg_timer = 30;
            g->enemy_turn_pending = 1;
        }
    } else if (g->combat_phase == COMBAT_RESULT) {
        if (g->combat_msg_timer > 0) g->combat_msg_timer--;
        if (g->combat_msg_timer == 0 || input_pressed(inp, BTN_A)) {
            if (g->enemy_turn_pending) {
                /* Now execute enemy's attack and show the result */
                g->enemy_turn_pending = 0;
                do_enemy_attack(g);
                process_turn_end(g);
                /* Stay in COMBAT_RESULT to show enemy's message */
                /* (process_turn_end may have changed phase to VICTORY/DEFEAT) */
                if (g->combat_phase != COMBAT_VICTORY &&
                    g->combat_phase != COMBAT_DEFEAT) {
                    g->combat_phase = COMBAT_RESULT;
                    g->combat_msg_timer = 45;
                }
            } else {
                g->combat_phase = COMBAT_MENU;
                g->combat_msg[0] = '\0';
            }
        }
    } else if (g->combat_phase == COMBAT_VICTORY) {
        if (input_pressed(inp, BTN_A)) {
            combat_victory(g);
            g->state = STATE_EXPLORE;
            update_hw_leds(g);
        }
    } else if (g->combat_phase == COMBAT_DEFEAT) {
        if (input_pressed(inp, BTN_A)) {
            g->state = STATE_GAME_OVER;
            sfx_death();
            update_hw_leds(g);
        }
    } else if (g->combat_phase == COMBAT_RUN) {
        if (input_pressed(inp, BTN_A)) {
            g->state = STATE_EXPLORE;
            update_hw_leds(g);
        }
    }
}

static void handle_inventory_input(Game *g, const InputContext *inp) {
    Player *p = &g->player;

    /* Tab switching */
    if (input_pressed(inp, BTN_LEFT) && g->inv_tab > 0) {
        g->inv_tab--;
        g->inv_cursor = 0;
        sfx_select();
    }
    if (input_pressed(inp, BTN_RIGHT) && g->inv_tab < 2) {
        g->inv_tab++;
        g->inv_cursor = 0;
        sfx_select();
    }

    /* Item navigation */
    if (g->inv_tab == 0) {
        if (input_pressed(inp, BTN_UP) && g->inv_cursor > 0) {
            g->inv_cursor--;
            sfx_select();
        }
        if (input_pressed(inp, BTN_DOWN) && g->inv_cursor < p->inv_count - 1) {
            g->inv_cursor++;
            sfx_select();
        }
        /* Use items */
        if (input_pressed(inp, BTN_A) && p->inv_count > 0 &&
            g->inv_cursor < p->inv_count) {
            int item_id = p->inventory[g->inv_cursor];
            const Item *item = &ALL_ITEMS[item_id];

            if (item->type == ITEM_CONSUMABLE) {
                if (item_id == ITEM_FULL_RESTORE) {
                    p->hp = p->max_hp;
                    p->energy = p->max_energy;
                } else if (item_id == ITEM_STIM_PACK) {
                    p->atk_buff_turns = 3;
                    p->def_buff_turns = 3;
                } else if (item_id == ITEM_ENERGY_CELL || item_id == ITEM_ENERGY_CELL_P) {
                    p->energy = clamp(p->energy + item->value, 0, p->max_energy);
                } else {
                    p->hp = clamp(p->hp + item->value, 0, p->max_hp);
                }
                sfx_heal();
                /* Remove item */
                for (int j = g->inv_cursor; j < p->inv_count - 1; j++)
                    p->inventory[j] = p->inventory[j + 1];
                p->inv_count--;
                if (g->inv_cursor >= p->inv_count && g->inv_cursor > 0)
                    g->inv_cursor--;
            } else if (item->type == ITEM_WEAPON) {
                int old = p->weapon;
                p->weapon = item_id;
                if (old >= 0) {
                    p->inventory[g->inv_cursor] = old; /* swap */
                } else {
                    for (int j = g->inv_cursor; j < p->inv_count - 1; j++)
                        p->inventory[j] = p->inventory[j + 1];
                    p->inv_count--;
                    if (g->inv_cursor >= p->inv_count && g->inv_cursor > 0)
                        g->inv_cursor--;
                }
                sfx_select();
            } else if (item->type == ITEM_ARMOR) {
                int old = p->armor;
                p->armor = item_id;
                if (old >= 0) {
                    p->inventory[g->inv_cursor] = old; /* swap */
                } else {
                    for (int j = g->inv_cursor; j < p->inv_count - 1; j++)
                        p->inventory[j] = p->inventory[j + 1];
                    p->inv_count--;
                    if (g->inv_cursor >= p->inv_count && g->inv_cursor > 0)
                        g->inv_cursor--;
                }
                sfx_select();
            } else if (item->type == ITEM_IMPLANT) {
                int old = p->implant;
                p->implant = item_id;
                if (old >= 0) {
                    p->inventory[g->inv_cursor] = old; /* swap */
                } else {
                    for (int j = g->inv_cursor; j < p->inv_count - 1; j++)
                        p->inventory[j] = p->inventory[j + 1];
                    p->inv_count--;
                    if (g->inv_cursor >= p->inv_count && g->inv_cursor > 0)
                        g->inv_cursor--;
                }
                sfx_select();
            }
        }
    } else if (g->inv_tab == 2) {
        /* Stats tab — scroll abilities list */
        int vis = 3;
        int max_scroll = p->ability_count > vis ? p->ability_count - vis : 0;
        if (input_pressed(inp, BTN_UP) && g->inv_cursor > 0) {
            g->inv_cursor--;
            sfx_select();
        }
        if (input_pressed(inp, BTN_DOWN) && g->inv_cursor < max_scroll) {
            g->inv_cursor++;
            sfx_select();
        }
    }

    /* Close */
    if (input_pressed(inp, BTN_B)) {
        g->state = STATE_EXPLORE;
        update_hw_leds(g);
    }
}

static void handle_dialogue_input(Game *g, const InputContext *inp) {
    const MapData *m = &MAPS[g->dlg_map];
    const NPC *npc = &m->npcs[g->dlg_npc_idx];

    if (input_pressed(inp, BTN_A)) {
        g->dlg_line += 2; /* advance 2 lines at a time */

        if (g->dlg_line >= npc->lines_count) {
            /* End dialogue — give rewards if any */
            int npc_key = g->dlg_map * MAX_NPCS_MAP + g->dlg_npc_idx;

            if (!g->npc_gifted[npc_key]) {
                g->npc_gifted[npc_key] = 1;

                if (npc->required_item >= 0 &&
                    !player_has_key(&g->player, npc->required_item)) {
                    set_msg(g, "Come back when you have", "the required item.");
                    g->npc_gifted[npc_key] = 0; /* Reset so they can try again */
                } else {
                    char line1[MAX_MSG] = "";
                    char line2[MAX_MSG] = "";
                    if (npc->gives_ability >= 0) {
                        player_learn_ability(g, npc->gives_ability);
                        snprintf(line1, MAX_MSG, "Learned: %s", ALL_ABILITIES[npc->gives_ability].name);
                    }
                    if (npc->gives_item >= 0) {
                        player_add_item(g, npc->gives_item);
                        snprintf(line2, MAX_MSG, "Received: %s", ALL_ITEMS[npc->gives_item].name);
                    }
                    /* Show what was given — put ability in line1, item in line2 */
                    if (line1[0] && line2[0])
                        set_msg(g, line1, line2);
                    else if (line1[0])
                        set_msg(g, line1, "");
                    else if (line2[0])
                        set_msg(g, line2, "");
                }
            }

            g->state = STATE_EXPLORE;
            update_hw_leds(g);
        }
    }
    if (input_pressed(inp, BTN_B)) {
        g->state = STATE_EXPLORE;
        update_hw_leds(g);
    }
}

static void handle_shop_input(Game *g, const InputContext *inp) {
    Player *p = &g->player;

    if (input_pressed(inp, BTN_UP) && g->shop_cursor > 0) {
        g->shop_cursor--;
        sfx_select();
    }
    if (input_pressed(inp, BTN_DOWN) && g->shop_cursor < g->shop_count - 1) {
        g->shop_cursor++;
        sfx_select();
    }
    if (input_pressed(inp, BTN_A) && g->shop_count > 0) {
        int item_id = g->shop_list[g->shop_cursor];
        const Item *item = &ALL_ITEMS[item_id];

        /* Prevent buying duplicate implant */
        if (item->type == ITEM_IMPLANT && p->implant == item_id) {
            /* Already installed — do nothing */
        } else if (p->credits >= item->price && p->inv_count < MAX_INVENTORY) {
            p->credits -= item->price;

            if (item->type == ITEM_WEAPON) {
                p->weapon = item_id;
            } else if (item->type == ITEM_ARMOR) {
                p->armor = item_id;
            } else if (item->type == ITEM_IMPLANT) {
                p->implant = item_id;
                /* Apply implant bonuses */
                if (item_id == ITEM_NEURAL_JACK) p->max_energy += item->value;
                if (item_id == ITEM_REFLEX_BOOSTER) p->speed += item->value;
                if (item_id == ITEM_CORTEX_CHIP) p->max_hp += item->value;
            } else {
                player_add_item(g, item_id);
            }
            sfx_chest();
        }
    }
    if (input_pressed(inp, BTN_B)) {
        g->state = STATE_EXPLORE;
        update_hw_leds(g);
    }
}

static void handle_terminal_input(Game *g, const InputContext *inp) {
    if (input_pressed(inp, BTN_A)) {
        g->term_line++;
        if (g->term_line >= 4) {
            /* Terminal complete */
            const TerminalData *td = &TERMINALS[g->term_idx];
            if (!g->terminals_used[g->term_idx]) {
                g->terminals_used[g->term_idx] = 1;
                if (td->teaches_ability >= 0) {
                    player_learn_ability(g, td->teaches_ability);
                }
                /* Special: last terminal teaches 2 abilities */
                if (g->term_idx == (int)NUM_TERMINALS - 1) {
                    player_learn_ability(g, 11); /* Decrypt */
                }
            }
            hw_set_brightness(100); /* restore brightness */
            g->state = STATE_EXPLORE;
            update_hw_leds(g);
        }
        sfx_select();
    }
    if (input_pressed(inp, BTN_B)) {
        hw_set_brightness(100);
        g->state = STATE_EXPLORE;
        update_hw_leds(g);
    }
}

static void handle_pause_input(Engine *engine, Game *g) {
    const InputContext *inp = &engine->input;

    if (input_pressed(inp, BTN_UP) && g->pause_cursor > 0) {
        g->pause_cursor--;
        sfx_select();
    }
    if (input_pressed(inp, BTN_DOWN) && g->pause_cursor < 2) {
        g->pause_cursor++;
        sfx_select();
    }
    if (input_pressed(inp, BTN_A)) {
        switch (g->pause_cursor) {
            case 0: /* Resume */
                g->state = STATE_EXPLORE;
                update_hw_leds(g);
                break;
            case 1: /* Quick heal (if at heal station) */
            {
                MapData *m = &MAPS[g->player.current_map];
                char tile = m->tiles[g->player.y][g->player.x];
                if (tile == TILE_HEAL) {
                    g->player.hp = g->player.max_hp;
                    g->player.energy = g->player.max_energy;
                    sfx_heal();
                    set_msg(g, "Emergency reboot: Restored!", NULL);
                } else {
                    set_msg(g, "No heal station nearby!", NULL);
                }
                g->state = STATE_EXPLORE;
                update_hw_leds(g);
                break;
            }
            case 2: /* Quit to title */
                save_game(g);
                g->state = STATE_TITLE;
                g->title_cursor = 0;
                g->save_available = save_exists();
                hw_led_all_off();
                break;
        }
    }
    if (input_pressed(inp, BTN_B) || input_pressed(inp, BTN_POWER)) {
        g->state = STATE_EXPLORE;
        update_hw_leds(g);
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     ENGINE CALLBACKS                                */
/* ════════════════════════════════════════════════════════════════════ */

static void game_init(Engine *engine, void *userdata) {
    Game *g = (Game *)userdata;
    (void)engine;

    memset(g, 0, sizeof(Game));
    g->state = STATE_TITLE;
    g->title_cursor = 0;
    g->save_available = save_exists();
    srand(time(NULL));

    /* Initial LED effect */
    hw_led_dpad_all(LED_GREEN);
    hw_flash_leds(LED_CYAN, 2, 100, 100);
    hw_set_brightness(100);
}

static void game_update(Engine *engine, float dt, void *userdata) {
    Game *g = (Game *)userdata;
    g->anim_t += dt;

    switch (g->state) {
        case STATE_TITLE:
            handle_title_input(g, &engine->input);
            /* Quit check — last option is always Quit */
            {
                int quit_idx = g->save_available ? 2 : 1;
                if (g->title_cursor == quit_idx &&
                    input_pressed(&engine->input, BTN_A))
                    engine_quit(engine);
            }
            /* Pulsing title LEDs */
            {
                int pulse = (int)(sin(g->anim_t * 3.0) * 127 + 128);
                LedColor c = {0, (uint8_t)pulse, (uint8_t)(pulse / 3)};
                hw_led_dpad_all(c);
            }
            break;
        case STATE_EXPLORE:
            handle_explore_input(engine, g);
            break;
        case STATE_COMBAT:
            handle_combat_input(g, &engine->input);
            break;
        case STATE_INVENTORY:
            handle_inventory_input(g, &engine->input);
            break;
        case STATE_DIALOGUE:
            handle_dialogue_input(g, &engine->input);
            break;
        case STATE_SHOP:
            handle_shop_input(g, &engine->input);
            break;
        case STATE_TERMINAL:
            handle_terminal_input(g, &engine->input);
            break;
        case STATE_PAUSE:
            handle_pause_input(engine, g);
            break;
        case STATE_GAME_OVER:
            if (input_pressed(&engine->input, BTN_A)) {
                /* Restart */
                delete_save();
                reset_all_maps();
                player_init(&g->player);
                memset(g->terminals_used, 0, sizeof(g->terminals_used));
                memset(g->npc_gifted, 0, sizeof(g->npc_gifted));
                g->state = STATE_EXPLORE;
                change_map(g, 0);
                update_hw_leds(g);
            }
            if (input_pressed(&engine->input, BTN_B)) {
                g->state = STATE_TITLE;
                g->title_cursor = 0;
                g->save_available = save_exists();
                hw_led_all_off();
            }
            break;
        case STATE_VICTORY:
            if (input_pressed(&engine->input, BTN_A)) {
                /* New game+ (keep level, reset maps) */
                delete_save();
                g->state = STATE_TITLE;
                g->title_cursor = 0;
                g->save_available = 0;
                hw_led_all_off();
            }
            if (input_pressed(&engine->input, BTN_B)) {
                engine_quit(engine);
            }
            break;
    }

    /* Check for final boss defeat → victory */
    if (g->state == STATE_COMBAT &&
        g->combat_phase == COMBAT_VICTORY &&
        g->enemy.is_boss &&
        g->player.current_map == 5) {
        /* Award rewards before transitioning to victory screen */
        combat_victory(g);
        g->state = STATE_VICTORY;
        sfx_victory();
        hw_play_rtttl("Victory:d=4,o=5,b=200:c,e,g,8c6,4e6,2g6");
        hw_vibrate_pattern("200,100,200,100,500");
        update_hw_leds(g);
    }
}

static void game_render(Engine *engine, void *userdata) {
    Game *g = (Game *)userdata;
    GfxContext *gfx = &engine->gfx;

    switch (g->state) {
        case STATE_TITLE:
            render_title(gfx, g);
            break;
        case STATE_EXPLORE:
            gfx_clear(gfx, CYB_BG);
            render_explore(gfx, g);
            break;
        case STATE_COMBAT:
            render_combat(gfx, g);
            break;
        case STATE_INVENTORY:
            render_inventory(gfx, g);
            break;
        case STATE_DIALOGUE:
            /* Render map underneath, then dialogue box on top */
            gfx_clear(gfx, CYB_BG);
            render_explore(gfx, g);
            render_dialogue(gfx, g);
            break;
        case STATE_SHOP:
            render_shop(gfx, g);
            break;
        case STATE_TERMINAL:
            render_terminal_screen(gfx, g);
            break;
        case STATE_PAUSE:
            /* Render map underneath, then pause overlay */
            gfx_clear(gfx, CYB_BG);
            render_explore(gfx, g);
            render_pause(gfx, g);
            break;
        case STATE_GAME_OVER:
            render_game_over(gfx, g);
            break;
        case STATE_VICTORY:
            render_victory_screen(gfx, g);
            break;
    }

    /* Screen flash effect — subtle red border (top + bottom + sides) */
    if (g->flash_frames > 0) {
        uint16_t fc = (uint16_t)g->flash_color;
        int t = 4;  /* border thickness */
        gfx_rect_fill(gfx, 0, 0, SCREEN_W, t, fc);              /* top */
        gfx_rect_fill(gfx, 0, SCREEN_H - t, SCREEN_W, t, fc);   /* bottom */
        gfx_rect_fill(gfx, 0, 0, t, SCREEN_H, fc);              /* left */
        gfx_rect_fill(gfx, SCREEN_W - t, 0, t, SCREEN_H, fc);   /* right */
        g->flash_frames--;
    }
}

static void game_cleanup(Engine *engine, void *userdata) {
    (void)engine;
    Game *g = (Game *)userdata;

    /* Auto-save if player is in a gameplay state */
    if (g->state == STATE_EXPLORE || g->state == STATE_COMBAT ||
        g->state == STATE_INVENTORY || g->state == STATE_DIALOGUE ||
        g->state == STATE_SHOP || g->state == STATE_TERMINAL ||
        g->state == STATE_PAUSE) {
        save_game(g);
    }

    hw_led_all_off();
    hw_set_brightness(100);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                              MAIN                                   */
/* ════════════════════════════════════════════════════════════════════ */

int main(void) {
    static Game game;
    Engine engine;

    if (engine_create(&engine, game_init, game_update, game_render,
                      game_cleanup, &game) != 0) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    engine.target_fps = 30;
    engine_run(&engine);
    engine_destroy(&engine);

    return 0;
}
