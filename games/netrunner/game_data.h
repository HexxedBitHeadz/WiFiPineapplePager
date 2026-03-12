/*
 * game_data.h — Game Data for NETRUNNER 2084
 * All maps, enemies, items, abilities, NPCs, and dialogue trees.
 *
 * Included by netrunner.c — NOT standalone.
 */

#ifndef GAME_DATA_H
#define GAME_DATA_H

#include <stdint.h>

/* ════════════════════════════════════════════════════════════════════ */
/*                         CONSTANTS                                   */
/* ════════════════════════════════════════════════════════════════════ */

#define MAP_W            24     /* tiles wide */
#define MAP_H            11     /* tiles tall (fits 222px with 20px tiles + HUD) */
#define TILE_SIZE        20
#define HUD_HEIGHT       22     /* top bar */
#define MSG_HEIGHT       44     /* bottom message area (2 lines + border) */

#define MAX_INVENTORY    64
#define MAX_ABILITIES    8
#define MAX_ENEMIES_MAP  6
#define MAX_NPCS_MAP     4
#define MAX_MAPS         6
#define MAX_NAME         24
#define MAX_MSG          60     /* max chars per message line */

/* ════════════════════════════════════════════════════════════════════ */
/*                         TILE TYPES                                  */
/* ════════════════════════════════════════════════════════════════════ */

typedef enum {
    TILE_FLOOR    = '.',   /* walkable */
    TILE_WALL     = '#',   /* solid wall */
    TILE_DOOR     = 'D',   /* door (walkable) */
    TILE_TERMINAL = 'T',   /* hackable terminal (interact with A) */
    TILE_CHEST    = 'C',   /* loot container */
    TILE_STAIRS   = '>',   /* transition to next map */
    TILE_VENDOR   = 'V',   /* shop terminal */
    TILE_HEAL     = '+',   /* healing station */
    TILE_SPAWN    = '@',   /* player start */
    TILE_HAZARD   = '~',   /* damage floor (EMP field) */
    TILE_NPC      = 'N',   /* NPC position marker */
    TILE_BOSS     = 'B',   /* boss encounter trigger */
    TILE_EXIT     = 'X',   /* map exit / return portal */
} TileType;

/* ════════════════════════════════════════════════════════════════════ */
/*                         ABILITIES                                   */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char     name[MAX_NAME];
    int      energy_cost;
    int      base_damage;    /* 0 for non-damage abilities */
    int      heal_amount;    /* 0 for non-heal abilities */
    int      effect;         /* AbilityEffect enum */
    char     desc[MAX_MSG];
} Ability;

typedef enum {
    FX_NONE = 0,
    FX_STUN,         /* skip enemy turn */
    FX_DRAIN,         /* steal HP */
    FX_PIERCE,        /* ignore defense */
    FX_BUFF_ATK,      /* boost attack for 3 turns */
    FX_DEBUFF_DEF,    /* lower enemy defense */
    FX_DOT,           /* damage over time (3 turns) */
    FX_SHIELD,        /* absorb next hit */
} AbilityEffect;

/* Player starts with Ping, learns others via level-up and terminals */
static const Ability ALL_ABILITIES[] = {
    /* name              cost  dmg  heal  effect        description */
    { "Ping",              0,  8,   0,  FX_NONE,       "Basic probe attack" },
    { "Deauth",            6, 12,   0,  FX_STUN,       "Disconnect target for 1 turn" },
    { "Packet Inject",    10, 22,   0,  FX_NONE,       "Inject malicious packets" },
    { "MITM",              8, 10,  10,  FX_DRAIN,      "Intercept data, steal HP" },
    { "Firewall Bypass",  12, 18,   0,  FX_PIERCE,     "Ignore target defenses" },
    { "Buffer Overflow",  14, 28,   0,  FX_NONE,       "Crash enemy process memory" },
    { "Rootkit",          10,  5,   0,  FX_DOT,        "Persistent damage, 3 turns" },
    { "Zero Day",         20, 40,   0,  FX_PIERCE,     "Devastating unknown exploit" },
    { "Patch",             8,  0,  25,  FX_NONE,       "Emergency system repair" },
    { "Aegis Shield",     12,  0,   0,  FX_SHIELD,     "Absorb the next attack" },
    { "Overclock",         6,  0,   0,  FX_BUFF_ATK,   "Boost attack for 3 turns" },
    { "Decrypt",          10, 15,   0,  FX_DEBUFF_DEF, "Strip enemy encryption" },
};

#define NUM_ABILITIES (sizeof(ALL_ABILITIES) / sizeof(ALL_ABILITIES[0]))

/* ════════════════════════════════════════════════════════════════════ */
/*                         ITEMS                                       */
/* ════════════════════════════════════════════════════════════════════ */

typedef enum {
    ITEM_CONSUMABLE,
    ITEM_WEAPON,      /* deck upgrade */
    ITEM_ARMOR,       /* firewall */
    ITEM_IMPLANT,     /* passive bonus */
    ITEM_KEY,         /* quest item */
} ItemType;

typedef struct {
    char     name[MAX_NAME];
    ItemType type;
    int      value;      /* heal amount, ATK bonus, DEF bonus, or key ID */
    int      price;      /* buy price in credits (0 = not sold) */
    char     desc[MAX_MSG];
} Item;

static const Item ALL_ITEMS[] = {
    /* ── Consumables ── */
    { "Health Patch",     ITEM_CONSUMABLE,  20,  15, "Restores 20 HP" },
    { "Health Patch+",    ITEM_CONSUMABLE,  50,  40, "Restores 50 HP" },
    { "Energy Cell",      ITEM_CONSUMABLE,  15,  12, "Restores 15 Energy" },
    { "Energy Cell+",     ITEM_CONSUMABLE,  40,  35, "Restores 40 Energy" },
    { "Full Restore",     ITEM_CONSUMABLE, 999, 100, "Fully restores HP + Energy" },
    { "Stim Pack",        ITEM_CONSUMABLE,   5,  25, "+5 ATK & DEF for 3 turns" },

    /* ── Weapons (Decks) ── */
    { "Basic Deck",       ITEM_WEAPON,       3,   0, "Standard-issue hacking deck" },
    { "Modded Deck",      ITEM_WEAPON,       6,  60, "+6 ATK - Custom firmware" },
    { "Military Deck",    ITEM_WEAPON,      10, 150, "+10 ATK - Mil-spec hardware" },
    { "Quantum Deck",     ITEM_WEAPON,      16, 400, "+16 ATK - Quantum processor" },
    { "Void Deck",        ITEM_WEAPON,      22,   0, "+22 ATK - Legendary prototype" },

    /* ── Armor (Firewalls) ── */
    { "Basic FW",         ITEM_ARMOR,        2,   0, "Standard packet filter" },
    { "Hardened FW",      ITEM_ARMOR,        5,  50, "+5 DEF - Hardened rules" },
    { "Adaptive FW",      ITEM_ARMOR,        9, 130, "+9 DEF - AI-driven filtering" },
    { "Quantum FW",       ITEM_ARMOR,       14, 350, "+14 DEF - Quantum encryption" },
    { "Phantom FW",       ITEM_ARMOR,       20,   0, "+20 DEF - Counterattack layer" },

    /* ── Implants ── */
    { "Neural Jack",      ITEM_IMPLANT,      5,  80, "+5 Max Energy" },
    { "Reflex Booster",   ITEM_IMPLANT,     10, 120, "+10 Speed" },
    { "Cortex Chip",      ITEM_IMPLANT,     15, 200, "+15 Max HP" },

    /* ── Key Items ── */
    { "Server Room Key",  ITEM_KEY,          1,   0, "Access to Corp Tower servers" },
    { "Root Access Card", ITEM_KEY,          2,   0, "Megacorp mainframe access" },
    { "Encrypted Drive",  ITEM_KEY,          3,   0, "Contains the evidence" },
};

#define NUM_ITEMS (sizeof(ALL_ITEMS) / sizeof(ALL_ITEMS[0]))

/* ── Item indices for quick reference ── */
#define ITEM_HEALTH_PATCH    0
#define ITEM_HEALTH_PATCH_P  1
#define ITEM_ENERGY_CELL     2
#define ITEM_ENERGY_CELL_P   3
#define ITEM_FULL_RESTORE    4
#define ITEM_STIM_PACK       5
#define ITEM_BASIC_DECK      6
#define ITEM_MODDED_DECK     7
#define ITEM_MILITARY_DECK   8
#define ITEM_QUANTUM_DECK    9
#define ITEM_VOID_DECK       10
#define ITEM_BASIC_FW        11
#define ITEM_HARDENED_FW     12
#define ITEM_ADAPTIVE_FW     13
#define ITEM_QUANTUM_FW      14
#define ITEM_PHANTOM_FW      15
#define ITEM_NEURAL_JACK     16
#define ITEM_REFLEX_BOOSTER  17
#define ITEM_CORTEX_CHIP     18
#define ITEM_SERVER_KEY      19
#define ITEM_ROOT_CARD       20
#define ITEM_ENCRYPTED_DRIVE 21

/* ════════════════════════════════════════════════════════════════════ */
/*                         ENEMIES                                     */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char    name[MAX_NAME];
    int     max_hp;
    int     atk;
    int     def;
    int     speed;
    int     exp_reward;
    int     credit_reward;
    int     loot_item;      /* index into ALL_ITEMS, -1 = no loot */
    int     loot_chance;    /* percent chance 0-100 */
    uint16_t color;         /* display color */
    char    glyph;          /* map character */
} EnemyTemplate;

static const EnemyTemplate ENEMY_TEMPLATES[] = {
    /* name               HP   ATK DEF SPD  EXP  $   loot             %   color   glyph */
    { "Sentry Bot",       25,   6,  2,  3,  10,  8,  ITEM_HEALTH_PATCH, 40, 0x07E0, 's' },
    { "Firewall Node",    40,   4,  8,  1,  15, 12,  ITEM_ENERGY_CELL,  35, 0x001F, 'f' },
    { "RF Drone",        35,   9,  3,  5,  20, 15,  ITEM_HEALTH_PATCH, 30, 0x07FF, 'd' },
    { "Corp Guard",       50,  11,  6,  4,  30, 25,  ITEM_STIM_PACK,    25, 0xF800, 'g' },
    { "Data Wisp",        20,   7,  1,  8,  12, 20,  ITEM_ENERGY_CELL_P,30, 0xFFE0, 'w' },
    { "Rogue AI",         60,  14,  7,  6,  45, 35,  ITEM_HEALTH_PATCH_P,20, 0xF81F, 'a' },
    { "Crypto Miner",     45,   8, 10,  2,  25, 50,  -1,                 0, 0x8410, 'm' },
    { "Phantom",         100,  18, 12,  5,  80, 60,  ITEM_MODDED_DECK,  15, 0x780F, 'P' },
    /* Bosses */
    { "Sec Chief Kira",  120,  16, 10,  7, 120, 100, ITEM_SERVER_KEY,  100, 0xFD20, 'K' },
    { "Megacorp AI ARIA",200,  24, 15,  8, 300, 500, ITEM_ENCRYPTED_DRIVE,100,0xFFFF,'A' },
};

#define ENEMY_SENTRY       0
#define ENEMY_FIREWALL     1
#define ENEMY_DRONE        2
#define ENEMY_GUARD        3
#define ENEMY_WISP         4
#define ENEMY_ROGUE_AI     5
#define ENEMY_CRYPTO_MINER 6
#define ENEMY_PHANTOM      7
#define ENEMY_BOSS_KIRA    8
#define ENEMY_BOSS_ARIA    9
#define NUM_ENEMIES (sizeof(ENEMY_TEMPLATES) / sizeof(ENEMY_TEMPLATES[0]))

/* ════════════════════════════════════════════════════════════════════ */
/*                         NPC DIALOGUE                                */
/* ════════════════════════════════════════════════════════════════════ */

#define MAX_DIALOGUE_LINES 6

typedef struct {
    char name[MAX_NAME];
    int  x, y;              /* position on map */
    int  lines_count;
    char lines[MAX_DIALOGUE_LINES][MAX_MSG];
    int  gives_ability;      /* ability index, -1 = none */
    int  gives_item;         /* item index, -1 = none */
    int  required_item;      /* required key item to advance, -1 = none */
} NPC;

/* ════════════════════════════════════════════════════════════════════ */
/*                         MAP ENEMY SPAWN                             */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    int enemy_template;     /* index into ENEMY_TEMPLATES */
    int x, y;               /* position */
    int active;             /* 1 = alive, 0 = defeated */
} MapEnemy;

/* ════════════════════════════════════════════════════════════════════ */
/*                         MAP DEFINITION                              */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char       name[MAX_NAME];
    char       tiles[MAP_H][MAP_W + 1]; /* +1 for null terminator */
    int        player_start_x, player_start_y;
    int        next_map;                /* map index when stepping on '>' */
    int        prev_map;                /* map index when stepping on 'X' */
    MapEnemy   enemies[MAX_ENEMIES_MAP];
    int        enemy_count;
    NPC        npcs[MAX_NPCS_MAP];
    int        npc_count;
    uint16_t   floor_color;
    uint16_t   wall_color;
    int        encounter_rate;          /* percent chance per step of random battle */
    int        random_enemies[4];       /* possible random encounter templates */
    int        random_enemy_count;
} MapData;

/* ════════════════════════════════════════════════════════════════════ */
/*                         MAPS                                        */
/* ════════════════════════════════════════════════════════════════════ */

static MapData MAPS[MAX_MAPS] = {
    /* ── MAP 0: Neon Slums (Starting Area) ─────────────────────────── */
    {
        .name = "Neon Slums",
        .tiles = {
            "########################",
            "#@.....#..............>#",
            "#......#...N..........##",
            "#..##..D..........##...#",
            "#..##..#....##....##.N.#",
            "#......#....##.........#",
            "#..+...#...............#",
            "#......D.......T...V...#",
            "#..##..#..............C#",
            "#..##..#...............#",
            "########################",
        },
        .player_start_x = 1, .player_start_y = 1,
        .next_map = 1, .prev_map = -1,
        .enemies = {
            { ENEMY_SENTRY, 15, 3, 1 },
            { ENEMY_SENTRY, 10, 8, 1 },
        },
        .enemy_count = 2,
        .npcs = {
            {
                .name = "Zero Cool",
                .x = 11, .y = 2,
                .lines_count = 4,
                .lines = {
                    "Hey newbie. Welcome to Net.",
                    "Corp towers are east.",
                    "Jack in at terminals to",
                    "learn new exploits.",
                },
                .gives_ability = 1, /* Deauth */
                .gives_item = -1,
                .required_item = -1,
            },
            {
                .name = "Patch",
                .x = 21, .y = 4,
                .lines_count = 3,
                .lines = {
                    "Corp drones are patrolling.",
                    "Be careful in Data Market.",
                    "Take this for the road.",
                },
                .gives_ability = -1,
                .gives_item = ITEM_HEALTH_PATCH,
                .required_item = -1,
            },
        },
        .npc_count = 2,
        .floor_color = 0x0821,  /* very dark blue */
        .wall_color  = 0x4208,  /* dark gray */
        .encounter_rate = 8,
        .random_enemies = { ENEMY_SENTRY, ENEMY_WISP },
        .random_enemy_count = 2,
    },

    /* ── MAP 1: Data Market ────────────────────────────────────────── */
    {
        .name = "Data Market",
        .tiles = {
            "########################",
            "#X....V#......N........#",
            "#......#...............#",
            "#......D...............#",
            "#..##..#...##..##..##..#",
            "#......#...##..##..##..#",
            "#..+...#...............#",
            "#......D..............>#",
            "#..##..#...............#",
            "#..##..#..........N...C#",
            "########################",
        },
        .player_start_x = 1, .player_start_y = 1,
        .next_map = 2, .prev_map = 0,
        .enemies = {
            { ENEMY_DRONE,       12, 2, 1 },
            { ENEMY_FIREWALL,    18, 5, 1 },
            { ENEMY_CRYPTO_MINER, 8, 8, 1 },
        },
        .enemy_count = 3,
        .npcs = {
            {
                .name = "Cipher",
                .x = 13, .y = 1,
                .lines_count = 3,
                .lines = {
                    "Data flows like water here.",
                    "Corp Tower needs a Key.",
                    "Try the terminal nearby.",
                },
                .gives_ability = 3, /* MITM */
                .gives_item = -1,
                .required_item = -1,
            },
            {
                .name = "Glitch",
                .x = 18, .y = 9,
                .lines_count = 4,
                .lines = {
                    "Psst, runner. I got intel.",
                    "Sec Chief Kira guards L3.",
                    "She drops the Server Key.",
                    "You'll need it to go on.",
                },
                .gives_ability = -1,
                .gives_item = -1,
                .required_item = -1,
            },
        },
        .npc_count = 2,
        .floor_color = 0x0011,
        .wall_color  = 0x2104,
        .encounter_rate = 12,
        .random_enemies = { ENEMY_DRONE, ENEMY_FIREWALL, ENEMY_WISP },
        .random_enemy_count = 3,
    },

    /* ── MAP 2: Corporate Tower - Lower ─────────────────────────────── */
    {
        .name = "Corp Tower Lo",
        .tiles = {
            "########################",
            "#X.............T......>#",
            "#..##..##..............#",
            "#..##..##..##....##....#",
            "#..........##....##....#",
            "#....~~~~~.............#",
            "#....~~~~~....##..##...#",
            "#.............##..##...#",
            "#..T...................#",
            "#..V...........+......C#",
            "########################",
        },
        .player_start_x = 1, .player_start_y = 1,
        .next_map = 3, .prev_map = 1,
        .enemies = {
            { ENEMY_GUARD,    10, 2, 1 },
            { ENEMY_DRONE,    15, 4, 1 },
            { ENEMY_GUARD,    13, 7, 1 },
            { ENEMY_ROGUE_AI,  5, 8, 1 },
        },
        .enemy_count = 4,
        .npcs = {{ 0 }},
        .npc_count = 0,
        .floor_color = 0x1082,
        .wall_color  = 0x3186,
        .encounter_rate = 18,
        .random_enemies = { ENEMY_GUARD, ENEMY_DRONE, ENEMY_ROGUE_AI },
        .random_enemy_count = 3,
    },

    /* ── MAP 3: Corporate Tower - Upper (Boss: Kira) ────────────────── */
    {
        .name = "Corp Tower Hi",
        .tiles = {
            "########################",
            "#X..T..................#",
            "#......##..##..........#",
            "#......##..##....##....#",
            "#..................##..#",
            "#..~~..............##..#",
            "#..~~....##............#",
            "#........##......B....>#",
            "#...........+.........#",
            "#..C...................#",
            "########################",
        },
        .player_start_x = 1, .player_start_y = 1,
        .next_map = 4, .prev_map = 2,
        .enemies = {
            { ENEMY_GUARD,     13, 3, 1 },
            { ENEMY_ROGUE_AI,  17, 5, 1 },
            { ENEMY_PHANTOM,   8, 6, 1 },
            { ENEMY_BOSS_KIRA, 17, 7, 1 },  /* Boss! */
        },
        .enemy_count = 4,
        .npcs = {{ 0 }},
        .npc_count = 0,
        .floor_color = 0x1082,
        .wall_color  = 0x528A,
        .encounter_rate = 20,
        .random_enemies = { ENEMY_GUARD, ENEMY_ROGUE_AI, ENEMY_PHANTOM },
        .random_enemy_count = 3,
    },

    /* ── MAP 4: Server Farm ────────────────────────────────────────── */
    {
        .name = "Server Farm",
        .tiles = {
            "########################",
            "#X.....T...............#",
            "#..##..##..##..##..##..#",
            "#..##..##..##..##..##..#",
            "#......................#",
            "#..##..##..##..##..##..#",
            "#..##..##..##..##..##..#",
            "#................N....>#",
            "#..+..........V........#",
            "#..C...................#",
            "########################",
        },
        .player_start_x = 1, .player_start_y = 1,
        .next_map = 5, .prev_map = 3,
        .enemies = {
            { ENEMY_PHANTOM, 10, 4, 1 },
            { ENEMY_ROGUE_AI,  16, 4, 1 },
            { ENEMY_PHANTOM, 12, 8, 1 },
        },
        .enemy_count = 3,
        .npcs = {
            {
                .name = "Ghost",
                .x = 17, .y = 7,
                .lines_count = 4,
                .lines = {
                    "You made it this far...",
                    "ARIA, the Megacorp AI,",
                    "guards the mainframe.",
                    "Take this. You'll need it.",
                },
                .gives_ability = 7, /* Zero Day */
                .gives_item = ITEM_ROOT_CARD,
                .required_item = ITEM_SERVER_KEY,
            },
        },
        .npc_count = 1,
        .floor_color = 0x0841,
        .wall_color  = 0x2945,
        .encounter_rate = 22,
        .random_enemies = { ENEMY_PHANTOM, ENEMY_ROGUE_AI, ENEMY_GUARD },
        .random_enemy_count = 3,
    },

    /* ── MAP 5: Mainframe Core (Final Boss: ARIA) ──────────────────── */
    {
        .name = "Mainframe",
        .tiles = {
            "########################",
            "#X.........T...........#",
            "#..........##..........#",
            "#.....##..####..##.....#",
            "#.....##...##...##.....#",
            "#..........##..........#",
            "#.....##..####..##.....#",
            "#.....##...##...##.....#",
            "#..+.......##.........C#",
            "#..........B...........#",
            "########################",
        },
        .player_start_x = 1, .player_start_y = 1,
        .next_map = -1, .prev_map = 4,  /* no next map — game ends */
        .enemies = {
            { ENEMY_PHANTOM,   5, 4, 1 },
            { ENEMY_PHANTOM,  18, 4, 1 },
            { ENEMY_ROGUE_AI,    5, 7, 1 },
            { ENEMY_ROGUE_AI,   18, 7, 1 },
            { ENEMY_BOSS_ARIA,  11, 9, 1 },  /* Final Boss! */
        },
        .enemy_count = 5,
        .npcs = {{ 0 }},
        .npc_count = 0,
        .floor_color = 0x0000,
        .wall_color  = 0x780F,  /* purple walls */
        .encounter_rate = 25,
        .random_enemies = { ENEMY_PHANTOM, ENEMY_ROGUE_AI },
        .random_enemy_count = 2,
    },
};

/* ════════════════════════════════════════════════════════════════════ */
/*                         PLAYER STATE                                */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char name[MAX_NAME];
    int  level;
    int  exp;
    int  exp_next;     /* EXP needed for next level */

    int  hp, max_hp;
    int  energy, max_energy;
    int  atk, def, speed;
    int  credits;

    /* Equipment (indices into ALL_ITEMS, -1 = none) */
    int  weapon;       /* deck */
    int  armor;        /* firewall */
    int  implant;

    /* Abilities known */
    int  abilities[MAX_ABILITIES];
    int  ability_count;

    /* Inventory */
    int  inventory[MAX_INVENTORY];  /* item indices */
    int  inv_count;

    /* Position */
    int  x, y;
    int  current_map;

    /* Combat buffs (turns remaining) */
    int  atk_buff_turns;
    int  def_buff_turns;
    int  shield_active;
    int  dot_damage;
    int  dot_turns;

    /* Flags */
    int  has_item[32];   /* simple bitfield for key items */
} Player;

/* ── Level-up stat growth ── */

static void player_level_up(Player *p) {
    p->level++;
    p->max_hp += 8 + p->level;
    p->max_energy += 4 + p->level / 2;
    p->atk += 2;
    p->def += 1;
    p->speed += 1;
    p->hp = p->max_hp;
    p->energy = p->max_energy;
    p->exp_next = p->level * p->level * 25 + 50;
}

static void player_init(Player *p) {
    memset(p, 0, sizeof(Player));
    snprintf(p->name, MAX_NAME, "Runner");
    p->level = 1;
    p->exp = 0;
    p->exp_next = 75;

    p->max_hp = 40;
    p->hp = 40;
    p->max_energy = 20;
    p->energy = 20;
    p->atk = 8;
    p->def = 4;
    p->speed = 5;
    p->credits = 30;

    p->weapon = ITEM_BASIC_DECK;
    p->armor  = ITEM_BASIC_FW;
    p->implant = -1;

    /* Start with Ping */
    p->abilities[0] = 0; /* Ping */
    p->ability_count = 1;

    /* Starting items */
    p->inventory[0] = ITEM_HEALTH_PATCH;
    p->inventory[1] = ITEM_ENERGY_CELL;
    p->inv_count = 2;

    p->current_map = 0;
    p->x = MAPS[0].player_start_x;
    p->y = MAPS[0].player_start_y;
}

/* ════════════════════════════════════════════════════════════════════ */
/*                         COMBAT STATE                                */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Enemy copy (from template) */
    char name[MAX_NAME];
    int  hp, max_hp;
    int  atk, def, speed;
    int  exp_reward, credit_reward;
    int  loot_item, loot_chance;
    uint16_t color;
    int  is_boss;

    /* Enemy buffs */
    int  stunned;
    int  def_debuff_turns;
    int  dot_damage;
    int  dot_turns;
} CombatEnemy;

typedef enum {
    COMBAT_MENU,       /* selecting action */
    COMBAT_ABILITY,    /* selecting ability */
    COMBAT_ITEM,       /* selecting item */
    COMBAT_ANIMATE,    /* playing attack animation */
    COMBAT_RESULT,     /* showing result text, waiting for button */
    COMBAT_VICTORY,    /* won the fight */
    COMBAT_DEFEAT,     /* lost the fight */
    COMBAT_RUN,        /* escaped */
} CombatPhase;

/* ════════════════════════════════════════════════════════════════════ */
/*                         SHOP DATA                                   */
/* ════════════════════════════════════════════════════════════════════ */

/* Shop inventory — different vendors sell different items based on map */
static const int SHOP_ITEMS_SLUMS[] = {
    ITEM_HEALTH_PATCH, ITEM_ENERGY_CELL, ITEM_STIM_PACK, -1
};

static const int SHOP_ITEMS_MARKET[] = {
    ITEM_HEALTH_PATCH, ITEM_HEALTH_PATCH_P, ITEM_ENERGY_CELL,
    ITEM_ENERGY_CELL_P, ITEM_MODDED_DECK, ITEM_HARDENED_FW,
    ITEM_NEURAL_JACK, -1
};

static const int SHOP_ITEMS_TOWER[] = {
    ITEM_HEALTH_PATCH_P, ITEM_ENERGY_CELL_P, ITEM_FULL_RESTORE,
    ITEM_MILITARY_DECK, ITEM_ADAPTIVE_FW, ITEM_REFLEX_BOOSTER,
    ITEM_CORTEX_CHIP, -1
};

static const int SHOP_ITEMS_ENDGAME[] = {
    ITEM_FULL_RESTORE, ITEM_QUANTUM_DECK, ITEM_QUANTUM_FW, -1
};

/* ════════════════════════════════════════════════════════════════════ */
/*                      TERMINAL MESSAGES                              */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    int  map;        /* which map this terminal belongs to */
    int  x, y;       /* position on map */
    char lines[4][MAX_MSG];
    int  teaches_ability;  /* ability index, -1 = none */
} TerminalData;

static const TerminalData TERMINALS[] = {
    {
        0, 15, 7,  /* Neon Slums */
        {
            "> ACCESSING LOCAL NODE...",
            "> Packet Injection found.",
            "> Downloading exploit...",
            "> LEARNED: Packet Inject",
        },
        2, /* Packet Inject */
    },
    {
        1, 6, 1,   /* Data Market */
        {
            "> DECRYPTING DATA CACHE...",
            "> Firewall Bypass technique",
            "> extracted from stolen DB.",
            "> LEARNED: FW Bypass",
        },
        4, /* Firewall Bypass */
    },
    {
        2, 15, 1,  /* Corp Tower Lower */
        {
            "> CORP NETWORK ACCESSED...",
            "> Rootkit deployment module",
            "> loaded from corp node.",
            "> LEARNED: Rootkit",
        },
        6, /* Rootkit */
    },
    {
        2, 3, 8,   /* Corp Tower Lower — second terminal */
        {
            "> MEDICAL SUBROUTINE FOUND",
            "> Emergency patch protocol",
            "> downloaded to your deck.",
            "> LEARNED: Patch",
        },
        8, /* Patch */
    },
    {
        3, 4, 1,   /* Corp Tower Upper */
        {
            "> SEC OVERRIDE DETECTED...",
            "> Buffer Overflow exploit",
            "> compiled and ready.",
            "> LEARNED: Buffer Overflow",
        },
        5, /* Buffer Overflow */
    },
    {
        4, 7, 1,   /* Server Farm */
        {
            "> DEFENSIVE PROTOCOL FOUND",
            "> Aegis Shield loaded.",
            "> Absorbs next attack.",
            "> LEARNED: Aegis Shield",
        },
        9, /* Aegis Shield */
    },
    {
        5, 11, 1,  /* Mainframe Core */
        {
            "> FINAL PROTOCOL UNLOCKED",
            "> Overclock: boost systems",
            "> Decrypt: strip armor",
            "> 2 SKILLS LEARNED!",
        },
        10, /* Overclock (Decrypt given as bonus) */
    },
};

#define NUM_TERMINALS (sizeof(TERMINALS) / sizeof(TERMINALS[0]))

#endif /* GAME_DATA_H */
