/*
 * Web plasma demo — same as demos/plasma.c but builds with emcc
 */
#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;
    float t = (float)ctx->time;

    for (int y = 0; y < s->height; y++) {
        for (int x = 0; x < s->width; x++) {
            float fx = (float)x / s->width;
            float fy = (float)y / s->height;
            float v = sinf(fx * 10.0f + t)
                    + sinf(fy * 8.0f + t * 1.3f)
                    + sinf((fx + fy) * 6.0f + t * 0.7f)
                    + sinf(sqrtf(fx*fx + fy*fy) * 12.0f - t * 2.0f);
            v = v / 4.0f * 0.5f + 0.5f;
            rb_pixel(s, x, y, rb_hue(v + t * 0.1f));
        }
    }

    char info[64];
    snprintf(info, sizeof(info), "FPS:%d  %dx%d", ctx->fps, s->width, s->height);
    rb_text(s, 4, 4, info, RB_RGB(255, 255, 255));
    rb_text(s, 4, 14, "PLASMA (WASM)", RB_RGB(200, 200, 220));
}

int main(void) {
    rb_ctx_t ctx = rb_init(320, 160, RB_FB_CANVAS);
    rb_run(&ctx, (rb_callbacks_t){ .draw = draw });
    rb_destroy(&ctx);
    return 0;
}
