/*
 * editor.c — Video editor WASM core
 *
 * Exports pixel buffer + effect functions for the JS frontend.
 * No rb_run loop — JS drives the frame loop.
 *
 * Build: emcc -O2 -o editor.js editor.c -s MODULARIZE=1 -s EXPORT_NAME='RawBuf' ...
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static rb_surface_t surface;
static int frame_counter;

/* ─── Exported API ─── */

EMSCRIPTEN_KEEPALIVE
int editor_init(int w, int h) {
    /* Set canvas dimensions */
    EM_ASM({ document.getElementById('rawbuf').width = $0; }, w);
    EM_ASM({ document.getElementById('rawbuf').height = $0; }, h);

    /* Allocate surface */
    h = (h + 1) & ~1;
    surface.width = w;
    surface.height = h;
    surface.stride = w;
    surface.size = (size_t)w * h * sizeof(uint32_t);
    surface.mode = RB_FB_CANVAS;
    surface.fd = -1;
    surface.front = (uint32_t *)calloc((size_t)w * h, sizeof(uint32_t));
    surface.back  = (uint32_t *)calloc((size_t)w * h, sizeof(uint32_t));
    frame_counter = 0;
    return (int)surface.back;
}

EMSCRIPTEN_KEEPALIVE
int editor_get_buffer(void) { return (int)surface.back; }

EMSCRIPTEN_KEEPALIVE
int editor_width(void) { return surface.width; }

EMSCRIPTEN_KEEPALIVE
int editor_height(void) { return surface.height; }

EMSCRIPTEN_KEEPALIVE
void editor_present(void) {
    rb_present(&surface);
}

/* ─── Effect: Thermal / Infrared ─── */

static void effect_thermal(void) {
    int total = surface.width * surface.height;
    uint32_t *p = surface.back;
    for (int i = 0; i < total; i++) {
        uint32_t c = p[i];
        int lum = (RB_R(c) * 77 + RB_G(c) * 150 + RB_B(c) * 29) >> 8;
        float t = lum / 255.0f;
        uint32_t heat;
        if (t < 0.25f)
            heat = rb_lerp_color(RB_RGB(0,0,40), RB_RGB(0,0,200), t * 4.0f);
        else if (t < 0.5f)
            heat = rb_lerp_color(RB_RGB(0,0,200), RB_RGB(200,0,0), (t - 0.25f) * 4.0f);
        else if (t < 0.75f)
            heat = rb_lerp_color(RB_RGB(200,0,0), RB_RGB(255,255,0), (t - 0.5f) * 4.0f);
        else
            heat = rb_lerp_color(RB_RGB(255,255,0), RB_RGB(255,255,255), (t - 0.75f) * 4.0f);
        p[i] = heat;
    }
}

/* ─── Effect: Edge Detection (Sobel) ─── */

static void effect_edge(void) {
    int w = surface.width, h = surface.height;
    uint32_t *src = surface.back;
    uint32_t *tmp = (uint32_t *)malloc((size_t)w * h * 4);

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            /* Luminance of 3×3 neighborhood */
            int lum[9];
            int k = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t c = src[(y + dy) * w + (x + dx)];
                    lum[k++] = (RB_R(c) * 77 + RB_G(c) * 150 + RB_B(c) * 29) >> 8;
                }
            int gx = -lum[0] + lum[2] - 2*lum[3] + 2*lum[5] - lum[6] + lum[8];
            int gy = -lum[0] - 2*lum[1] - lum[2] + lum[6] + 2*lum[7] + lum[8];
            int mag = (int)sqrtf((float)(gx * gx + gy * gy));
            if (mag > 255) mag = 255;
            /* Color edges by angle */
            float angle = atan2f((float)gy, (float)gx) / 6.2831853f + 0.5f;
            uint32_t color = rb_hue(angle);
            tmp[y * w + x] = rb_lerp_color(RB_RGB(0, 0, 0), color, mag / 255.0f);
        }
    }
    memcpy(src, tmp, (size_t)w * h * 4);
    free(tmp);
}

/* ─── Effect: Night Vision ─── */

static void effect_nightvision(void) {
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;
    int cx = w / 2, cy = h / 2;
    float max_r = sqrtf((float)(cx * cx + cy * cy));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t c = p[y * w + x];
            int lum = (RB_R(c) * 77 + RB_G(c) * 150 + RB_B(c) * 29) >> 8;
            /* Boost + green tint */
            int g = lum + 40; if (g > 255) g = 255;
            int rb_val = lum / 4;
            /* Noise speckle */
            if ((rand() & 0xFF) < 8) { g += 30; if (g > 255) g = 255; }
            /* Vignette */
            float dx = (float)(x - cx), dy = (float)(y - cy);
            float dist = sqrtf(dx * dx + dy * dy) / max_r;
            float vig = 1.0f - dist * dist * 0.8f;
            if (vig < 0) vig = 0;
            /* Scanlines */
            float scan = (y & 1) ? 0.85f : 1.0f;
            p[y * w + x] = RB_RGB(
                (uint8_t)(rb_val * vig * scan),
                (uint8_t)(g * vig * scan),
                (uint8_t)(rb_val * vig * scan)
            );
        }
    }
}

/* ─── Effect: VHS / Retro ─── */

static void effect_vhs(void) {
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;

    /* Chromatic aberration: shift R left, B right */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int rl = x - 2 >= 0 ? x - 2 : 0;
            int br = x + 2 < w ? x + 2 : w - 1;
            uint32_t cl = p[y * w + rl];
            uint32_t cc = p[y * w + x];
            uint32_t cr = p[y * w + br];
            int r = RB_R(cl);
            int g = RB_G(cc);
            int b = RB_B(cr);
            /* Reduce color depth */
            r = (r >> 3) << 3;
            g = (g >> 2) << 2;
            b = (b >> 3) << 3;
            /* Scanline darkening */
            if (y & 1) { r = r * 3 / 4; g = g * 3 / 4; b = b * 3 / 4; }
            p[y * w + x] = RB_RGB(r, g, b);
        }
    }

    /* Random tracking noise bands */
    if ((rand() & 0x1F) == 0) {
        int band_y = rand() % h;
        int band_h = 2 + rand() % 4;
        int shift = (rand() % 10) - 5;
        for (int y = band_y; y < band_y + band_h && y < h; y++) {
            if (shift > 0) {
                memmove(p + y * w + shift, p + y * w, (size_t)(w - shift) * 4);
            } else if (shift < 0) {
                memmove(p + y * w, p + y * w - shift, (size_t)(w + shift) * 4);
            }
        }
    }
}

/* ─── Effect: Glitch ─── */

static void effect_glitch(void) {
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;

    /* Random block corruption */
    int num_blocks = 3 + rand() % 5;
    for (int b = 0; b < num_blocks; b++) {
        int bx = rand() % w;
        int by = rand() % h;
        int bw = 10 + rand() % 40;
        int bh = 2 + rand() % 8;
        int sx = rand() % w;
        int sy = rand() % h;
        for (int y = 0; y < bh && by + y < h && sy + y < h; y++)
            for (int x = 0; x < bw && bx + x < w && sx + x < w; x++)
                p[(by + y) * w + bx + x] = p[(sy + y) * w + sx + x];
    }

    /* Scanline shift */
    int num_shifts = 2 + rand() % 4;
    for (int i = 0; i < num_shifts; i++) {
        int y = rand() % h;
        int shift = (rand() % 20) - 10;
        if (shift > 0 && shift < w)
            memmove(p + y * w + shift, p + y * w, (size_t)(w - shift) * 4);
    }

    /* RGB channel swap on random rows */
    if ((rand() & 3) == 0) {
        int y = rand() % h;
        int rows = 1 + rand() % 3;
        for (int r = 0; r < rows && y + r < h; r++) {
            for (int x = 0; x < w; x++) {
                uint32_t c = p[(y + r) * w + x];
                p[(y + r) * w + x] = RB_RGB(RB_B(c), RB_R(c), RB_G(c));
            }
        }
    }
}

/* ─── Effect: Pixelate ─── */

static void effect_pixelate(void) {
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;
    int block = 6;

    for (int by = 0; by < h; by += block) {
        for (int bx = 0; bx < w; bx += block) {
            int r = 0, g = 0, b = 0, cnt = 0;
            for (int y = by; y < by + block && y < h; y++)
                for (int x = bx; x < bx + block && x < w; x++) {
                    uint32_t c = p[y * w + x];
                    r += RB_R(c); g += RB_G(c); b += RB_B(c); cnt++;
                }
            if (cnt == 0) continue;
            uint32_t avg = RB_RGB(r / cnt, g / cnt, b / cnt);
            for (int y = by; y < by + block && y < h; y++)
                for (int x = bx; x < bx + block && x < w; x++)
                    p[y * w + x] = avg;
        }
    }
}

/* ─── Effect: Invert ─── */

static void effect_invert(void) {
    int total = surface.width * surface.height;
    uint32_t *p = surface.back;
    for (int i = 0; i < total; i++) {
        uint32_t c = p[i];
        p[i] = RB_RGB(255 - RB_R(c), 255 - RB_G(c), 255 - RB_B(c));
    }
}

/* ─── Effect Dispatch ─── */

EMSCRIPTEN_KEEPALIVE
void editor_apply_effect(int id) {
    frame_counter++;
    switch (id) {
        case 0: break; /* none */
        case 1: effect_thermal(); break;
        case 2: effect_edge(); break;
        case 3: effect_nightvision(); break;
        case 4: effect_vhs(); break;
        case 5: effect_glitch(); break;
        case 6: effect_pixelate(); break;
        case 7: effect_invert(); break;
    }
}

/* ─── Text overlay (draw timecode etc on the frame) ─── */

EMSCRIPTEN_KEEPALIVE
void editor_draw_text(int x, int y, const char *str, int r, int g, int b) {
    rb_text(&surface, x, y, str, RB_RGB(r, g, b));
}

EMSCRIPTEN_KEEPALIVE
void editor_draw_rect(int x, int y, int w, int h, int r, int g, int b) {
    rb_rect(&surface, x, y, w, h, RB_RGB(r, g, b));
}

int main(void) { return 0; }
