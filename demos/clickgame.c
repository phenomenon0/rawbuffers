/*
 * clickgame.c — Click targets with collision detection + sprite animation + score
 *
 * Targets spawn randomly, click them to score. Uses mouse input,
 * collision helpers, and sprite frame helper.
 *
 * Build: gcc -O2 -o clickgame demos/clickgame.c -lm -lpthread
 * Run:   ./clickgame
 * Quit:  q or ESC
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_TARGETS  12
#define SPRITE_W     16
#define SPRITE_H     16
#define NUM_FRAMES   4
#define SHEET_COLS   4

/* Sprite sheet: 4 frames in a row (64×16 pixels) */
static uint32_t sprite_pixels[SPRITE_H][SPRITE_W * NUM_FRAMES];
static rb_sprite_t cursor_sprite;

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    bool  active;
    float hue;
} target_t;

static target_t targets[MAX_TARGETS];
static int score;
static int combo;
static float combo_timer;
static int anim_frame;
static int anim_tick;
static float flash_timer;
static int flash_x, flash_y;

/* Scale factor, set once from screen size */
static float SC;
static int TARGET_R;

static void generate_sprite(void) {
    for (int f = 0; f < NUM_FRAMES; f++) {
        int ox = f * SPRITE_W;
        int cx = SPRITE_W / 2, cy = SPRITE_H / 2;

        for (int y = 0; y < SPRITE_H; y++)
            for (int x = 0; x < SPRITE_W; x++)
                sprite_pixels[y][ox + x] = RB_RGBA(0, 0, 0, 0);

        float angle = f * 0.25f;
        for (int i = -6; i <= 6; i++) {
            int hx = cx + (int)(i * cosf(angle));
            int hy = cy + (int)(i * sinf(angle));
            if (hx >= 0 && hx < SPRITE_W && hy >= 0 && hy < SPRITE_H)
                sprite_pixels[hy][ox + hx] = RB_RGBA(255, 255, 255, 220);
            int vx = cx + (int)(i * cosf(angle + 1.5708f));
            int vy = cy + (int)(i * sinf(angle + 1.5708f));
            if (vx >= 0 && vx < SPRITE_W && vy >= 0 && vy < SPRITE_H)
                sprite_pixels[vy][ox + vx] = RB_RGBA(255, 255, 255, 220);
        }
        sprite_pixels[cy][ox + cx] = RB_RGBA(255, 80, 80, 255);
    }

    cursor_sprite = (rb_sprite_t){
        .pixels = (const uint32_t *)sprite_pixels,
        .frame_w = SPRITE_W,
        .frame_h = SPRITE_H,
        .sheet_stride = SPRITE_W * NUM_FRAMES,
        .cols = SHEET_COLS,
    };
}

static void spawn_target(int sw, int sh) {
    int margin = TARGET_R + (int)(10 * SC);
    int top = (int)(30 * SC) + margin;
    int bot = sh - (int)(20 * SC) - margin;
    if (bot <= top) bot = top + 10;
    for (int i = 0; i < MAX_TARGETS; i++) {
        if (targets[i].active) continue;
        targets[i].x = (float)(margin + rand() % (sw - 2 * margin));
        targets[i].y = (float)(top + rand() % (bot - top));
        targets[i].vx = (rand() % 60 - 30) / 10.0f * SC;
        targets[i].vy = (rand() % 60 - 30) / 10.0f * SC;
        targets[i].life = 3.0f + (rand() % 30) / 10.0f;
        targets[i].hue = (float)(rand() % 100) / 100.0f;
        targets[i].active = true;
        return;
    }
}

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;
    int W = s->width, H = s->height;
    float dt = (float)ctx->dt;
    rb_clear(s, RB_RGB(10, 12, 20));

    int margin = (int)(6 * SC);
    int text_gap = (int)(12 * SC);

    /* Spawn targets */
    static float spawn_timer = 0;
    spawn_timer += dt;
    if (spawn_timer > 0.8f) {
        spawn_target(W, H);
        spawn_timer = 0;
    }

    /* Update and draw targets */
    for (int i = 0; i < MAX_TARGETS; i++) {
        target_t *t = &targets[i];
        if (!t->active) continue;

        t->x += t->vx;
        t->y += t->vy;
        t->life -= dt;

        if (t->x < TARGET_R || t->x > W - TARGET_R) t->vx = -t->vx;
        if (t->y < (int)(30*SC) + TARGET_R || t->y > H - (int)(20*SC) - TARGET_R) t->vy = -t->vy;

        if (t->life <= 0) {
            t->active = false;
            combo = 0;
            continue;
        }

        float pulse = 1.0f + 0.15f * sinf(t->life * 8.0f);
        int r = (int)(TARGET_R * pulse);

        float alpha = t->life < 1.0f ? t->life : 1.0f;
        uint32_t color = rb_lerp_color(RB_RGB(40, 40, 40), rb_hue(t->hue), alpha);
        rb_circle_fill(s, (int)t->x, (int)t->y, r, color);
        rb_circle(s, (int)t->x, (int)t->y, r, RB_RGB(255, 255, 255));

        int pts = 10 + combo * 5;
        char ptxt[16];
        snprintf(ptxt, sizeof(ptxt), "%d", pts);
        rb_text(s, (int)t->x - 4, (int)t->y - 4, ptxt, RB_RGB(255, 255, 255));
    }

    /* Mouse click → hit test */
    if (rb_mouse_pressed(&ctx->input, RB_MOUSE_LEFT)) {
        int mx = ctx->input.mouse_x;
        int my = ctx->input.mouse_y;
        for (int i = 0; i < MAX_TARGETS; i++) {
            target_t *t = &targets[i];
            if (!t->active) continue;
            if (rb_overlap_circle(mx, my, 0, (int)t->x, (int)t->y, TARGET_R)) {
                int pts = 10 + combo * 5;
                score += pts;
                combo++;
                combo_timer = 2.0f;
                flash_timer = 0.3f;
                flash_x = (int)t->x;
                flash_y = (int)t->y;
                t->active = false;
                break;
            }
        }
    }

    if (combo_timer > 0) {
        combo_timer -= dt;
        if (combo_timer <= 0) combo = 0;
    }

    if (flash_timer > 0) {
        flash_timer -= dt;
        int fr = (int)((0.3f - flash_timer) * 80 * SC);
        rb_circle(s, flash_x, flash_y, fr,
                  RB_RGBA(255, 255, 200, (uint8_t)(flash_timer / 0.3f * 200)));
    }

    /* Crosshair cursor */
    anim_tick++;
    if (anim_tick >= 10) { anim_tick = 0; anim_frame = (anim_frame + 1) % NUM_FRAMES; }
    rb_sprite_draw(s, ctx->input.mouse_x - SPRITE_W / 2,
                      ctx->input.mouse_y - SPRITE_H / 2,
                      &cursor_sprite, anim_frame);

    /* HUD — scaled positions */
    char hud[128];
    snprintf(hud, sizeof(hud), "SCORE: %d", score);
    rb_text(s, margin, margin, hud, RB_RGB(255, 255, 255));

    if (combo > 1) {
        snprintf(hud, sizeof(hud), "COMBO x%d!", combo);
        rb_text(s, margin, margin + text_gap, hud, rb_hue(ctx->time * 0.5f));
    }

    snprintf(hud, sizeof(hud), "mouse:%d,%d  FPS:%d  q=quit",
             ctx->input.mouse_x, ctx->input.mouse_y, ctx->fps);
    rb_text(s, margin, H - text_gap, hud, RB_RGB(100, 100, 120));
}

int main(void) {
    srand((unsigned)time(NULL));
    generate_sprite();
    memset(targets, 0, sizeof(targets));
    score = 0; combo = 0;

    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);

    /* Compute scale from actual screen size */
    SC = (float)ctx.surface.width / 160.0f;
    if (SC < 1.0f) SC = 1.0f;
    TARGET_R = (int)(12 * SC);
    if (TARGET_R < 8) TARGET_R = 8;

    rb_run(&ctx, (rb_callbacks_t){ .draw = draw });
    rb_destroy(&ctx);
    return 0;
}
