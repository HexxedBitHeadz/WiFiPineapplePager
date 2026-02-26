/*
 * pager_engine.h — Lightweight Game Engine for WiFi Pineapple Pager
 * Target: WiFi Pineapple Pager
 *
 * Combines pager_gfx.h + pager_input.h into a simple game loop engine.
 * Define PAGER_ENGINE_IMPLEMENTATION in exactly ONE .c file before including.
 *
 * Usage:
 *   #define PAGER_ENGINE_IMPLEMENTATION
 *   #include "pager_engine.h"
 */

#ifndef PAGER_ENGINE_H
#define PAGER_ENGINE_H

/*
 * Ensure POSIX/GNU extensions are available (clock_gettime, nanosleep, etc.)
 * Must be defined before including any system headers. The Makefiles also
 * pass -D_GNU_SOURCE via CFLAGS to guarantee this.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "pager_gfx.h"
#include "pager_input.h"
#include <signal.h>
#include <time.h>

/* ── Engine Configuration ───────────────────────────────────────────── */

#define DEFAULT_FPS  30

/* ── Engine Context ─────────────────────────────────────────────────── */

typedef struct Engine Engine;

/* Game callback function types */
typedef void (*GameInitFn)(Engine *engine, void *userdata);
typedef void (*GameUpdateFn)(Engine *engine, float dt, void *userdata);
typedef void (*GameRenderFn)(Engine *engine, void *userdata);
typedef void (*GameCleanupFn)(Engine *engine, void *userdata);

struct Engine {
    GfxContext    gfx;
    InputContext  input;

    int           target_fps;
    int           running;
    float         dt;           /* delta time in seconds */
    uint32_t      frame_count;
    float         fps_actual;   /* measured FPS */
    float         _quit_hold;   /* internal: B-button hold timer for exit */

    /* Game callbacks */
    GameInitFn    on_init;
    GameUpdateFn  on_update;
    GameRenderFn  on_render;
    GameCleanupFn on_cleanup;
    void         *userdata;     /* passed to all callbacks */
};

/* ── Function Prototypes ────────────────────────────────────────────── */

/*
 * engine_create — Initialize engine with game callbacks.
 *
 * Example:
 *   Engine engine;
 *   engine_create(&engine, my_init, my_update, my_render, my_cleanup, &game_state);
 *   engine_run(&engine);
 */
int  engine_create(Engine *engine,
                   GameInitFn init,
                   GameUpdateFn update,
                   GameRenderFn render,
                   GameCleanupFn cleanup,
                   void *userdata);

/*
 * engine_run — Enter the main game loop. Blocks until engine->running = 0
 * or SIGINT/SIGTERM is received.
 */
void engine_run(Engine *engine);

/*
 * engine_quit — Signal the engine to stop after the current frame.
 */
void engine_quit(Engine *engine);

/*
 * engine_destroy — Clean up all resources.
 */
void engine_destroy(Engine *engine);

/* ── Helper: High-Resolution Timer ──────────────────────────────────── */

static inline double _clock_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

#endif /* PAGER_ENGINE_H */

/* ════════════════════════════════════════════════════════════════════ */
/*                         IMPLEMENTATION                              */
/* ════════════════════════════════════════════════════════════════════ */

#ifdef PAGER_ENGINE_IMPLEMENTATION
#undef PAGER_ENGINE_IMPLEMENTATION

/* Pull in the implementations of our sub-libraries */
#define PAGER_GFX_IMPLEMENTATION
#include "pager_gfx.h"
#define PAGER_INPUT_IMPLEMENTATION
#include "pager_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

/* ── Signal Handling for Clean Exit ─────────────────────────────────── */

static volatile sig_atomic_t _engine_sig_quit = 0;

static void _sig_handler(int sig) {
    (void)sig;
    _engine_sig_quit = 1;
}

/* ── Native UI Management ───────────────────────────────────────────── */

/*
 * The Pager's native UI is /pineapple/pineapple — it draws to the display
 * and reads input, competing with our games. We SIGSTOP it on game start
 * and SIGCONT it on exit.
 */

static int _pineapple_pid = 0;

static int _find_pid_by_name(const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pidof %s 2>/dev/null", name);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;
    int pid = 0;
    if (fscanf(fp, "%d", &pid) != 1) pid = 0;
    pclose(fp);
    return pid;
}

static void _stop_native_ui(void) {
    _pineapple_pid = _find_pid_by_name("pineapple");
    if (_pineapple_pid > 0) {
        kill(_pineapple_pid, SIGSTOP);
        fprintf(stderr, "[engine] Suspended /pineapple/pineapple (PID %d)\n", _pineapple_pid);
    } else {
        fprintf(stderr, "[engine] No native UI process found\n");
    }
}

static void _resume_native_ui(void) {
    if (_pineapple_pid > 0) {
        kill(_pineapple_pid, SIGCONT);
        fprintf(stderr, "[engine] Resumed /pineapple/pineapple (PID %d)\n", _pineapple_pid);
        _pineapple_pid = 0;
    }
}

/* ── Engine Implementation ──────────────────────────────────────────── */

int engine_create(Engine *engine,
                  GameInitFn init,
                  GameUpdateFn update,
                  GameRenderFn render,
                  GameCleanupFn cleanup,
                  void *userdata) {
    memset(engine, 0, sizeof(Engine));

    engine->target_fps  = DEFAULT_FPS;
    engine->running     = 0;
    engine->frame_count = 0;
    engine->on_init     = init;
    engine->on_update   = update;
    engine->on_render   = render;
    engine->on_cleanup  = cleanup;
    engine->userdata    = userdata;

    /* Stop native UI before taking over framebuffer + input */
    _stop_native_ui();

    /* Initialize graphics */
    if (gfx_init(&engine->gfx) < 0) {
        fprintf(stderr, "[engine] Failed to initialize graphics\n");
        _resume_native_ui();
        return -1;
    }

    /* Initialize input (auto-detect device) */
    if (input_init(&engine->input, NULL) < 0) {
        fprintf(stderr, "[engine] Failed to initialize input (continuing without)\n");
    }

    /* Register signal handlers */
    signal(SIGINT, _sig_handler);
    signal(SIGTERM, _sig_handler);

    return 0;
}

void engine_run(Engine *engine) {
    engine->running = 1;

    /* Call game init */
    if (engine->on_init)
        engine->on_init(engine, engine->userdata);

    double frame_time = 1.0 / engine->target_fps;
    double last_time = _clock_now();
    double fps_timer = last_time;
    int    fps_frames = 0;

    fprintf(stderr, "[engine] Entering game loop (%d FPS target)\n", engine->target_fps);

    while (engine->running && !_engine_sig_quit) {
        double now = _clock_now();
        engine->dt = (float)(now - last_time);
        last_time = now;

        /* Clamp dt to prevent spiral of death */
        if (engine->dt > 0.1f) engine->dt = 0.1f;

        /* 1. Poll input */
        input_poll(&engine->input);

        /* Global exit: hold B (red) for 1.5 seconds */
        if (input_held(&engine->input, BTN_B)) {
            engine->_quit_hold += engine->dt;
            if (engine->_quit_hold >= 1.5f) {
                fprintf(stderr, "[engine] Quit: B held for 1.5s\n");
                engine->running = 0;
                break;
            }
        } else {
            engine->_quit_hold = 0.0f;
        }

        /* 2. Update game state */
        if (engine->on_update)
            engine->on_update(engine, engine->dt, engine->userdata);

        /* 3. Render */
        if (engine->on_render)
            engine->on_render(engine, engine->userdata);

        /* 4. Flip to screen */
        gfx_flip(&engine->gfx);

        engine->frame_count++;
        fps_frames++;

        /* Calculate actual FPS once per second */
        if (now - fps_timer >= 1.0) {
            engine->fps_actual = fps_frames / (float)(now - fps_timer);
            fps_frames = 0;
            fps_timer = now;
        }

        /* 5. Frame rate limiting */
        double elapsed = _clock_now() - now;
        double sleep_time = frame_time - elapsed;
        if (sleep_time > 0.0) {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = (long)(sleep_time * 1e9);
            nanosleep(&ts, NULL);
        }
    }

    fprintf(stderr, "[engine] Exiting game loop (total frames: %u)\n", engine->frame_count);

    /* Call game cleanup */
    if (engine->on_cleanup)
        engine->on_cleanup(engine, engine->userdata);

    /* Clear the screen on exit so it doesn't freeze on last frame */
    gfx_clear(&engine->gfx, 0x0000);
    gfx_flip(&engine->gfx);
}

void engine_quit(Engine *engine) {
    engine->running = 0;
}

void engine_destroy(Engine *engine) {
    gfx_cleanup(&engine->gfx);
    input_cleanup(&engine->input);

    /* Resume native UI now that we're done */
    _resume_native_ui();

    memset(engine, 0, sizeof(Engine));
}

#endif /* PAGER_ENGINE_IMPLEMENTATION */
