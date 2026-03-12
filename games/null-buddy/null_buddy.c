/*
 * null_buddy.c — NULL-BUDDY: Cyber-Tamagotchi for WiFi Pineapple Pager
 *
 * A virtual pet that lives on your Pager! Feed it packets, pet its
 * antenna, play a mini-game to keep it happy. Stats decay in real-time,
 * evolve through 4 stages by earning XP. State is saved between sessions.
 *
 * Controls:
 *   D-pad Up/Down  — navigate menu / mini-game
 *   D-pad Left/Right — mini-game movement
 *   A (green)      — confirm / action
 *   B (red, hold)  — quit
 *
 * Build: see games/Makefile
 */

#define FONT_SCALE 2
#define PAGER_ENGINE_IMPLEMENTATION
#include "../engine/pager_engine.h"
#define PAGER_HW_IMPLEMENTATION
#include "../engine/pager_hw.h"
#include "game_data.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════ */
/*                     GAME STATES                                     */
/* ════════════════════════════════════════════════════════════════════ */

typedef enum {
    NB_STATE_TITLE,
    NB_STATE_NAMING,
    NB_STATE_HOME,
    NB_STATE_FEED,
    NB_STATE_PET,
    NB_STATE_PLAY,      /* mini-game: catch falling packets */
    NB_STATE_STATS,
    NB_STATE_SLEEPING,
    NB_STATE_CONFIRM_RESET,
    NB_STATE_CONFIRM_NAME,
    NB_STATE_TUTORIAL,
    NB_STATE_OPTIONS,
} NBState;

#define NB_TUTORIAL_PAGES 7

/* ════════════════════════════════════════════════════════════════════ */
/*                     MINI-GAME PACKET                                */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    float x, y;
    float speed;
    int   active;
    int   good;     /* 1 = feed packet, 0 = malware */
} MGPacket;

/* ════════════════════════════════════════════════════════════════════ */
/*                     GAME CONTEXT                                    */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    NBState    state;
    BuddyStats buddy;

    /* Timing */
    double     last_tick;     /* clock for stat decay */
    float      hunger_acc;
    float      happy_acc;
    float      energy_acc;
    float      xp_acc;
    float      age_acc;

    /* Home screen */
    int        menu_cursor;   /* 0=Feed 1=Pet 2=Play 3=Stats 4=Options */
    #define    MENU_COUNT 5

    /* Options submenu */
    int        options_cursor; /* 0=Recon 1=Reset 2=Tutorial (3=DevMode if enabled) */
    #define    OPTIONS_COUNT 5 /* Uncomment for dev mode */
    
    /* #define    OPTIONS_COUNT 3 /*        /* Comment out for dev/demo mode */

    /* Dev mode — lowers XP thresholds (remove later) */
    int        dev_mode;

    /* Demo mode — auto-levels every 3 seconds, auto-prestiges */
    int        demo_mode;
    float      demo_timer;

    /* Confirm reset */
    int        confirm_cursor; /* 0=No 1=Yes */

    /* Terminal messages scrolling */
    int        term_idx;
    float      term_timer;

    /* Action feedback */
    char       action_msg[48];
    float      action_timer;

    /* Feed cooldown */
    float      feed_cd;
    float      pet_cd;

    /* Mini-game state */
    float      mg_player_x;
    MGPacket   mg_packets[MG_MAX_PACKETS];
    int        mg_score;
    int        mg_misses;
    float      mg_timer;       /* countdown */
    float      mg_spawn_timer;

    /* Naming screen */
    int        name_cursor;    /* character position */
    int        name_char;      /* current char index in alphabet */

    /* Tutorial */
    int        tutorial_page;

    /* Animation */
    float      anim_t;
    float      bounce_t;

    /* Idle wandering */
    float      wander_x;       /* offset from center (-40..+40) */
    float      wander_vx;      /* current drift velocity */
    float      wander_timer;   /* time until next direction change */
    int        wander_facing;  /* -1=left, 0=center, 1=right */

    /* Idle fidgets */
    int        fidget_type;    /* 0=none, 1=look-around, 2=shake, 3=head-tilt */
    float      fidget_timer;   /* countdown for current fidget */
    float      fidget_cd;      /* cooldown until next fidget */

    /* Chat bubbles */
    char       bubble_msg[32];
    float      bubble_timer;   /* >0 = showing */
    float      bubble_cd;      /* cooldown until next bubble */

    /* Sleep */
    float      sleep_timer;

    /* Particles */
    struct { float x, y, vx, vy, life; uint16_t color; } particles[16];

    /* Ambient background particles (drawn behind buddy at higher stages) */
    #define NB_AMB_MAX 24
    struct { float x, y, vx, vy, life, max_life; uint16_t color; int size; } amb[24];
    float amb_spawn_timer;

    /* Recon XP splash (shown briefly on home screen) */
    int        recon_xp_gained;   /* >0 = show splash */
    int        recon_ssid_delta;  /* how many new SSIDs */
    int        recon_client_delta;/* how many new client MACs */
    int        recon_hs_delta;    /* how many new handshakes */
    int        recon_milestone_xp;/* bonus XP from milestones */
    int        recon_ranks_gained;/* how many stages advanced */
    float      recon_splash_timer;

    /* Prestige popup (shown when hitting max stage) */
    float      prestige_popup_timer; /* >0 = show popup, input locked */

    /* Critical stat alert */
    float      critical_alert_cd;    /* cooldown between critical alerts */

    /* LED pulse state */
    float      led_pulse_timer;

    /* Real SSID ticker — new SSID discovery */
    char       ssid_ticker[48];      /* current display string */
    float      ssid_ticker_timer;    /* countdown to next DB check */
    int        ssid_ticker_sticky;   /* cycles to keep showing ssid_ticker */
    int        ssid_bg_count;        /* last known SSID count (passive, not saved) */

    /* Afterimage trail (P7+) */
    struct { float x, y; } afterimages[6];
    int        afterimage_idx;
    float      afterimage_timer;

    /* Screen shake (P15+) */
    float      shake_timer;

    /* Teleport effect (S12+) */
    float      teleport_cd;        /* cooldown until next teleport */
    float      teleport_flash;     /* >0 = flash/static effect active */
    float      teleport_old_x;     /* position before teleport (for static burst) */

    /* Smooth sprite scale (lerps toward target for transitions) */
    float      display_scale;

} NBGame;

/* ════════════════════════════════════════════════════════════════════ */
/*                     SAVE / LOAD                                     */
/* ════════════════════════════════════════════════════════════════════ */

/* Forward declaration — defined below helpers */
static void nb_generate_traits(BuddyStats *b);

static int nb_save(const BuddyStats *b) {
    SaveData sd;
    sd.magic   = NB_SAVE_MAGIC;
    sd.version = NB_SAVE_VERSION;
    sd.buddy   = *b;
    FILE *f = fopen(NB_SAVE_PATH, "wb");
    if (!f) return 0;
    size_t n = fwrite(&sd, sizeof(sd), 1, f);
    fclose(f);
    return n == 1;
}

static int nb_load(BuddyStats *b) {
    FILE *f = fopen(NB_SAVE_PATH, "rb");
    if (!f) return 0;
    SaveData sd;
    memset(&sd, 0, sizeof(sd));
    size_t n = fread(&sd, 1, sizeof(sd), f);
    fclose(f);
    /* Accept if we read at least magic + version + old buddy struct */
    if (n < 8 || sd.magic != NB_SAVE_MAGIC)
        return 0;
    *b = sd.buddy;
    /* Migrate v1 saves — generate traits if missing */
    if (b->trait_seed == 0)
        nb_generate_traits(b);
    /* Migrate v2 saves — prestige + recon fields default to 0 (from memset) */
    return 1;
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     HELPERS                                         */
/* ════════════════════════════════════════════════════════════════════ */

static int nb_clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── Recon DB integration ───────────────────────────────────────────── */
/* Query recon.db for distinct SSID count via sqlite3 CLI.
 * Returns -1 on error (db missing, sqlite3 not found, etc.) */
static int nb_query_ssid_count(void) {
    FILE *fp = popen(
        "sqlite3 " NB_RECON_DB_PATH
        " 'SELECT COUNT(DISTINCT ssid) FROM SSID;' 2>/dev/null",
        "r");
    if (!fp) return -1;
    char buf[32];
    int count = -1;
    if (fgets(buf, sizeof(buf), fp))
        count = atoi(buf);  /* 0 if parse fails, which is safe */
    pclose(fp);
    return count;
}

/* Award XP for newly discovered SSIDs.
 * ssid_now: current distinct SSID count from recon.db.
 * Returns XP awarded (0 if no delta). */
static int nb_check_recon_xp(BuddyStats *b, int ssid_now) {
    int delta = ssid_now - b->last_ssid_count;
    if (delta <= 0) {
        b->last_ssid_count = ssid_now; /* sync if count somehow shrank */
        return 0;
    }

    int xp_gain = delta * NB_RECON_XP_RATE;
    b->xp += xp_gain;
    b->last_ssid_count = ssid_now;
    return xp_gain;
}

/* Fetch the most recently added SSID from recon.db.
 * Writes into buf (max buflen). Returns 1 on success, 0 on failure. */
static int nb_query_newest_ssid(char *buf, int buflen) {
    FILE *fp = popen(
        "sqlite3 " NB_RECON_DB_PATH
        " 'SELECT ssid FROM SSID ORDER BY rowid DESC LIMIT 1;' 2>/dev/null",
        "r");
    if (!fp) return 0;
    int ok = 0;
    if (fgets(buf, buflen, fp)) {
        int len = (int)strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        if (buf[0] != '\0') ok = 1;
    }
    pclose(fp);
    return ok;
}

/* Query recon.db for distinct client MAC count.
 * Returns -1 on error. */
static int nb_query_client_count(void) {
    FILE *fp = popen(
        "sqlite3 " NB_RECON_DB_PATH
        " 'SELECT COUNT(DISTINCT mac) FROM wifi_client_ssid;' 2>/dev/null",
        "r");
    if (!fp) return -1;
    char buf[32];
    int count = -1;
    if (fgets(buf, sizeof(buf), fp))
        count = atoi(buf);
    pclose(fp);
    return count;
}

/* Count .pcap files in the handshakes loot directory.
 * Returns -1 on error. */
static int nb_count_handshakes(void) {
    FILE *fp = popen(
        "find " NB_LOOT_HANDSHAKE_PATH " -name '*.pcap' 2>/dev/null | wc -l",
        "r");
    if (!fp) return -1;
    char buf[32];
    int count = -1;
    if (fgets(buf, sizeof(buf), fp))
        count = atoi(buf);
    pclose(fp);
    return count;
}

/* Check & award XP for new client MACs.  Returns XP awarded. */
static int nb_check_client_xp(BuddyStats *b, int client_now) {
    int delta = client_now - b->last_client_count;
    if (delta <= 0) {
        b->last_client_count = client_now;
        return 0;
    }
    int xp_gain = delta * NB_CLIENT_XP_RATE;
    b->xp += xp_gain;
    b->last_client_count = client_now;
    return xp_gain;
}

/* Check & award XP for new handshake captures.  Returns XP awarded. */
static int nb_check_handshake_xp(BuddyStats *b, int hs_now) {
    int delta = hs_now - b->last_handshake_count;
    if (delta <= 0) {
        b->last_handshake_count = hs_now;
        return 0;
    }
    int xp_gain = delta * NB_HANDSHAKE_XP;
    b->xp += xp_gain;
    b->last_handshake_count = hs_now;
    return xp_gain;
}

/* Check & award SSID milestone bonuses.  Returns total bonus XP. */
static int nb_check_ssid_milestones(BuddyStats *b, int ssid_count) {
    int bonus = 0;
    for (int i = b->ssid_milestone; i < (int)NB_NUM_MILESTONES; i++) {
        if (ssid_count >= SSID_MILESTONES[i]) {
            bonus += NB_MILESTONE_XP;
            b->ssid_milestone = i + 1;
        } else {
            break;
        }
    }
    if (bonus > 0)
        b->xp += bonus;
    return bonus;
}

/* Get a mood-appropriate terminal message.  Returns a pointer to a
 * static or const string. */
static const char *nb_mood_terminal_msg(NBGame *g) {
    BuddyStats *b = &g->buddy;
    static const char *hungry_msgs[] = {
        "> null-buddy is starving!",
        "> feed me or i rm -rf myself",
        "> packets... need packets...",
        "> hunger overflow detected",
        "> stomach.c: segfault",
        "> cat /dev/fridge: empty",
        "> crontab: eat * * * * *",
        "> wget dinner.pcap FAILED",
        "> sudo apt-get install food",
        "> /proc/belly: 0 bytes",
        "> traceroute to pizza timed out",
        "> tcpdump -i stomach: no packets",
        "> DNS resolved: starvation.local",
    };
    static const char *sad_msgs[] = {
        "> null-buddy feels abandoned...",
        "> connection timed out on love",
        "> happiness: 404 not found",
        "> who needs friends when u have shells",
        "> grep -r 'joy' /self: 0 results",
        "> ssh friend@anyone: refused",
        "> loneliness.service: active",
        "> tail -f /var/log/tears",
        "> chmod 000 feelings/",
        "> ping happiness: 100% loss",
        "> DROP TABLE emotions;",
        "> git commit -m 'why tho'",
        "> curl love.api: 403 forbidden",
    };
    static const char *tired_msgs[] = {
        "> null-buddy is exhausted!",
        "> uptime too long. halp.",
        "> kernel panic: out of naps",
        "> energy level: /dev/null",
        "> load average: 99.9 99.9 99.9",
        "> OOM killer targeting self",
        "> swap space: your pillow",
        "> systemctl start sleep.service",
        "> top: buddy using 100% CPU",
        "> nice -n 19 existence",
        "> /proc/energy: permission denied",
        "> cron: sleep job overdue",
        "> dmesg: caffeine buffer empty",
    };
    static const char *thriving_msgs[] = {
        "> null-buddy is thriving! :D",
        "> feeling l33t today!",
        "> vibes.sh running at 100%",
        "> status: absolutely hacking",
        "> living my best root life",
        "> chmod 777 happiness/",
        "> exploit landed. life is good.",
        "> pwned the whole subnet :3",
        "> root@life: everything is fine",
        "> nmap says all ports open on joy",
        "> hashcat cracked my smile",
        "> metasploit: session 1 opened",
        "> reverse shell on serotonin",
        "> today's CVE: too happy",
        "> gobuster found /good-vibes",
    };
    static const char *kinda_hungry[] = {
        "> null-buddy wants packets...",
        "> could use a byte to eat",
        "> sniffing for snacks...",
        "> arp -a: no food found",
        "> pinging the fridge...",
    };
    static const char *kinda_sad[] = {
        "> null-buddy is lonely :(",
        "> netstat: 0 connections",
        "> no one on my subnet :(",
        "> arp table: just me",
        "> listening on port lonely",
    };
    int r = g->term_idx;
    /* New SSID discovery takes priority over mood messages */
    if (g->ssid_ticker_sticky > 0 && g->ssid_ticker[0] != '\0')
        return g->ssid_ticker;
    if (b->hunger < 10)     return hungry_msgs[r % 13];
    if (b->happiness < 10)  return sad_msgs[r % 13];
    if (b->energy < 10)     return tired_msgs[r % 13];
    if (b->hunger < 25)     return kinda_hungry[r % 5];
    if (b->happiness < 25)  return kinda_sad[r % 5];
    if (b->happiness > 90)  return thriving_msgs[r % 15];
    /* Fallback to normal messages */
    return TERMINAL_MSGS[g->term_idx];
}

static int nb_get_stage(int xp, int dev_mode) {
    int divisor = dev_mode ? 100 : 1;
    for (int i = NB_NUM_STAGES - 1; i >= 0; i--)
        if (xp >= STAGES[i].xp_threshold / divisor)
            return i;
    return 0;
}

static void nb_spawn_particles(NBGame *g, float x, float y, uint16_t color, int count) {
    for (int i = 0; i < 16 && count > 0; i++) {
        if (g->particles[i].life <= 0) {
            g->particles[i].x = x;
            g->particles[i].y = y;
            g->particles[i].vx = (float)((rand() % 120) - 60);
            g->particles[i].vy = (float)(-(rand() % 60 + 30));
            g->particles[i].life = 0.4f + (rand() % 30) * 0.01f;
            g->particles[i].color = color;
            count--;
        }
    }
}

/* ── Ambient background particles (float behind buddy at higher stages) ── */
static uint16_t nb_buddy_color(const BuddyStats *b);  /* forward decl */
static void nb_tick_ambient(NBGame *g, float dt) {
    int tier = g->buddy.stage / 8;
    int pres = g->buddy.prestige;
    /* No ambient particles below stage 3 (unless prestiged) */
    if (g->buddy.stage < 3 && pres == 0) return;

    /* Number of particles scales with stage + prestige bonus */
    int max_active = 2 + g->buddy.stage + pres * 3;
    if (max_active > NB_AMB_MAX) max_active = NB_AMB_MAX;

    /* Update existing */
    for (int i = 0; i < NB_AMB_MAX; i++) {
        if (g->amb[i].life > 0) {
            g->amb[i].x += g->amb[i].vx * dt;
            g->amb[i].y += g->amb[i].vy * dt;
            /* Gentle sine drift for floaty feel */
            g->amb[i].x += sinf(g->anim_t * 2.0f + (float)i) * 8.0f * dt;
            g->amb[i].life -= dt;
        }
    }

    /* Spawn zone: follows buddy's wandering position, prestige widens it */
    float cx = SCREEN_W / 3.0f - 20.0f + g->wander_x;
    float cy = SCREEN_H / 2.0f + 16.0f;
    float zone_w = 60.0f + tier * 20.0f + pres * 12.0f;
    float zone_h = 50.0f + tier * 15.0f + pres * 8.0f;

    /* Spawn timer — faster at higher tiers & prestige */
    float interval = 0.35f - tier * 0.08f - pres * 0.04f;
    if (interval < 0.06f) interval = 0.06f;
    g->amb_spawn_timer += dt;
    if (g->amb_spawn_timer < interval) return;
    g->amb_spawn_timer = 0;

    uint16_t acc = nb_buddy_color(&g->buddy);

    /* Spawn one particle */
    for (int i = 0; i < NB_AMB_MAX; i++) {
        if (g->amb[i].life <= 0) {
            /* Count active */
            int active = 0;
            for (int j = 0; j < NB_AMB_MAX; j++)
                if (g->amb[j].life > 0) active++;
            if (active >= max_active) break;

            g->amb[i].x = cx + (float)((rand() % (int)(zone_w * 2)) - (int)zone_w);
            g->amb[i].y = cy + (float)((rand() % (int)(zone_h * 2)) - (int)zone_h);
            g->amb[i].vx = (float)((rand() % 20) - 10);
            g->amb[i].vy = (float)(-(rand() % 12 + 4));  /* drift upward */
            float base_life = 2.0f + (rand() % 20) * 0.1f;
            g->amb[i].life = base_life;
            g->amb[i].max_life = base_life;
            /* Vary color: prestige adds wilder color variety */
            if (pres >= 3 && rand() % 4 == 0)
                g->amb[i].color = NB_YELLOW;
            else if (pres >= 2 && rand() % 4 == 0)
                g->amb[i].color = NB_PURPLE;
            else if (pres >= 1 && rand() % 3 == 0)
                g->amb[i].color = NB_PINK;
            else if (rand() % 3 == 0)
                g->amb[i].color = NB_DIM_GREEN;
            else
                g->amb[i].color = acc;
            /* Size grows with tier + prestige — small dots to larger motes */
            g->amb[i].size = 1 + (rand() % (1 + tier + (pres > 0 ? pres : 0)));
            if (g->amb[i].size > 5) g->amb[i].size = 5;
            break;
        }
    }
}


/* ── Idle Wandering ──────────────────────────────────────────────── */
static void nb_tick_wander(NBGame *g, float dt) {
    if (g->state != NB_STATE_HOME) return;

    /* Stage personality: early = jittery fast, mid = steady, late = slow imposing */
    int stage = g->buddy.stage;
    float speed_mult = stage < 4 ? 1.6f : (stage < 10 ? 1.0f : 0.6f);
    float change_time = stage < 4 ? 1.5f : (stage < 10 ? 3.0f : 5.0f);

    /* Mood affects speed: hungry = sluggish, happy = bouncy */
    if (g->buddy.hunger < 30) speed_mult *= 0.5f;
    if (g->buddy.happiness > 80) speed_mult *= 1.3f;
    if (g->buddy.energy < 20) speed_mult *= 0.3f;

    g->wander_timer -= dt;
    if (g->wander_timer <= 0) {
        /* Pick new direction: 40% pause, 30% left, 30% right */
        int r = rand() % 10;
        if (r < 4) {
            g->wander_vx = 0;
            g->wander_facing = 0;
        } else if (r < 7) {
            g->wander_vx = -(12.0f + (float)(rand() % 12)) * speed_mult;
            g->wander_facing = -1;
        } else {
            g->wander_vx = (12.0f + (float)(rand() % 12)) * speed_mult;
            g->wander_facing = 1;
        }
        g->wander_timer = change_time + (float)(rand() % 20) * 0.1f;
    }

    g->wander_x += g->wander_vx * dt;
    /* Clamp to stay in the left area */
    if (g->wander_x < -50.0f) { g->wander_x = -50.0f; g->wander_vx = 10.0f; g->wander_facing = 1; }
    if (g->wander_x > 50.0f)  { g->wander_x = 50.0f;  g->wander_vx = -10.0f; g->wander_facing = -1; }
}

/* ── Idle Fidgets ────────────────────────────────────────────────── */
static void nb_tick_fidget(NBGame *g, float dt) {
    if (g->state != NB_STATE_HOME) return;

    if (g->fidget_type > 0) {
        g->fidget_timer -= dt;
        if (g->fidget_timer <= 0) {
            g->fidget_type = 0;
            g->fidget_cd = 3.0f + (float)(rand() % 40) * 0.1f;
        }
        return;
    }

    g->fidget_cd -= dt;
    if (g->fidget_cd <= 0) {
        /* Pick a random fidget: 1=look-around, 2=shake, 3=head-tilt */
        g->fidget_type = 1 + (rand() % 3);
        g->fidget_timer = 0.6f + (float)(rand() % 6) * 0.1f;
    }
}

/* ── Chat Bubbles ────────────────────────────────────────────────── */
static void nb_tick_bubble(NBGame *g, float dt) {
    if (g->state != NB_STATE_HOME) return;

    if (g->bubble_timer > 0) {
        g->bubble_timer -= dt;
        return;
    }

    g->bubble_cd -= dt;
    if (g->bubble_cd > 0) return;

    /* Pick a message based on mood priority */
    const char *msg;
    BuddyStats *b = &g->buddy;
    if (b->energy < 20) {
        msg = BUBBLE_TIRED[rand() % NUM_BUBBLE_TIRED];
    } else if (b->hunger < 30) {
        msg = BUBBLE_HUNGRY[rand() % NUM_BUBBLE_HUNGRY];
    } else if (b->happiness < 30) {
        msg = BUBBLE_SAD[rand() % NUM_BUBBLE_SAD];
    } else if (b->happiness > 80) {
        msg = BUBBLE_HAPPY[rand() % NUM_BUBBLE_HAPPY];
    } else {
        msg = BUBBLE_IDLE[rand() % NUM_BUBBLE_IDLE];
    }
    snprintf(g->bubble_msg, sizeof(g->bubble_msg), "%s", msg);
    g->bubble_timer = 2.5f;
    g->bubble_cd = 5.0f + (float)(rand() % 50) * 0.1f;
}


/* Get fidget offsets for drawing */
static void nb_get_fidget_offset(NBGame *g, int *dx, int *dy) {
    *dx = 0; *dy = 0;
    if (g->fidget_type == 0) return;
    float t = g->fidget_timer;
    switch (g->fidget_type) {
    case 1: /* look-around: shift left then right */
        *dx = (t > 0.3f) ? -3 : 3;
        break;
    case 2: /* shake */
        *dx = ((int)(t * 30) % 2 == 0) ? 2 : -2;
        break;
    case 3: /* head-tilt */
        *dy = (t > 0.3f) ? -2 : 0;
        *dx = (t > 0.3f) ? 2 : -1;
        break;
    }
}

/* ── Action Reaction Particles ───────────────────────────────────── */
/* Hearts float up when pet */
static void nb_spawn_hearts(NBGame *g, float cx, float cy) {
    int count = 5;
    for (int i = 0; i < 16 && count > 0; i++) {
        if (g->particles[i].life <= 0) {
            g->particles[i].x = cx + (float)((rand() % 40) - 20);
            g->particles[i].y = cy + (float)((rand() % 10) - 5);
            g->particles[i].vx = (float)((rand() % 20) - 10);
            g->particles[i].vy = (float)(-(rand() % 30 + 20)); /* float up */
            g->particles[i].life = 0.8f + (rand() % 30) * 0.02f;
            g->particles[i].color = NB_PINK;
            count--;
        }
    }
}
/* Sparkles burst after play */
static void nb_spawn_sparkles(NBGame *g, float cx, float cy) {
    for (int i = 0, c = 0; i < 16 && c < 6; i++) {
        if (g->particles[i].life <= 0) {
            g->particles[i].x = cx + (float)((rand() % 30) - 15);
            g->particles[i].y = cy + (float)((rand() % 30) - 15);
            g->particles[i].vx = (float)((rand() % 80) - 40);
            g->particles[i].vy = (float)((rand() % 80) - 40);
            g->particles[i].life = 0.5f + (rand() % 20) * 0.01f;
            g->particles[i].color = NB_YELLOW;
            c++;
        }
    }
}

static void nb_set_action(NBGame *g, const char *msg) {
    snprintf(g->action_msg, sizeof(g->action_msg), "%s", msg);
    g->action_timer = 2.0f;
}

static void nb_init_buddy(BuddyStats *b) {
    memset(b, 0, sizeof(BuddyStats));
    snprintf(b->name, NB_MAX_NAME + 1, "NULL");
    b->hunger    = 50;
    b->happiness = 50;
    b->energy    = NB_MAX_STAT;
    b->stage     = 0;
}

/* Generate visual traits from the buddy's name (call after name is set) */
static void nb_generate_traits(BuddyStats *b) {
    /* Simple djb2 hash of name */
    uint32_t h = 5381;
    for (const char *p = b->name; *p; p++)
        h = ((h << 5) + h) + (uint32_t)*p;
    b->trait_seed  = h;
    b->color_shift = (int)((h >> 7) & 7) % 6;/* 0-5 */
}

/* Shift a color's hue slightly based on color_shift trait */
static uint16_t nb_tint_color(uint16_t col, int shift) {
    if (shift == 0) return col;
    int r = (col >> 11) & 0x1F;
    int g = (col >> 5)  & 0x3F;
    int b = col & 0x1F;
    switch (shift) {
    case 1: r = (r + 6 > 31) ? 31 : r + 6; break;           /* warmer */
    case 2: b = (b + 6 > 31) ? 31 : b + 6; break;           /* cooler */
    case 3: g = (g + 12 > 63) ? 63 : g + 12; break;         /* greener */
    case 4: r = (r + 4 > 31) ? 31 : r + 4;
            b = (b + 4 > 31) ? 31 : b + 4; break;           /* pinker */
    case 5: r = (r + 3 > 31) ? 31 : r + 3;
            g = (g + 6 > 63) ? 63 : g + 6; break;           /* golden */
    }
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* Return a unique color for this buddy at this stage, seeded by traits */
static uint16_t nb_buddy_color(const BuddyStats *b) {
    uint16_t base = (b->stage >= 0 && b->stage < NB_NUM_STAGES)
                    ? STAGES[b->stage].accent_color : NB_GREEN;
    /* Mix in trait seed so each buddy gets a unique shade per stage */
    uint32_t mix = b->trait_seed ^ ((uint32_t)b->stage * 2654435761u);
    int shift = (int)(mix & 7) % 6;  /* 0-5 */
    return nb_tint_color(base, shift);
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     STAT DECAY / TICK                               */
/* ════════════════════════════════════════════════════════════════════ */

static void nb_tick_stats(NBGame *g, float dt) {
    BuddyStats *b = &g->buddy;

    /* Hunger decays */
    g->hunger_acc += dt;
    while (g->hunger_acc >= HUNGER_DECAY_RATE) {
        g->hunger_acc -= HUNGER_DECAY_RATE;
        b->hunger = nb_clamp(b->hunger - 1, 0, NB_MAX_STAT);
    }

    /* Happiness decays */
    g->happy_acc += dt;
    while (g->happy_acc >= HAPPINESS_DECAY_RATE) {
        g->happy_acc -= HAPPINESS_DECAY_RATE;
        b->happiness = nb_clamp(b->happiness - 1, 0, NB_MAX_STAT);
    }

    /* Energy regenerates (when not playing) */
    if (g->state != NB_STATE_PLAY) {
        g->energy_acc += dt;
        while (g->energy_acc >= ENERGY_REGEN_RATE) {
            g->energy_acc -= ENERGY_REGEN_RATE;
            b->energy = nb_clamp(b->energy + 1, 0, NB_MAX_STAT);
        }
    }

    /* Passive XP */
    g->xp_acc += dt;
    {
        float xp_rate = g->dev_mode ? (PASSIVE_XP_RATE / 10.0f) : PASSIVE_XP_RATE;
        while (g->xp_acc >= xp_rate) {
            g->xp_acc -= xp_rate;
            b->xp++;
        }
    }

    /* Age */
    g->age_acc += dt;
    while (g->age_acc >= AGE_TICK_RATE) {
        g->age_acc -= AGE_TICK_RATE;
        b->age_minutes++;
    }

    /* Demo mode — auto-level every 3 seconds, auto-prestige at max */
    if (g->demo_mode) {
        g->demo_timer += dt;
        if (g->demo_timer >= 3.0f) {
            g->demo_timer = 0;
            if (b->stage >= NB_NUM_STAGES - 1) {
                /* Auto-prestige */
                b->prestige++;
                b->xp = 0;
                b->stage = 0;
                b->last_ssid_count = 0;
                nb_set_action(g, "** PRESTIGE! **");
                hw_level_up_feedback();
                nb_spawn_particles(g, SCREEN_W / 3.0f - 20 + g->wander_x,
                                   SCREEN_H / 2.0f + 16, nb_buddy_color(b), 16);
            } else {
                /* Advance one stage */
                b->stage++;
                b->xp = STAGES[b->stage].xp_threshold;
                nb_set_action(g, "** LEVEL UP! **");
                hw_level_up_feedback();
                nb_spawn_particles(g, SCREEN_W / 3.0f - 20 + g->wander_x,
                                   SCREEN_H / 2.0f + 16, nb_buddy_color(b), 16);
            }
        }
    }

    /* Check level up */
    int new_stage = nb_get_stage(b->xp, g->dev_mode);
    if (new_stage > b->stage) {
        b->stage = new_stage;
        nb_set_action(g, "** LEVEL UP! **");
        hw_level_up_feedback();
        nb_spawn_particles(g, SCREEN_W / 3.0f - 20 + g->wander_x,
                           SCREEN_H / 2.0f + 16, nb_buddy_color(b), 16);
        if (b->prestige >= 15) g->shake_timer = 0.5f;
        /* Prompt prestige when hitting max stage */
        if (new_stage >= NB_NUM_STAGES - 1 && !g->demo_mode) {
            g->prestige_popup_timer = 3.0f;
        }
    }
}


/* Rendering functions split into separate file */
#include "null_buddy_render.c"


/* ════════════════════════════════════════════════════════════════════ */
/*                     ENGINE CALLBACKS                                */
/* ════════════════════════════════════════════════════════════════════ */

static void nb_init(Engine *engine, void *userdata) {
    (void)engine;
    NBGame *g = (NBGame *)userdata;
    srand((unsigned)time(NULL));
    memset(g, 0, sizeof(NBGame));
    g->last_tick = _clock_now();
    g->ssid_bg_count = -1; /* sentinel: first DB check silently syncs */

    /* Try to load saved buddy */
    if (nb_load(&g->buddy)) {
        g->state = NB_STATE_HOME;
    } else {
        g->state = NB_STATE_TITLE;
        nb_init_buddy(&g->buddy);
    }

    hw_led_dpad_all(LED_GREEN);
    hw_led_a_button(40);
    hw_led_b_button(20);

    /* Initialize display scale to current target */
    g->display_scale = (float)nb_get_sprite_scale(g->buddy.stage);
}

static void nb_update(Engine *engine, float dt, void *userdata) {
    NBGame *g = (NBGame *)userdata;
    InputContext *inp = &engine->input;
    BuddyStats *b = &g->buddy;

    /* Animation timers */
    g->anim_t += dt;
    g->bounce_t += dt;

    /* Smooth scale interpolation toward target */
    {
        /* Determine target scale based on current mood/state */
        int mood = NB_MOOD_DEFAULT;
        if (b->energy < 25)         mood = NB_MOOD_SLEEPY_ID;
        else if (b->hunger < 30)    mood = NB_MOOD_HUNGRY_ID;
        else if (b->happiness < 30) mood = NB_MOOD_SAD_ID;
        else if (b->happiness > 75) mood = NB_MOOD_HAPPY_ID;
        float target;
        if (mood != NB_MOOD_DEFAULT) {
            const Sprite *ms = nb_get_mood_sprite(mood, (int)(g->anim_t * 0.125f));
            target = ms ? (float)NB_MOOD_SPRITE_SCALE
                        : (float)nb_get_sprite_scale(b->stage);
        } else {
            target = (float)nb_get_sprite_scale(b->stage);
        }
        /* Lerp toward target (speed 4 = smooth ~0.25s transition) */
        float diff = target - g->display_scale;
        if (diff > 0.01f || diff < -0.01f)
            g->display_scale += diff * dt * 4.0f;
        else
            g->display_scale = target;
    }

    /* Action message timer */
    if (g->action_timer > 0) g->action_timer -= dt;

    /* Recon splash timer */
    if (g->recon_splash_timer > 0) g->recon_splash_timer -= dt;

    /* Prestige popup timer */
    if (g->prestige_popup_timer > 0) g->prestige_popup_timer -= dt;

    /* Cooldowns */
    if (g->feed_cd > 0) g->feed_cd -= dt;
    if (g->pet_cd > 0) g->pet_cd -= dt;

    /* Particles */
    for (int i = 0; i < 16; i++) {
        if (g->particles[i].life > 0) {
            g->particles[i].x += g->particles[i].vx * dt;
            g->particles[i].y += g->particles[i].vy * dt;
            g->particles[i].vy += 150.0f * dt;
            g->particles[i].life -= dt;
        }
    }

    /* Ambient background particles */
    nb_tick_ambient(g, dt);

    /* Idle behaviors */
    nb_tick_wander(g, dt);
    nb_tick_fidget(g, dt);
    nb_tick_bubble(g, dt);

    /* Teleport effect — S12+ occasionally blink to a new position */
    if (g->state == NB_STATE_HOME && b->stage >= 12) {
        if (g->teleport_flash > 0) {
            g->teleport_flash -= dt;
        }
        g->teleport_cd -= dt;
        if (g->teleport_cd <= 0) {
            /* Save old position for static burst */
            g->teleport_old_x = g->wander_x;
            /* Teleport to random wander offset */
            g->wander_x = -40.0f + (float)(rand() % 80);
            g->teleport_flash = 0.35f;
            /* Higher stages teleport more often */
            float base_cd = b->stage >= 16 ? 5.0f : (b->stage >= 14 ? 8.0f : 12.0f);
            g->teleport_cd = base_cd + (float)(rand() % 50) * 0.1f;
        }
    }

    /* Mood-based bounce speed: happy = energetic, tired = slow */
    {
        float bounce_speed = 3.0f;
        if (g->buddy.happiness > 80) bounce_speed = 5.0f;
        else if (g->buddy.energy < 20) bounce_speed = 1.2f;
        else if (g->buddy.hunger < 30) bounce_speed = 1.8f;
        g->bounce_t += (bounce_speed - 3.0f) * dt; /* adjust on top of base */
    }

    /* Terminal message scroller + new SSID discovery ticker */
    g->term_timer += dt;
    if (g->term_timer > 4.0f) {
        g->term_timer = 0;
        g->term_idx = (g->term_idx + 1) % (int)NUM_TERMINAL_MSGS;
        if (g->ssid_ticker_sticky > 0) g->ssid_ticker_sticky--;
    }
    /* Check for new SSIDs every 15 seconds (lightweight COUNT query) */
    g->ssid_ticker_timer += dt;
    if (g->ssid_ticker_timer >= 15.0f) {
        g->ssid_ticker_timer = 0;
        int count = nb_query_ssid_count();
        if (count >= 0 && g->ssid_bg_count < 0) {
            g->ssid_bg_count = count; /* initial sync — don't flash old SSIDs */
        } else if (count > 0 && count > g->ssid_bg_count) {
            /* New SSID(s) discovered — fetch the latest name */
            char tmp[40];
            if (nb_query_newest_ssid(tmp, sizeof(tmp))) {
                snprintf(g->ssid_ticker, sizeof(g->ssid_ticker),
                         "> NEW SSID!: %s", tmp);
                g->ssid_ticker_sticky = 3; /* show for 3 ticker cycles */
                /* Buddy reacts to the discovery */
                snprintf(g->bubble_msg, sizeof(g->bubble_msg), "%s",
                         BUBBLE_SSID_FOUND[rand() % NUM_BUBBLE_SSID]);
                g->bubble_timer = 3.0f;
                hw_vibrate(30);
            }
            g->ssid_bg_count = count;
        }
    }

    /* Critical stat alerts — vibrate + beep when stats dangerously low */
    if (g->critical_alert_cd > 0) g->critical_alert_cd -= dt;
    if (g->critical_alert_cd <= 0 && g->state == NB_STATE_HOME) {
        BuddyStats *bs = &g->buddy;
        if (bs->hunger < 10 || bs->happiness < 10 || bs->energy < 10) {
            hw_vibrate(40);
            hw_beep(200, 30);
            g->critical_alert_cd = 30.0f;  /* alert every 30s max */
        }
    }

    /* LED pulse — match buddy's accent color */
    g->led_pulse_timer += dt;
    if (g->led_pulse_timer >= 1.0f) {
        g->led_pulse_timer = 0;
        uint16_t c = nb_buddy_color(&g->buddy);
        /* Convert RGB565 to LedColor (8-bit per channel) */
        uint8_t lr = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
        uint8_t lg = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
        uint8_t lb = (uint8_t)((c & 0x1F) * 255 / 31);
        /* Dim to ~30% for subtle pulse */
        LedColor lc = { lr / 3, lg / 3, lb / 3 };
        hw_led_dpad_all(lc);
    }

    /* Tick stats (except on title/naming/confirm) */
    if (g->state != NB_STATE_TITLE && g->state != NB_STATE_NAMING &&
        g->state != NB_STATE_CONFIRM_NAME)
        nb_tick_stats(g, dt);

    switch (g->state) {

    /* ── TITLE ─────────────────────────────────────────────────────── */
    case NB_STATE_TITLE:
        if (input_pressed(inp, BTN_A)) {
            if (nb_load(&g->buddy)) {
                g->state = NB_STATE_HOME;
                nb_set_action(g, "Welcome back!");
            } else {
                g->state = NB_STATE_NAMING;
                nb_init_buddy(&g->buddy);
                g->name_cursor = 0;
                g->name_char = 0;
                memset(g->buddy.name, 0, sizeof(g->buddy.name));
                g->buddy.name[0] = 'A';
            }
            hw_beep(600, 40);
        }
        break;

    /* ── NAMING ────────────────────────────────────────────────────── */
    case NB_STATE_NAMING:
        if (input_pressed(inp, BTN_UP)) {
            g->name_char = (g->name_char + 1) % 26;
            b->name[g->name_cursor] = 'A' + g->name_char;
            hw_beep(400 + g->name_char * 20, 20);
        }
        if (input_pressed(inp, BTN_DOWN)) {
            g->name_char = (g->name_char + 25) % 26;
            b->name[g->name_cursor] = 'A' + g->name_char;
            hw_beep(400 + g->name_char * 20, 20);
        }
        if (input_pressed(inp, BTN_RIGHT) && g->name_cursor < NB_MAX_NAME - 1) {
            g->name_cursor++;
            if (b->name[g->name_cursor] == '\0') {
                g->name_char = 0;
                b->name[g->name_cursor] = 'A';
            } else {
                g->name_char = b->name[g->name_cursor] - 'A';
            }
        }
        if (input_pressed(inp, BTN_LEFT) && g->name_cursor > 0) {
            g->name_cursor--;
            g->name_char = b->name[g->name_cursor] - 'A';
        }
        if (input_pressed(inp, BTN_A)) {
            /* Trim trailing empty chars */
            b->name[g->name_cursor + 1] = '\0';
            g->confirm_cursor = 0;
            g->state = NB_STATE_CONFIRM_NAME;
            hw_beep(600, 20);
        }
        break;

    /* ── CONFIRM NAME ──────────────────────────────────────────────── */
    case NB_STATE_CONFIRM_NAME:
        if (input_pressed(inp, BTN_LEFT) || input_pressed(inp, BTN_RIGHT)) {
            g->confirm_cursor = !g->confirm_cursor;
            hw_beep(500, 15);
        }
        if (input_pressed(inp, BTN_A)) {
            if (g->confirm_cursor == 1) {
                /* Confirmed — create buddy */
                nb_generate_traits(b);
                g->state = NB_STATE_TUTORIAL;
                g->tutorial_page = 0;
                nb_save(b);
                nb_set_action(g, "Buddy created!");
                hw_beep(800, 40);
                hw_beep(1000, 60);
            } else {
                /* Go back to naming */
                g->state = NB_STATE_NAMING;
                g->name_char = b->name[g->name_cursor] - 'A';
                hw_beep(300, 20);
            }
        }
        if (input_pressed(inp, BTN_B)) {
            g->state = NB_STATE_NAMING;
            g->name_char = b->name[g->name_cursor] - 'A';
            hw_beep(300, 20);
        }
        break;

    /* ── HOME ──────────────────────────────────────────────────────── */
    case NB_STATE_HOME:
        /* Lock input while prestige popup is showing */
        if (g->prestige_popup_timer > 0) break;
        if (input_pressed(inp, BTN_UP)) {
            g->menu_cursor = (g->menu_cursor + MENU_COUNT - 1) % MENU_COUNT;
            hw_beep(500, 15);
        }
        if (input_pressed(inp, BTN_DOWN)) {
            g->menu_cursor = (g->menu_cursor + 1) % MENU_COUNT;
            hw_beep(500, 15);
        }
        if (input_pressed(inp, BTN_A)) {
            switch (g->menu_cursor) {
            case 0: /* Feed */
                if (g->feed_cd > 0 && !g->dev_mode) {
                    static const char *feed_cd_msgs[] = {
                        "Not hungry yet...",
                        "buffer full!",
                        "still digesting...",
                        "queue overflow!",
                        "wait for ACK...",
                        "429 Too Many Reqs",
                        "rate limited!",
                        "packet dropped.",
                    };
                    nb_set_action(g, feed_cd_msgs[rand() % 8]);
                    hw_beep(200, 40);
                } else {
                    b->hunger = nb_clamp(b->hunger + FEED_HUNGER_GAIN, 0, NB_MAX_STAT);
                    b->xp += g->dev_mode ? FEED_XP_GAIN * 10 : FEED_XP_GAIN;
                    b->times_fed++;
                    b->total_packets++;
                    g->feed_cd = FEED_COOLDOWN;
                    static const char *feed_msgs[] = {
                        "> packet consumed!",
                        "> nom nom nom",
                        "> TCP/delicious",
                        "> *burp* thx",
                        "> yummy payload",
                        "> 200 OK (tasty)",
                        "> consumed 1500 MTU",
                        "> checksum: valid",
                        "> injected snacks",
                        "> data exfiltrated",
                        "> mmm fresh pcap",
                        "> SYN-ACK-NOM",
                        "> /dev/stomach full",
                        "> ate the whole LAN",
                        "> deep packet yum",
                        "> sudo feed --force",
                    };
                    nb_set_action(g, feed_msgs[rand() % 16]);
                    nb_spawn_particles(g, SCREEN_W / 3.0f - 20 + g->wander_x,
                                       SCREEN_H / 2.0f + 16, NB_GREEN, 8);
                    hw_beep(600, 30);
                    hw_beep(800, 30);
                    hw_vibrate(30);
                }
                nb_save(b);
                break;
            case 1: /* Pet */
                if (g->pet_cd > 0 && !g->dev_mode) {
                    static const char *pet_cd_msgs[] = {
                        "Give me space...",
                        "boundaries pls",
                        "firewall is up!",
                        "access denied!",
                        "rate limited :(",
                        "too many touches",
                        "403 Forbidden",
                        "IDS triggered!",
                    };
                    nb_set_action(g, pet_cd_msgs[rand() % 8]);
                    hw_beep(200, 40);
                } else {
                    b->happiness = nb_clamp(b->happiness + PET_HAPPINESS_GAIN, 0, NB_MAX_STAT);
                    b->xp += g->dev_mode ? PET_XP_GAIN * 10 : PET_XP_GAIN;
                    b->times_pet++;
                    g->pet_cd = PET_COOLDOWN;
                    static const char *pet_msgs[] = {
                        "> pager patted!",
                        "> * purrs in hex *",
                        "> 0xAWW",
                        "> chmod +love",
                        "> head scritches!",
                        "> root of happiness",
                        "> feels.log updated",
                        "> kernel: joy++",
                        "> happiness.exe OK",
                        "> critical UwU found",
                        "> pat pat pwn pwn",
                        "> * wiggles antenna *",
                        "> serotonin injected",
                        "> CVE: cuteness 10.0",
                        "> stack overflow <3",
                        "> patched with love",
                    };
                    nb_set_action(g, pet_msgs[rand() % 16]);
                    nb_spawn_hearts(g, SCREEN_W / 3.0f - 20 + g->wander_x,
                                   SCREEN_H / 2.0f + 16);
                    hw_beep(700, 20);
                    hw_beep(900, 30);
                    hw_vibrate(20);
                }
                nb_save(b);
                break;
            case 2: /* Play mini-game */
                if (b->energy < PLAY_ENERGY_COST) {
                    nb_set_action(g, "Too tired...");
                    hw_beep(200, 60);
                } else {
                    g->state = NB_STATE_PLAY;
                    g->mg_player_x = SCREEN_W / 2.0f - MG_PLAYER_W / 2.0f;
                    g->mg_score = 0;
                    g->mg_misses = 0;
                    g->mg_timer = MG_DURATION;
                    g->mg_spawn_timer = 0;
                    memset(g->mg_packets, 0, sizeof(g->mg_packets));
                    b->energy -= PLAY_ENERGY_COST;
                    b->times_played++;
                    hw_beep(500, 30);
                    hw_beep(700, 30);
                    hw_led_dpad_all(LED_CYAN);
                }
                break;
            case 3: /* Stats */
                g->state = NB_STATE_STATS;
                hw_beep(600, 20);
                break;
            case 4: /* Options */
                g->state = NB_STATE_OPTIONS;
                g->options_cursor = 0;
                hw_beep(600, 20);
                break;
            }
        }
        break;

    /* ── PLAY (mini-game) ──────────────────────────────────────────── */
    case NB_STATE_PLAY:
        g->mg_timer -= dt;
        if (g->mg_timer <= 0) {
            /* Game over — award XP */
            int xp_earned = PLAY_BASE_XP + g->mg_score * 2;
            if (g->dev_mode) xp_earned *= 10;
            b->xp += xp_earned;
            b->happiness = nb_clamp(b->happiness + PLAY_HAPPINESS_GAIN, 0, NB_MAX_STAT);
            if (g->mg_score > b->mg_high_score)
                b->mg_high_score = g->mg_score;
            char buf[48];
            snprintf(buf, sizeof(buf), "Score:%d XP+%d", g->mg_score, xp_earned);
            nb_set_action(g, buf);
            g->state = NB_STATE_HOME;
            nb_spawn_sparkles(g, SCREEN_W / 3.0f - 20 + g->wander_x,
                              SCREEN_H / 2.0f + 16);
            nb_save(b);
            hw_beep(800, 50);
            hw_beep(600, 50);
            hw_led_dpad_all(LED_GREEN);
            break;
        }

        /* Move player */
        if (input_held(inp, BTN_LEFT))
            g->mg_player_x -= MG_BASE_SPEED * 2.0f * dt;
        if (input_held(inp, BTN_RIGHT))
            g->mg_player_x += MG_BASE_SPEED * 2.0f * dt;
        if (g->mg_player_x < 0) g->mg_player_x = 0;
        if (g->mg_player_x > SCREEN_W - MG_PLAYER_W)
            g->mg_player_x = (float)(SCREEN_W - MG_PLAYER_W);

        /* Spawn packets */
        g->mg_spawn_timer += dt;
        if (g->mg_spawn_timer >= MG_SPAWN_INTERVAL) {
            g->mg_spawn_timer -= MG_SPAWN_INTERVAL;
            for (int i = 0; i < MG_MAX_PACKETS; i++) {
                if (!g->mg_packets[i].active) {
                    g->mg_packets[i].active = 1;
                    g->mg_packets[i].x = (float)(rand() % (SCREEN_W - MG_PACKET_W));
                    g->mg_packets[i].y = -(float)MG_PACKET_H;
                    g->mg_packets[i].speed = MG_BASE_SPEED + (float)(rand() % 40);
                    g->mg_packets[i].good = (rand() % 100) < 70;
                    break;
                }
            }
        }

        /* Update packets */
        for (int i = 0; i < MG_MAX_PACKETS; i++) {
            MGPacket *p = &g->mg_packets[i];
            if (!p->active) continue;
            p->y += p->speed * dt;

            /* Catch check */
            int catcher_y = SCREEN_H - MG_PLAYER_H - 4;
            if (p->y + MG_PACKET_H >= catcher_y &&
                p->x + MG_PACKET_W > g->mg_player_x &&
                p->x < g->mg_player_x + MG_PLAYER_W) {
                p->active = 0;
                if (p->good) {
                    g->mg_score++;
                    nb_spawn_particles(g, p->x + MG_PACKET_W / 2.0f,
                                       (float)catcher_y, NB_GREEN, 4);
                    hw_beep(700, 15);
                } else {
                    g->mg_misses++;
                    nb_spawn_particles(g, p->x + MG_PACKET_W / 2.0f,
                                       (float)catcher_y, NB_RED, 6);
                    hw_beep(150, 30);
                    hw_vibrate(30);
                }
            }

            /* Off screen */
            if (p->y > SCREEN_H + MG_PACKET_H)
                p->active = 0;
        }
        break;

    /* ── STATS ─────────────────────────────────────────────────────── */
    case NB_STATE_STATS:
        if (input_pressed(inp, BTN_A)) {
            int is_max = (b->stage >= NB_NUM_STAGES - 1);
            if (is_max && !g->dev_mode) {
                /* Prestige — reset XP/stage/recon, keep everything else */
                b->prestige++;
                b->xp = 0;
                b->stage = 0;
                b->last_ssid_count = 0;
                b->last_client_count = 0;
                b->last_handshake_count = 0;
                b->ssid_milestone = 0;
                nb_set_action(g, "** PRESTIGE! **");
                hw_level_up_feedback();
                nb_spawn_particles(g, SCREEN_W / 3.0f - 20 + g->wander_x,
                                   SCREEN_H / 2.0f + 16, nb_buddy_color(b), 16);
                if (b->prestige >= 15) g->shake_timer = 0.5f;
                nb_save(b);
                g->state = NB_STATE_HOME;
            } else if (is_max && g->dev_mode) {
                /* Dev mode prestige — same but with dev thresholds */
                b->prestige++;
                b->xp = 0;
                b->stage = 0;
                b->last_ssid_count = 0;
                b->last_client_count = 0;
                b->last_handshake_count = 0;
                b->ssid_milestone = 0;
                nb_set_action(g, "** PRESTIGE! **");
                hw_level_up_feedback();
                if (b->prestige >= 15) g->shake_timer = 0.5f;
                nb_save(b);
                g->state = NB_STATE_HOME;
            } else {
                g->state = NB_STATE_HOME;
                hw_beep(400, 15);
            }
        }
        if (input_pressed(inp, BTN_B)) {
            g->state = NB_STATE_HOME;
            hw_beep(400, 15);
        }
        break;
    /* ── OPTIONS ──────────────────────────────────────────────────── */
    case NB_STATE_OPTIONS:
        if (input_pressed(inp, BTN_UP)) {
            g->options_cursor = (g->options_cursor + OPTIONS_COUNT - 1) % OPTIONS_COUNT;
            hw_beep(500, 15);
        }
        if (input_pressed(inp, BTN_DOWN)) {
            g->options_cursor = (g->options_cursor + 1) % OPTIONS_COUNT;
            hw_beep(500, 15);
        }
        if (input_pressed(inp, BTN_B)) {
            g->state = NB_STATE_HOME;
            hw_beep(400, 15);
        }
        if (input_pressed(inp, BTN_A)) {
            switch (g->options_cursor) {
            case 0: /* Recon Feed */
            {
                int ssid_now = nb_query_ssid_count();
                if (ssid_now < 0) {
                    nb_set_action(g, "recon.db not found!");
                    hw_beep(200, 60);
                } else {
                    int old_stage = b->stage;
                    int total_xp = 0;

                    /* SSIDs */
                    int ssid_delta = ssid_now - b->last_ssid_count;
                    if (ssid_delta < 0) ssid_delta = 0;
                    g->recon_ssid_delta = ssid_delta;
                    total_xp += nb_check_recon_xp(b, ssid_now);

                    /* Client MACs */
                    int client_now = nb_query_client_count();
                    int client_delta = 0;
                    if (client_now > 0) {
                        client_delta = client_now - b->last_client_count;
                        if (client_delta < 0) client_delta = 0;
                        total_xp += nb_check_client_xp(b, client_now);
                    }
                    g->recon_client_delta = client_delta;

                    /* Handshakes */
                    int hs_now = nb_count_handshakes();
                    int hs_delta = 0;
                    if (hs_now > 0) {
                        hs_delta = hs_now - b->last_handshake_count;
                        if (hs_delta < 0) hs_delta = 0;
                        total_xp += nb_check_handshake_xp(b, hs_now);
                    }
                    g->recon_hs_delta = hs_delta;

                    /* SSID milestones */
                    int milestone_bonus = nb_check_ssid_milestones(b, ssid_now);
                    g->recon_milestone_xp = milestone_bonus;
                    total_xp += milestone_bonus;

                    g->recon_xp_gained = total_xp;

                    if (total_xp > 0) {
                        int new_stage = nb_get_stage(b->xp, g->dev_mode);
                        g->recon_ranks_gained = new_stage - old_stage;
                        if (new_stage > b->stage) {
                            b->stage = new_stage;
                            hw_level_up_feedback();
                        }
                        g->recon_splash_timer = 5.0f;
                        nb_save(b);
                        hw_beep(800, 40);
                        hw_beep(1000, 60);
                    } else {
                        nb_set_action(g, "No new data found.");
                        hw_beep(400, 30);
                    }
                }
                g->state = NB_STATE_HOME;
                break;
            }
            case 1: /* Reset */
                g->state = NB_STATE_CONFIRM_RESET;
                g->confirm_cursor = 0;
                hw_beep(300, 40);
                break;
            case 2: /* Tutorial */
                g->state = NB_STATE_TUTORIAL;
                g->tutorial_page = 0;
                hw_beep(600, 20);
                break;
            case 3: /* Dev Mode toggle */
                g->dev_mode = !g->dev_mode;
                if (g->dev_mode)
                    hw_beep(1000, 40);
                else
                    hw_beep(300, 40);
                break;
            case 4: /* Demo Mode toggle */
                g->demo_mode = !g->demo_mode;
                g->demo_timer = 0;
                if (g->demo_mode)
                    hw_beep(1200, 40);
                else
                    hw_beep(300, 40);
                break;
            }
        }
        break;

    /* ── CONFIRM RESET ───────────────────────────────────────────── */
    case NB_STATE_CONFIRM_RESET:
        if (input_pressed(inp, BTN_LEFT) || input_pressed(inp, BTN_RIGHT)) {
            g->confirm_cursor = !g->confirm_cursor;
            hw_beep(500, 15);
        }
        if (input_pressed(inp, BTN_B)) {
            g->state = NB_STATE_OPTIONS;
            hw_beep(400, 15);
        }
        if (input_pressed(inp, BTN_A)) {
            if (g->confirm_cursor == 1) {
                /* Delete save and restart */
                remove(NB_SAVE_PATH);
                nb_init_buddy(&g->buddy);
                g->state = NB_STATE_NAMING;
                g->name_cursor = 0;
                g->name_char = 0;
                memset(g->buddy.name, 0, sizeof(g->buddy.name));
                g->buddy.name[0] = 'A';
                g->menu_cursor = 0;
                hw_beep(200, 80);
                hw_beep(150, 80);
                hw_beep(100, 120);
                hw_vibrate(100);
            } else {
                g->state = NB_STATE_HOME;
                hw_beep(400, 15);
            }
        }
        break;
    /* ── SLEEPING ──────────────────────────────────────────────────── */
    case NB_STATE_SLEEPING:
        g->sleep_timer -= dt;
        if (g->sleep_timer <= 0) {
            g->state = NB_STATE_HOME;
            hw_beep(600, 30);
        }
        break;

    /* ── TUTORIAL ──────────────────────────────────────────────────── */
    case NB_STATE_TUTORIAL:
        if (input_pressed(inp, BTN_A)) {
            g->tutorial_page++;
            if (g->tutorial_page >= NB_TUTORIAL_PAGES) {
                g->state = NB_STATE_HOME;
                hw_beep(800, 40);
                hw_beep(1000, 60);
            } else {
                hw_beep(600, 20);
            }
        }
        if (input_pressed(inp, BTN_B)) {
            g->state = NB_STATE_HOME;
            hw_beep(400, 15);
        }
        break;

    default:
        break;
    }
}


static void nb_cleanup(Engine *engine, void *userdata) {
    (void)engine;
    NBGame *g = (NBGame *)userdata;
    /* Auto-save on exit */
    nb_save(&g->buddy);
    hw_led_all_off();
}

/* ════════════════════════════════════════════════════════════════════ */
/*                     MAIN                                            */
/* ════════════════════════════════════════════════════════════════════ */

int main(void) {
    NBGame game;
    Engine engine;

    if (engine_create(&engine, nb_init, nb_update, nb_render, nb_cleanup, &game) < 0) {
        fprintf(stderr, "Failed to create engine\n");
        return 1;
    }

    engine.target_fps = 15;
    engine_run(&engine);
    engine_destroy(&engine);

    return 0;
}