/*
 * plasma.c — Classic demoscene plasma effect
 *
 * No GPU. No libraries. Just sin() waves writing into a pixel buffer.
 *
 * Build: gcc -O2 -o plasma demos/plasma.c -lm
 * Run:   ./plasma
 * Quit:  q or ESC
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;
    float t = (float)ctx->time;

    for (int y = 0; y < s->height; y++) {
        float fy = (float)y;
        for (int x = 0; x < s->width; x++) {
            float fx = (float)x;

            /* Combine multiple sine waves for organic motion */
            float v = sinf(fx * 0.03f + t * 1.1f)
                    + sinf(fy * 0.04f + t * 0.7f)
                    + sinf((fx + fy) * 0.015f + t * 1.3f)
                    + sinf(sqrtf(fx * fx + fy * fy) * 0.02f - t * 0.9f);

            /* Map [-4, 4] to color via phase-shifted sines */
            uint8_t r = (uint8_t)(sinf(v * 0.7854f) * 127.0f + 128.0f);
            uint8_t g = (uint8_t)(sinf(v * 0.7854f + 2.094f) * 127.0f + 128.0f);
            uint8_t b = (uint8_t)(sinf(v * 0.7854f + 4.189f) * 127.0f + 128.0f);

            rb_pixel(s, x, y, RB_RGB(r, g, b));
        }
    }

    /* FPS overlay */
    char fps[32];
    snprintf(fps, sizeof(fps), "FPS:%d %dx%d", ctx->fps, s->width, s->height);
    rb_text(s, 4, 4, fps, RB_RGB(255, 255, 255));
    rb_text(s, 4, 14, "q=quit", RB_RGB(180, 180, 180));
}

int main(void) {
    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);  /* auto-detect terminal size */
    rb_run(&ctx, (rb_callbacks_t){ .draw = draw });
    rb_destroy(&ctx);
    return 0;
}
