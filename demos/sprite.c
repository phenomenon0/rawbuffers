/*
 * sprite.c — Animated sprite on tiled background
 *
 * Procedurally generated sprite sheet, arrow key movement,
 * tiled background using rb_blit, sprite drawn with rb_blit_alpha.
 *
 * Build: gcc -O2 -o sprite demos/sprite.c -lm -lpthread
 * Run:   ./sprite
 * Quit:  q or ESC
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>
#include <string.h>

#define SPRITE_W  16
#define SPRITE_H  16
#define NUM_FRAMES 4
#define TILE_W    16
#define TILE_H    16
#define MOVE_SPEED 2

/* Sprite sheet: 4 animation frames, each 16x16 */
static uint32_t sprite_sheet[NUM_FRAMES][SPRITE_H][SPRITE_W];

/* Two tile types for checkerboard */
static uint32_t tile_a[TILE_H][TILE_W];
static uint32_t tile_b[TILE_H][TILE_W];

/* Camera and sprite position */
static float sprite_x, sprite_y;
static float cam_x, cam_y;
static int anim_frame = 0;
static int anim_counter = 0;
static bool moving = false;

static void generate_assets(void) {
    /* Generate smiley sprite with 4 animation frames */
    for (int f = 0; f < NUM_FRAMES; f++) {
        /* Clear to transparent */
        for (int y = 0; y < SPRITE_H; y++)
            for (int x = 0; x < SPRITE_W; x++)
                sprite_sheet[f][y][x] = RB_RGBA(0, 0, 0, 0);

        /* Body circle (yellow) */
        int cx = SPRITE_W / 2, cy = SPRITE_H / 2;
        for (int y = 0; y < SPRITE_H; y++) {
            for (int x = 0; x < SPRITE_W; x++) {
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy <= 49) { /* radius ~7 */
                    sprite_sheet[f][y][x] = RB_RGBA(255, 220, 50, 255);
                }
            }
        }

        /* Eyes */
        int eye_y = cy - 2;
        /* Blink on frame 2 */
        if (f == 2) {
            /* Blinking: horizontal line for eyes */
            sprite_sheet[f][eye_y][cx - 3] = RB_RGBA(40, 40, 40, 255);
            sprite_sheet[f][eye_y][cx - 2] = RB_RGBA(40, 40, 40, 255);
            sprite_sheet[f][eye_y][cx + 2] = RB_RGBA(40, 40, 40, 255);
            sprite_sheet[f][eye_y][cx + 3] = RB_RGBA(40, 40, 40, 255);
        } else {
            /* Normal eyes */
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 0; dx++) {
                    sprite_sheet[f][eye_y + dy][cx - 3 + dx] = RB_RGBA(40, 40, 40, 255);
                    sprite_sheet[f][eye_y + dy][cx + 2 + dx] = RB_RGBA(40, 40, 40, 255);
                }
        }

        /* Mouth varies per frame for animation */
        int mouth_y = cy + 2;
        int mouth_w = 2 + f;  /* wider smile each frame */
        for (int mx = -mouth_w; mx <= mouth_w; mx++) {
            int my = (mx * mx) / (mouth_w + 1); /* parabolic smile */
            if (mouth_y + my < SPRITE_H && cx + mx >= 0 && cx + mx < SPRITE_W)
                sprite_sheet[f][mouth_y + my][cx + mx] = RB_RGBA(180, 60, 40, 255);
        }
    }

    /* Generate tiles */
    for (int y = 0; y < TILE_H; y++) {
        for (int x = 0; x < TILE_W; x++) {
            /* Tile A: grass-like green with subtle pattern */
            int g = 90 + ((x ^ y) & 3) * 10;
            tile_a[y][x] = RB_RGB(30, g, 40);

            /* Tile B: slightly different shade */
            g = 80 + ((x + y) & 3) * 8;
            tile_b[y][x] = RB_RGB(25, g, 35);
        }
    }
}

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;

    /* Movement */
    moving = false;
    if (rb_key_down(&ctx->input, RB_KEY_LEFT))  { sprite_x -= MOVE_SPEED; moving = true; }
    if (rb_key_down(&ctx->input, RB_KEY_RIGHT)) { sprite_x += MOVE_SPEED; moving = true; }
    if (rb_key_down(&ctx->input, RB_KEY_UP))    { sprite_y -= MOVE_SPEED; moving = true; }
    if (rb_key_down(&ctx->input, RB_KEY_DOWN))  { sprite_y += MOVE_SPEED; moving = true; }

    /* Animate on movement */
    if (moving) {
        anim_counter++;
        if (anim_counter >= 8) {
            anim_counter = 0;
            anim_frame = (anim_frame + 1) % NUM_FRAMES;
        }
    } else {
        anim_frame = 0;
        anim_counter = 0;
    }

    /* Camera follows sprite (centered) */
    cam_x = sprite_x - s->width / 2 + SPRITE_W / 2;
    cam_y = sprite_y - s->height / 2 + SPRITE_H / 2;

    /* Draw tiled background */
    int start_tx = (int)floorf(cam_x / TILE_W);
    int start_ty = (int)floorf(cam_y / TILE_H);
    int end_tx = start_tx + s->width / TILE_W + 2;
    int end_ty = start_ty + s->height / TILE_H + 2;

    for (int ty = start_ty; ty <= end_ty; ty++) {
        for (int tx = start_tx; tx <= end_tx; tx++) {
            int sx = tx * TILE_W - (int)cam_x;
            int sy = ty * TILE_H - (int)cam_y;
            uint32_t *tile = ((tx + ty) & 1) ? (uint32_t *)tile_a : (uint32_t *)tile_b;
            rb_blit(s, sx, sy, tile, TILE_W, TILE_H, TILE_W);
        }
    }

    /* Draw sprite with alpha blending */
    int draw_x = (int)(sprite_x - cam_x);
    int draw_y = (int)(sprite_y - cam_y);
    rb_blit_alpha(s, draw_x, draw_y,
                  (uint32_t *)sprite_sheet[anim_frame],
                  SPRITE_W, SPRITE_H, SPRITE_W);

    /* HUD */
    rb_text(s, 4, 4, "SPRITE DEMO", RB_RGB(255, 255, 255));
    rb_text(s, 4, 16, "Arrow keys to move", RB_RGB(180, 180, 200));

    char info[64];
    snprintf(info, sizeof(info), "pos: %.0f,%.0f  frame:%d  FPS:%d",
             sprite_x, sprite_y, anim_frame, ctx->fps);
    rb_text(s, 4, s->height - 12, info, RB_RGB(160, 160, 160));
}

int main(void) {
    generate_assets();
    sprite_x = 0; sprite_y = 0;

    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);
    rb_run(&ctx, (rb_callbacks_t){ .draw = draw });
    rb_destroy(&ctx);
    return 0;
}
