/*
 * game_data.h — Data Definitions for NULL-BUDDY
 * Cyber-Tamagotchi for WiFi Pineapple Pager
 *
 * Evolution stages, stat constants, terminal messages, save format.
 * Included by null_buddy.c — NOT standalone.
 */

#ifndef NB_GAME_DATA_H
#define NB_GAME_DATA_H

#include <stdint.h>

/* ════════════════════════════════════════════════════════════════════ */
/*                         CONSTANTS                                   */
/* ════════════════════════════════════════════════════════════════════ */

#define NB_MAX_NAME       12
#define NB_MAX_STAT       100
#define NB_NUM_STAGES     18

/* Stat decay rates (seconds per -1 point) */
#define HUNGER_DECAY_RATE     30.0f
#define HAPPINESS_DECAY_RATE  45.0f
#define ENERGY_REGEN_RATE     15.0f   /* seconds per +1 energy */
#define PASSIVE_XP_RATE       60.0f   /* seconds per +1 XP */
#define AGE_TICK_RATE         60.0f   /* seconds per +1 age minute */

/* Action effects */
#define FEED_HUNGER_GAIN      20
#define FEED_XP_GAIN          5
#define PET_HAPPINESS_GAIN    10
#define PET_XP_GAIN           2
#define PLAY_HAPPINESS_GAIN   15
#define PLAY_ENERGY_COST      20
#define PLAY_BASE_XP          10   /* + bonus from mini-game score */

/* Mini-game settings */
#define MG_DURATION           15.0f   /* seconds */
#define MG_MAX_PACKETS        12
#define MG_PACKET_W           20
#define MG_PACKET_H           14
#define MG_PLAYER_W           36
#define MG_PLAYER_H           12
#define MG_BASE_SPEED         80.0f
#define MG_SPAWN_INTERVAL     0.6f

/* Cooldowns */
#define FEED_COOLDOWN         3.0f    /* seconds between feeds */
#define PET_COOLDOWN          5.0f    /* seconds between pets */

/* Save system */
#define NB_SAVE_PATH    "/root/games/null-buddy/save.dat"
#define NB_SAVE_MAGIC   0x4E554C4C  /* "NULL" */
#define NB_SAVE_VERSION 3

/* Recon integration — reads Pineapple recon database */
#define NB_RECON_DB_PATH "/mmc/root/recon/recon.db"
#define NB_RECON_XP_RATE 1    /* XP per new SSID */
#define NB_CLIENT_XP_RATE 2   /* XP per new client MAC */
#define NB_HANDSHAKE_XP   25  /* XP per new handshake file */
#define NB_LOOT_HANDSHAKE_PATH "/mmc/root/loot/handshakes"

/* SSID milestone thresholds and bonus XP */
static const int SSID_MILESTONES[] = { 5, 10, 25, 50, 100, 250, 500, 1000 };
#define NB_NUM_MILESTONES (sizeof(SSID_MILESTONES) / sizeof(SSID_MILESTONES[0]))
#define NB_MILESTONE_XP   50  /* bonus XP per milestone reached */

/* ════════════════════════════════════════════════════════════════════ */
/*                     EVOLUTION STAGES                                */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *name;
    int         xp_threshold;
    uint16_t    accent_color;   /* neon accent for this stage */
} EvolutionStage;

static const EvolutionStage STAGES[NB_NUM_STAGES] = {
    /* Tier 0 — forms 0-7 */
    { "Binary Spore",     0,      RGB565(0x00, 0xFF, 0x66) },
    { "Packet Sniffer",   100,    RGB565(0x00, 0xCC, 0xFF) },
    { "Script Kiddie",    300,    RGB565(0xFF, 0x88, 0x00) },
    { "Botnet Node",      600,    RGB565(0xFF, 0x00, 0x88) },
    { "Root Shell",       1200,   RGB565(0xAA, 0x00, 0xFF) },
    { "Zero Day",         2500,   RGB565(0xFF, 0x44, 0x44) },
    { "Grid Overlord",    5000,   RGB565(0xFF, 0xEE, 0x00) },
    { "Singularity",      10000,  RGB565(0xFF, 0xFF, 0xFF) },
    /* Tier 1 — evolved forms */
    { "Cyber Wraith",     15000,  RGB565(0x00, 0xFF, 0xAA) },
    { "Shadow Proxy",     22000,  RGB565(0x44, 0x88, 0xFF) },
    { "Rogue Agent",      30000,  RGB565(0xFF, 0x66, 0x00) },
    { "Hive Mind",        40000,  RGB565(0xFF, 0x00, 0xCC) },
    { "Kernel Panic",     55000,  RGB565(0xCC, 0x44, 0xFF) },
    { "Exploit Chain",    75000,  RGB565(0xFF, 0x22, 0x00) },
    { "Digital Titan",    100000, RGB565(0xFF, 0xCC, 0x00) },
    { "The Architect",    150000, RGB565(0xCC, 0xFF, 0xFF) },
    /* Tier 2 — ascended forms */
    { "Omega Protocol",   250000, RGB565(0x88, 0xFF, 0x44) },
    { "NULL Prime",       500000, RGB565(0xFF, 0xFF, 0x88) },
};

/* ════════════════════════════════════════════════════════════════════ */
/*                     BUDDY STATS (PERSISTENT)                        */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char     name[NB_MAX_NAME + 1];
    int      stage;         /* 0–7 evolution stage */
    int      xp;
    int      hunger;        /* 0–100 */
    int      happiness;     /* 0–100 */
    int      energy;        /* 0–100 */
    int      age_minutes;   /* total alive time */
    int      times_fed;
    int      times_played;
    int      times_pet;
    int      mg_high_score; /* mini-game best */
    int      total_packets; /* lifetime packets "consumed" */

    /* Prestige (New Game+) */
    int      prestige;       /* number of times player hit max and reset */

    /* Recon integration */
    int      last_ssid_count; /* SSID count at last recon check */

    /* Visual traits — generated once at creation, saved */
    uint32_t trait_seed;    /* hash of name, drives all traits */
    int      last_client_count;  /* was _reserved1 — client MAC count at last check */
    int      last_handshake_count; /* was _reserved2 — handshake file count at last check */
    int      ssid_milestone;     /* was _reserved3 — index of last reached milestone */
    int      color_shift;   /* 0–5 hue tweak applied to accent */
} BuddyStats;

/* ════════════════════════════════════════════════════════════════════ */
/*                     SAVE DATA                                       */
/* ════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t   magic;
    uint32_t   version;
    BuddyStats buddy;
} SaveData;

/* ════════════════════════════════════════════════════════════════════ */
/*                     TERMINAL / FLAVOR TEXT                           */
/* ════════════════════════════════════════════════════════════════════ */

static const char *TERMINAL_MSGS[] = {
    "> Brought to you by Hexxed BitHeadz",
    "> scanning 2.4GHz band...",
    "> probe request captured",
    "> beacon frame processed",
    "> handshake intercepted!",
    "> deauth packet sent",
    "> SSID: CoffeeShop_5G",
    "> SSID: FREE_WIFI_xoxo",
    "> SSID: FBI_Van_7",
    "> SSID: DefinitelyNotAPineapple",
    "> SSID: PrettyFlyForAWiFi",
    "> SSID: DropItLikeItsHotspot",
    "> MAC: DE:AD:BE:EF:CA:FE",
    "> MAC: 00:C0:FF:EE:00:01",
    "> injecting craft pkt...",
    "> WPA2 hash captured",
    "> channel hopping: 1..6..11",
    "> PNL entry discovered",
    "> rogue AP deployed",
    "> karma attack active",
    "> PMKID extracted",
    "> 802.11 monitor mode OK",
    "> aircrack-ng loaded",
    "> parsing EAPOL frames...",
    "> entropy: 0xDEADC0DE",
    "> signal: -42 dBm [GOOD]",
    "> uplink established",
    "> root@pineapple:~#",
    "> null-buddy is hungry...",
    "> packet storm incoming!",
    "> firmware v4.2.0-rc1",
    "> /dev/null overflow!",
    "> segfault in feelings.c",
    "> rm -rf /loneliness",
    "> hashcat go brrrr",
    "> nmap -sS -T5 your_heart",
    "> pivoting to snack table",
    "> metasploit loaded. jk.",
    "> OSCP: 69 pts (PASS)",
    "> CVE-2024-BUDDY-0DAY",
    "> it's always DNS.",
    "> sudo make me a sandwich",
    "> chmod 777 feelings/",
    "> buffer overflow in love.c",
    "> enum4linux found: you <3",
    "> reverse shell to ur heart",
    "> recon complete. u r cute.",
    "> gobuster found /secrets",
    "> john says: password123",
    "> burpsuite intercepted UwU",
    "> wireshark sees all.",
    "> \"trust me, im a hacker\"",
};

#define NUM_TERMINAL_MSGS (sizeof(TERMINAL_MSGS) / sizeof(TERMINAL_MSGS[0]))

/* ════════════════════════════════════════════════════════════════════ */
/*                     CHAT BUBBLE MESSAGES                            */
/* ════════════════════════════════════════════════════════════════════ */

/* Idle / neutral */
static const char *BUBBLE_IDLE[] = {
    "...", "* beep boop *", "0xCAFE?", "< idle >",
    "* hums *", "zzZ..wait", "~bits~", "pkt plz",
    "null?", "* vibing *", "heh", "beep!",
    "01101...", "* chirp *", "lo?",
    "sudo pet me", "rm -rf bored", "it's GNU/me",
    "got root?", "I <3 0days", "pwn sweet pwn",
    "* sniffs *", "MITM myself", "CVE-me-0001",
    "hack the...", "im in", "port 1337",
    "chmod 777 :)", "ping!", "fork bomb?",
    "nmap says hi", "/dev/random", "just vibes",
    "ack!", "segfault lol", "core dumped",
    "id;whoami", "php is fine", "use vim btw",
};
#define NUM_BUBBLE_IDLE (sizeof(BUBBLE_IDLE)/sizeof(BUBBLE_IDLE[0]))

/* Hungry (hunger < 30) */
static const char *BUBBLE_HUNGRY[] = {
    "feed me!", "so hungry", "pkts plz", "* grumble *",
    "starving!", "need data", "empty...",
    "no packets?", "need pcaps", "drop tables",
    "feed > /dev/me", "TCP or food", "SYN me food",
    "pls no diet", "eat exploit", "hangry.exe",
    "starving.sh", "null lunch", "inject food",
};
#define NUM_BUBBLE_HUNGRY (sizeof(BUBBLE_HUNGRY)/sizeof(BUBBLE_HUNGRY[0]))

/* Sad (happiness < 30) */
static const char *BUBBLE_SAD[] = {
    "lonely...", "pet me?", "* sigh *", ":((",
    "miss you", "play?", "so bored",
    "404 joy", "null tears", "patched out",
    "bricked :(", "no shells?", "unplug me",
    "DENIED", "lost privs", "jailbreak?",
    "dropped pkt", "need hugs", "timeout :(",
    "ban sadness", "RST ACK :(", "* leaks *",
};
#define NUM_BUBBLE_SAD (sizeof(BUBBLE_SAD)/sizeof(BUBBLE_SAD[0]))

/* Tired (energy < 20) */
static const char *BUBBLE_TIRED[] = {
    "so tired", "zzZzZ", "* yawn *", "need nap",
    "low NRG", "sleepy..",
    "kill -9 me", "OOM killer", "swap full",
    "need reboot", "hybernate?", "uptime: 0",
    "power off?", "drain 0%", "* crashes *",
    "sleep 9999", "halt -p me", "no coffee",
};
#define NUM_BUBBLE_TIRED (sizeof(BUBBLE_TIRED)/sizeof(BUBBLE_TIRED[0]))

/* Happy (happiness > 80) */
static const char *BUBBLE_HAPPY[] = {
    ":DD", "* purrs *", "love u!", "yay!",
    "hehe!", "* dances *", "woo!",
    "rooted! :D", "got shell!", "pwned <3",
    "0wn3d!", "l33t!", "full access",
    "gg no re", "haxxor :3", "r00t dance",
    "we're in!", "popped box!", "king of LAN",
    "* h4cks *", "OP OP OP!", "big W",
    "no patch 4 me", "stack smash!", "ez clap",
};
#define NUM_BUBBLE_HAPPY (sizeof(BUBBLE_HAPPY)/sizeof(BUBBLE_HAPPY[0]))

/* SSID discovery reactions */
static const char *BUBBLE_SSID_FOUND[] = {
    "There's another 1!", "and another 1!", "psh, ez catch!",
    "pwned lol", "nom nom", "mine now",
    "snatched!", "packet go brrrr!", "yoink!",
    "heh. rekt.", "ohhh, owned.", "gg ez",
    "too easy", "sniffed it!", "l00tz!",
    "mmmm, tasty SSID", "more plz!", "delicious", 
    "*slurp*", "thx 4 dat", "keep em comin",
    "gime gime, more!", "Ur SSIDs, r mine!",
};
#define NUM_BUBBLE_SSID (sizeof(BUBBLE_SSID_FOUND)/sizeof(BUBBLE_SSID_FOUND[0]))

/* ════════════════════════════════════════════════════════════════════ */
/*                     CYBERPUNK COLOR PALETTE                         */
/* ════════════════════════════════════════════════════════════════════ */

#define NB_BG           RGB565(0x04, 0x04, 0x0E)
#define NB_GREEN        RGB565(0x00, 0xFF, 0x66)
#define NB_BLUE         RGB565(0x00, 0xCC, 0xFF)
#define NB_PINK         RGB565(0xFF, 0x00, 0x88)
#define NB_CYAN         RGB565(0x00, 0xFF, 0xDD)
#define NB_YELLOW       RGB565(0xFF, 0xEE, 0x00)
#define NB_PURPLE       RGB565(0xBB, 0x00, 0xFF)
#define NB_ORANGE       RGB565(0xFF, 0x88, 0x00)
#define NB_RED          RGB565(0xFF, 0x22, 0x33)
#define NB_DIM_GREEN    RGB565(0x00, 0x44, 0x1A)
#define NB_DIM_BLUE     RGB565(0x00, 0x22, 0x44)
#define NB_DIM_PINK     RGB565(0x44, 0x00, 0x22)
#define NB_DIM_CYAN     RGB565(0x00, 0x33, 0x33)
#define NB_DARK_BG      RGB565(0x06, 0x06, 0x12)
#define NB_HUD_BG       RGB565(0x0A, 0x0A, 0x1E)
#define NB_PANEL_BG     RGB565(0x08, 0x0A, 0x18)
#define NB_MENU_SEL     RGB565(0x18, 0x18, 0x38)
#define NB_GRID_LINE    RGB565(0x0C, 0x10, 0x1C)
#define NB_BAR_BG       RGB565(0x18, 0x0A, 0x10)
#define NB_HUNGER_BAR   RGB565(0x00, 0xCC, 0x44)
#define NB_HAPPY_BAR    RGB565(0xFF, 0xAA, 0x00)
#define NB_ENERGY_BAR   RGB565(0x00, 0xAA, 0xFF)

#endif /* NB_GAME_DATA_H */
