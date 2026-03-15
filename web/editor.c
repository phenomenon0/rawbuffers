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

/* Chroma key state */
static int chroma_key_r = 0, chroma_key_g = 255, chroma_key_b = 0;
static int chroma_threshold = 100;
static uint32_t chroma_bg_color = 0xFF404040;

/* Channel split state */
static int channel_split_mode = 0; /* 0=horizontal, 1=vertical */
static int channel_split_offset = 4;

/* Audio-reactive state */
static float audio_bass = 0, audio_mid = 0, audio_high = 0;
static int audio_beat = 0;
static float audio_waveform[256];

/* ML state */
static uint8_t *ml_mask = NULL;
static int ml_mask_size = 0;
static uint32_t ml_bg_color = 0xFF404040;

/* Detection state (max 20 boxes) */
static int det_count = 0;
static struct { int x, y, w, h; char label[32]; } det_boxes[20];

/* Pose state (33 landmarks from MediaPipe) */
static float pose_x[33], pose_y[33];
static int pose_valid = 0;

/* Face state */
static int face_x, face_y, face_w, face_h;
static float face_landmarks[12]; /* 6 landmarks × 2 coords */
static int face_valid = 0;

/* Depth state */
static uint8_t *depth_map = NULL;
static int depth_map_size = 0;

/* PiP state */
static uint32_t *pip_buffer = NULL;
static int pip_w = 0, pip_h = 0;

/* Annotation state */
static uint32_t *annotation_buffer = NULL;
static int annotation_size = 0;

/* Subtitle state */
static char subtitle_text[256] = "";
static int subtitle_timer = 0;
static int subtitle_decay_frames = 90;  /* 3s at 30fps */

/* Effect parameters */
static int thermal_palette = 0; /* 0=classic, 1=arctic, 2=lava */
static int edge_threshold = 128;
static int edge_mono = 0;
static float nv_gain = 1.5f;
static int nv_noise = 8;
static int vhs_aberration = 2;
static int vhs_noise_freq = 30; /* out of 60: rand()&0x3F < vhs_noise_freq means noise */
static int glitch_intensity = 5;
static int glitch_block_size = 30;
static int pixel_block = 6;
static int film_dust = 8;
static int film_scratch_freq = 4; /* out of 15: rand()%15 < film_scratch_freq */
static float film_flicker = 0.1f; /* flicker range 0-1 */
static float fft_bass_gain = 1.0f, fft_mid_gain = 1.0f, fft_high_gain = 1.0f;
static int wave_color_r = 0, wave_color_g = 255, wave_color_b = 100;
static int wave_position = 0; /* 0=bottom, 1=center, 2=top */
static int pose_bone_r=0, pose_bone_g=255, pose_bone_b=200;
static int pose_joint_r=255, pose_joint_g=100, pose_joint_b=100;
static int face_mode = 0; /* 0=pixelate, 1=blur, 2=mask */
static int face_block = 8;
static int fog_r=180, fog_g=190, fog_b=210;
static float fog_intensity = 0.8f;
static int annot_brush_size = 2;
static int subtitle_font_scale = 1; /* 1=normal, 2=medium, 3=large */

/* Adjustment state */
static int adj_brightness = 0; /* -100 to 100 */
static int adj_contrast = 0;   /* -100 to 100 */
static int adj_saturation = 0; /* -100 to 100 */
static int adj_hue = 0;        /* 0 to 360 */
static int flip_h = 0, flip_v = 0;

/* BG removal modes */
static int bg_mode = 0;        /* 0=solid, 1=blur, 2=image, 3=video */
static int bg_blur_radius = 20;
static uint32_t *bg_image_buffer = NULL;

/* Face landmarks expanded (478 points) */
static float face_landmarks_x[478], face_landmarks_y[478];
static int face_landmark_count = 0;

/* PiP position/opacity */
static int pip_pos_x = -1, pip_pos_y = -1;  /* -1 = auto bottom-right */
static float pip_opacity = 1.0f;

/* Blend modes */
static int blend_mode = 0; /* 0=normal, 1=multiply, 2=screen, 3=overlay, 4=soft-light */

/* Hair color effect */
static uint32_t hair_color = 0xFF8B00FF; /* default purple */
static float hair_intensity = 0.5f;

/* Body outline effect */
static uint32_t outline_color = 0xFF00FF00;
static int outline_thickness = 2;

/* Tilt-shift effect */
static int tiltshift_focus = 50;   /* 0-100 */
static int tiltshift_range = 20;   /* how wide the sharp band */

/* Face mask buffer */
static uint8_t *face_mask_buffer = NULL;
static int face_mask_size = 0;

/* VJ blend state */
static uint32_t *saved_buffer = NULL;
static int saved_buffer_size = 0;

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
int editor_reinit(int w, int h) {
    if (surface.front) free(surface.front);
    if (surface.back) free(surface.back);
    if (saved_buffer) { free(saved_buffer); saved_buffer = NULL; saved_buffer_size = 0; }
    return editor_init(w, h);
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
        if (thermal_palette == 1) {
            /* Arctic: white -> cyan -> blue -> darkblue */
            if (t < 0.33f)
                heat = rb_lerp_color(RB_RGB(0,0,60), RB_RGB(0,0,200), t * 3.0f);
            else if (t < 0.66f)
                heat = rb_lerp_color(RB_RGB(0,0,200), RB_RGB(0,200,220), (t - 0.33f) * 3.0f);
            else
                heat = rb_lerp_color(RB_RGB(0,200,220), RB_RGB(255,255,255), (t - 0.66f) * 3.0f);
        } else if (thermal_palette == 2) {
            /* Lava: black -> red -> orange -> yellow */
            if (t < 0.33f)
                heat = rb_lerp_color(RB_RGB(0,0,0), RB_RGB(200,0,0), t * 3.0f);
            else if (t < 0.66f)
                heat = rb_lerp_color(RB_RGB(200,0,0), RB_RGB(255,160,0), (t - 0.33f) * 3.0f);
            else
                heat = rb_lerp_color(RB_RGB(255,160,0), RB_RGB(255,255,0), (t - 0.66f) * 3.0f);
        } else {
            /* Classic: blue -> red -> yellow -> white */
            if (t < 0.25f)
                heat = rb_lerp_color(RB_RGB(0,0,40), RB_RGB(0,0,200), t * 4.0f);
            else if (t < 0.5f)
                heat = rb_lerp_color(RB_RGB(0,0,200), RB_RGB(200,0,0), (t - 0.25f) * 4.0f);
            else if (t < 0.75f)
                heat = rb_lerp_color(RB_RGB(200,0,0), RB_RGB(255,255,0), (t - 0.5f) * 4.0f);
            else
                heat = rb_lerp_color(RB_RGB(255,255,0), RB_RGB(255,255,255), (t - 0.75f) * 4.0f);
        }
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
            /* Threshold the magnitude */
            if (mag < edge_threshold) mag = 0;
            if (edge_mono) {
                /* White-on-black */
                tmp[y * w + x] = RB_RGB(mag, mag, mag);
            } else {
                /* Color edges by angle */
                float angle = atan2f((float)gy, (float)gx) / 6.2831853f + 0.5f;
                uint32_t color = rb_hue(angle);
                tmp[y * w + x] = rb_lerp_color(RB_RGB(0, 0, 0), color, mag / 255.0f);
            }
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
            int g = (int)(lum * nv_gain); if (g > 255) g = 255;
            int rb_val = lum / 4;
            /* Noise speckle */
            if ((rand() & 0xFF) < nv_noise) { g += 30; if (g > 255) g = 255; }
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
            int rl = x - vhs_aberration >= 0 ? x - vhs_aberration : 0;
            int br = x + vhs_aberration < w ? x + vhs_aberration : w - 1;
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
    if (rand() % 60 < vhs_noise_freq) {
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
    int num_blocks = glitch_intensity;
    for (int b = 0; b < num_blocks; b++) {
        int bx = rand() % w;
        int by = rand() % h;
        int bw = 10 + rand() % glitch_block_size;
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
    int block = pixel_block;

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

/* ─── Effect: Chroma Key ─── */

static void effect_chroma_key(void) {
    int total = surface.width * surface.height;
    uint32_t *p = surface.back;
    int thr = chroma_threshold;
    for (int i = 0; i < total; i++) {
        uint32_t c = p[i];
        int r = RB_R(c), g = RB_G(c), b = RB_B(c);
        int dr = r - chroma_key_r, dg = g - chroma_key_g, db = b - chroma_key_b;
        int dist = dr * dr + dg * dg + db * db;
        if (dist < thr * thr)
            p[i] = chroma_bg_color;
    }
}

/* ─── Effect: Channel Split ─── */

static void effect_channel_split(void) {
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;
    int off = channel_split_offset;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int rl, br;
            if (channel_split_mode == 0) { /* horizontal */
                rl = x - off >= 0 ? x - off : 0;
                br = x + off < w ? x + off : w - 1;
                uint32_t cl = p[y * w + rl];
                uint32_t cr = p[y * w + br];
                uint32_t cc = p[y * w + x];
                p[y * w + x] = RB_RGB(RB_R(cl), RB_G(cc), RB_B(cr));
            } else { /* vertical */
                rl = y - off >= 0 ? y - off : 0;
                br = y + off < h ? y + off : h - 1;
                uint32_t cl = p[rl * w + x];
                uint32_t cr = p[br * w + x];
                uint32_t cc = p[y * w + x];
                p[y * w + x] = RB_RGB(RB_R(cl), RB_G(cc), RB_B(cr));
            }
        }
    }
}

/* ─── Effect: Old Film ─── */

static void effect_old_film(void) {
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;
    int cx = w / 2, cy = h / 2;
    float max_r = sqrtf((float)(cx * cx + cy * cy));
    /* Brightness flicker */
    int flicker_range = (int)(film_flicker * 100);
    if (flicker_range < 0) flicker_range = 0;
    float flicker = (1.0f - film_flicker) + (float)(rand() % (flicker_range + 1)) / 100.0f;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t c = p[y * w + x];
            int lum = (RB_R(c) * 77 + RB_G(c) * 150 + RB_B(c) * 29) >> 8;
            /* Sepia tint */
            int sr = (int)(lum * 1.2f); if (sr > 255) sr = 255;
            int sg = (int)(lum * 1.0f);
            int sb = (int)(lum * 0.8f);
            /* Vignette */
            float dx = (float)(x - cx), dy = (float)(y - cy);
            float dist = sqrtf(dx * dx + dy * dy) / max_r;
            float vig = 1.0f - dist * dist * 1.2f;
            if (vig < 0) vig = 0;
            /* Apply flicker + vignette */
            sr = (int)(sr * vig * flicker);
            sg = (int)(sg * vig * flicker);
            sb = (int)(sb * vig * flicker);
            if (sr > 255) sr = 255;
            if (sg > 255) sg = 255;
            if (sb > 255) sb = 255;
            p[y * w + x] = RB_RGB(sr, sg, sb);
        }
    }
    /* Random dust specks */
    int num_specks = film_dust + rand() % (film_dust / 2 + 1);
    for (int i = 0; i < num_specks; i++) {
        int sx = rand() % w, sy = rand() % h;
        int sz = 1 + rand() % 3;
        uint32_t dust = (rand() & 1) ? RB_RGB(200, 190, 170) : RB_RGB(40, 35, 30);
        for (int dy2 = 0; dy2 < sz && sy + dy2 < h; dy2++)
            for (int dx2 = 0; dx2 < sz && sx + dx2 < w; dx2++)
                p[(sy + dy2) * w + (sx + dx2)] = dust;
    }
    /* Vertical scratches */
    if (rand() % 15 < film_scratch_freq) {
        int sx = rand() % w;
        uint32_t scratch_c = RB_RGB(180, 170, 150);
        for (int y2 = 0; y2 < h; y2++)
            p[y2 * w + sx] = scratch_c;
    }
}

/* ─── Effect: Time Freeze ─── */

static void effect_time_freeze(void) {
    /* No-op: JS skips buffer write when this effect is active */
}

/* ─── Effect: FFT Color Grade ─── */

static void effect_fft_color_grade(void) {
    int total = surface.width * surface.height;
    uint32_t *p = surface.back;
    /* bass → red tint, mid → saturation boost, high → brightness */
    float red_add = (audio_bass * fft_bass_gain) * 80.0f;
    float sat_mult = 1.0f + (audio_mid * fft_mid_gain) * 0.5f;
    float bright = 1.0f + (audio_high * fft_high_gain) * 0.3f;
    for (int i = 0; i < total; i++) {
        uint32_t c = p[i];
        float r = RB_R(c), g = RB_G(c), b = RB_B(c);
        /* Add red tint from bass */
        r += red_add;
        /* Simple saturation boost: push away from gray */
        float gray = (r + g + b) / 3.0f;
        r = gray + (r - gray) * sat_mult;
        g = gray + (g - gray) * sat_mult;
        b = gray + (b - gray) * sat_mult;
        /* Brightness */
        r *= bright; g *= bright; b *= bright;
        /* Clamp */
        int ri = (int)r; if (ri < 0) ri = 0; if (ri > 255) ri = 255;
        int gi = (int)g; if (gi < 0) gi = 0; if (gi > 255) gi = 255;
        int bi = (int)b; if (bi < 0) bi = 0; if (bi > 255) bi = 255;
        p[i] = RB_RGB(ri, gi, bi);
    }
}

/* ─── Effect: Beat Glitch ─── */

static void effect_beat_glitch(void) {
    if (!audio_beat) return;
    /* Reuse glitch effect with extra intensity */
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;
    int intensity = 5 + (int)(audio_bass * 10);
    for (int b = 0; b < intensity; b++) {
        int bx = rand() % w, by = rand() % h;
        int bw = 20 + rand() % 60, bh = 2 + rand() % 12;
        int sx = rand() % w, sy = rand() % h;
        for (int y = 0; y < bh && by + y < h && sy + y < h; y++)
            for (int x = 0; x < bw && bx + x < w && sx + x < w; x++)
                p[(by + y) * w + bx + x] = p[(sy + y) * w + sx + x];
    }
    /* Color channel corruption */
    int num_rows = 3 + rand() % 6;
    for (int i = 0; i < num_rows; i++) {
        int y = rand() % h;
        for (int x = 0; x < w; x++) {
            uint32_t c = p[y * w + x];
            p[y * w + x] = RB_RGB(RB_B(c), RB_R(c), RB_G(c));
        }
    }
}

/* ─── Effect: Waveform Overlay ─── */

static void effect_waveform_overlay(void) {
    int w = surface.width, h = surface.height;
    int wave_h = h / 4;
    int base_y;
    if (wave_position == 1)
        base_y = h / 2 - wave_h / 2;
    else if (wave_position == 2)
        base_y = h / 6;
    else
        base_y = h * 2 / 3;
    uint32_t color = RB_RGB(wave_color_r, wave_color_g, wave_color_b);
    for (int i = 1; i < 256 && i < w; i++) {
        int x0 = (i - 1) * w / 256;
        int x1 = i * w / 256;
        int y0 = base_y + (int)(audio_waveform[i - 1] * wave_h);
        int y1 = base_y + (int)(audio_waveform[i] * wave_h);
        /* Clamp */
        if (y0 < 0) y0 = 0; if (y0 >= h) y0 = h - 1;
        if (y1 < 0) y1 = 0; if (y1 >= h) y1 = h - 1;
        rb_line(&surface, x0, y0, x1, y1, color);
    }
}

/* ─── Effect: BG Removal ─── */

static void effect_bg_removal(void) {
    if (!ml_mask) return;
    int w = surface.width, h = surface.height;
    int total = w * h;
    uint32_t *p = surface.back;
    int limit = total < ml_mask_size ? total : ml_mask_size;

    if (bg_mode == 1) {
        /* Blur mode: box blur entire frame, composite person over blur */
        uint32_t *blurred = (uint32_t *)malloc(total * sizeof(uint32_t));
        memcpy(blurred, p, total * sizeof(uint32_t));
        int rad = bg_blur_radius;
        /* Horizontal pass */
        for (int y = 0; y < h; y++) {
            int rsum = 0, gsum = 0, bsum = 0;
            for (int x = 0; x < rad && x < w; x++) {
                uint32_t c = p[y * w + x];
                rsum += RB_R(c); gsum += RB_G(c); bsum += RB_B(c);
            }
            for (int x = 0; x < w; x++) {
                int x2 = x + rad;
                if (x2 < w) { uint32_t c = p[y * w + x2]; rsum += RB_R(c); gsum += RB_G(c); bsum += RB_B(c); }
                int x0 = x - rad - 1;
                if (x0 >= 0) { uint32_t c = p[y * w + x0]; rsum -= RB_R(c); gsum -= RB_G(c); bsum -= RB_B(c); }
                int cnt = (x2 < w ? x2 : w - 1) - (x0 >= 0 ? x0 : -1);
                if (cnt <= 0) cnt = 1;
                blurred[y * w + x] = RB_RGB(rsum / cnt, gsum / cnt, bsum / cnt);
            }
        }
        /* Vertical pass on blurred */
        uint32_t *blurred2 = (uint32_t *)malloc(total * sizeof(uint32_t));
        for (int x = 0; x < w; x++) {
            int rsum = 0, gsum = 0, bsum = 0;
            for (int y = 0; y < rad && y < h; y++) {
                uint32_t c = blurred[y * w + x];
                rsum += RB_R(c); gsum += RB_G(c); bsum += RB_B(c);
            }
            for (int y = 0; y < h; y++) {
                int y2 = y + rad;
                if (y2 < h) { uint32_t c = blurred[y2 * w + x]; rsum += RB_R(c); gsum += RB_G(c); bsum += RB_B(c); }
                int y0 = y - rad - 1;
                if (y0 >= 0) { uint32_t c = blurred[y0 * w + x]; rsum -= RB_R(c); gsum -= RB_G(c); bsum -= RB_B(c); }
                int cnt = (y2 < h ? y2 : h - 1) - (y0 >= 0 ? y0 : -1);
                if (cnt <= 0) cnt = 1;
                blurred2[y * w + x] = RB_RGB(rsum / cnt, gsum / cnt, bsum / cnt);
            }
        }
        /* Composite: person over blur */
        for (int i = 0; i < limit; i++) {
            if (ml_mask[i] == 0) /* background */
                p[i] = blurred2[i];
        }
        free(blurred);
        free(blurred2);
    } else if (bg_mode == 2 || bg_mode == 3) {
        /* Image/Video BG mode */
        if (bg_image_buffer) {
            for (int i = 0; i < limit; i++) {
                if (ml_mask[i] == 0)
                    p[i] = bg_image_buffer[i];
            }
        } else {
            for (int i = 0; i < limit; i++) {
                if (ml_mask[i] == 0)
                    p[i] = ml_bg_color;
            }
        }
    } else {
        /* Solid color (mode 0, default) */
        for (int i = 0; i < limit; i++) {
            if (ml_mask[i] == 0)
                p[i] = ml_bg_color;
        }
    }

    /* Edge feathering: blend pixels near person/BG boundary */
    {
        int feather_rad = 2;
        for (int y = feather_rad; y < h - feather_rad; y++) {
            for (int x = feather_rad; x < w - feather_rad; x++) {
                int idx = y * w + x;
                if (idx >= limit) break;
                int cur = ml_mask[idx] > 0 ? 1 : 0;
                int bg_count = 0;
                for (int dy = -feather_rad; dy <= feather_rad; dy++) {
                    for (int dx = -feather_rad; dx <= feather_rad; dx++) {
                        int ni = (y + dy) * w + (x + dx);
                        if (ni >= 0 && ni < limit) {
                            int nb = ml_mask[ni] > 0 ? 1 : 0;
                            if (nb != cur) bg_count++;
                        }
                    }
                }
                if (bg_count > 0 && cur == 1) {
                    /* Person pixel near BG edge: soften toward BG */
                    float blend = (float)bg_count / (float)((feather_rad * 2 + 1) * (feather_rad * 2 + 1));
                    /* Get the BG color that would be here */
                    uint32_t bg_px;
                    if (bg_mode == 1) {
                        bg_px = p[idx]; /* already composited, approximate */
                    } else if ((bg_mode == 2 || bg_mode == 3) && bg_image_buffer) {
                        bg_px = bg_image_buffer[idx];
                    } else {
                        bg_px = ml_bg_color;
                    }
                    p[idx] = rb_lerp_color(p[idx], bg_px, blend * 0.5f);
                }
            }
        }
    }
}

/* ─── Effect: Object Detect ─── */

static void effect_object_detect(void) {
    for (int i = 0; i < det_count && i < 20; i++) {
        int x = det_boxes[i].x, y = det_boxes[i].y;
        int bw = det_boxes[i].w, bh = det_boxes[i].h;
        uint32_t color = rb_hue((float)i / 20.0f);
        rb_rect(&surface, x, y, bw, bh, color);
        rb_rect(&surface, x+1, y+1, bw-2, bh-2, color); /* thicker */
        rb_text(&surface, x + 2, y - 10 > 0 ? y - 10 : y + 2, det_boxes[i].label, color);
    }
}

/* ─── Effect: Pose Skeleton ─── */

/* COCO skeleton pairs (17 keypoints) */
static const int skeleton_pairs[][2] = {
    {0,1},{0,2},{1,3},{2,4}, /* head */
    {5,6},{5,7},{7,9},{6,8},{8,10}, /* arms */
    {5,11},{6,12},{11,12}, /* torso */
    {11,13},{13,15},{12,14},{14,16} /* legs */
};
#define NUM_SKELETON_PAIRS 16

static void effect_pose_skeleton(void) {
    if (!pose_valid) return;
    int w = surface.width, h = surface.height;
    uint32_t bone_color = RB_RGB(pose_bone_r, pose_bone_g, pose_bone_b);
    uint32_t joint_color = RB_RGB(pose_joint_r, pose_joint_g, pose_joint_b);
    /* Draw bones */
    for (int i = 0; i < NUM_SKELETON_PAIRS; i++) {
        int a = skeleton_pairs[i][0], b2 = skeleton_pairs[i][1];
        if (a >= 33 || b2 >= 33) continue;
        int x0 = (int)(pose_x[a] * w), y0 = (int)(pose_y[a] * h);
        int x1 = (int)(pose_x[b2] * w), y1 = (int)(pose_y[b2] * h);
        if (x0 <= 0 || y0 <= 0 || x1 <= 0 || y1 <= 0) continue;
        rb_line(&surface, x0, y0, x1, y1, bone_color);
    }
    /* Draw joints */
    for (int i = 0; i < 17; i++) {
        int x = (int)(pose_x[i] * w), y = (int)(pose_y[i] * h);
        if (x <= 0 || y <= 0) continue;
        rb_circle_fill(&surface, x, y, 4, joint_color);
    }
}

/* MediaPipe FaceMesh tessellation edges (subset of ~470 most important edges) */
static const int face_mesh_edges[][2] = {
    {10,338},{338,297},{297,332},{332,284},{284,251},{251,389},{389,356},{356,454},{454,323},{323,361},
    {361,288},{288,397},{397,365},{365,379},{379,378},{378,400},{400,377},{377,152},{152,148},{148,176},
    {176,149},{149,150},{150,136},{136,172},{172,58},{58,132},{132,93},{93,234},{234,127},{127,162},
    {162,21},{21,54},{54,103},{103,67},{67,109},{109,10},
    {67,69},{69,104},{104,68},{68,71},{71,139},{139,111},{111,49},{49,3},{3,196},{196,236},{236,198},
    {198,456},{456,420},{420,399},{399,330},{330,348},{348,329},{329,293},{293,298},{298,301},
    {109,108},{108,151},{151,337},{337,10},
    {21,162},{162,127},{127,234},{234,93},{93,132},{132,58},{58,172},{172,136},{136,150},{150,149},
    {149,176},{176,148},{148,152},{152,377},{377,400},{400,378},{378,379},{379,365},{365,397},{397,288},
    {288,361},{361,323},{323,454},{454,356},{356,389},{389,251},{251,284},{284,332},{332,297},{297,338},
    {338,10},
    {33,7},{7,163},{163,144},{144,145},{145,153},{153,154},{154,155},{155,133},
    {33,246},{246,161},{161,160},{160,159},{159,158},{158,157},{157,173},{173,133},
    {263,249},{249,390},{390,373},{373,374},{374,380},{380,381},{381,382},{382,362},
    {263,466},{466,388},{388,387},{387,386},{386,385},{385,384},{384,398},{398,362},
    {78,191},{191,80},{80,81},{81,82},{82,13},{13,312},{312,311},{311,310},{310,415},{415,308},
    {78,95},{95,88},{88,178},{178,87},{87,14},{14,317},{317,402},{402,318},{318,324},{324,308},
    {61,146},{146,91},{91,181},{181,84},{84,17},{17,314},{314,405},{405,321},{321,375},{375,291},
    {61,185},{185,40},{40,39},{39,37},{37,0},{0,267},{267,269},{269,270},{270,409},{409,291},
    {48,131},{131,134},{134,51},{51,5},{5,281},{281,363},{363,360},{360,279},{279,278},
    {48,115},{115,220},{220,45},{45,4},{4,275},{275,440},{440,344},{344,278},
    {2,326},{326,328},{328,97},{97,99},{99,60},{60,75},{75,59},{59,166},{166,79},{79,239},
    {239,238},{238,20},{20,242},{242,141},{141,94},{94,370},{370,462},{462,250},{250,458},
    {458,459},{459,309},{309,392},{392,289},{289,305},{305,290},{290,2}
};
#define NUM_FACE_MESH_EDGES (sizeof(face_mesh_edges) / sizeof(face_mesh_edges[0]))

/* ─── Effect: Face Filter ─── */

static void effect_face_filter(void) {
    if (!face_valid) return;
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;
    int block = face_block;
    int fx1 = face_x, fy1 = face_y;
    int fx2 = face_x + face_w, fy2 = face_y + face_h;
    if (fx1 < 0) fx1 = 0; if (fy1 < 0) fy1 = 0;
    if (fx2 > w) fx2 = w; if (fy2 > h) fy2 = h;

    if (face_mode == 3) {
        /* Wireframe mesh */
        uint32_t mesh_color = RB_RGB(0, 255, 200);
        for (int e = 0; e < (int)NUM_FACE_MESH_EDGES; e++) {
            int a = face_mesh_edges[e][0], b2 = face_mesh_edges[e][1];
            if (a >= face_landmark_count || b2 >= face_landmark_count) continue;
            int x0 = (int)(face_landmarks_x[a] * w);
            int y0 = (int)(face_landmarks_y[a] * h);
            int x1 = (int)(face_landmarks_x[b2] * w);
            int y1 = (int)(face_landmarks_y[b2] * h);
            if (x0 <= 0 || y0 <= 0 || x1 <= 0 || y1 <= 0) continue;
            rb_line(&surface, x0, y0, x1, y1, mesh_color);
        }
        return;
    }

    if (face_mode == 4) {
        /* AR: Sunglasses — draw rectangles at eye landmarks */
        /* Left eye: landmarks 33 (outer) and 133 (inner), right eye: 362 (outer) and 263 (inner) */
        if (face_landmark_count >= 400) {
            int lx1 = (int)(face_landmarks_x[33] * w), ly1 = (int)(face_landmarks_y[33] * h);
            int lx2 = (int)(face_landmarks_x[133] * w), ly2 = (int)(face_landmarks_y[133] * h);
            int rx1 = (int)(face_landmarks_x[263] * w), ry1 = (int)(face_landmarks_y[263] * h);
            int rx2 = (int)(face_landmarks_x[362] * w), ry2 = (int)(face_landmarks_y[362] * h);
            int pad = 4;
            /* Left lens */
            int llx = (lx1 < lx2 ? lx1 : lx2) - pad;
            int lly = ((ly1 + ly2) / 2) - 12;
            int llw = abs(lx2 - lx1) + pad * 2;
            int llh = 24;
            rb_rect_fill(&surface, llx, lly, llw, llh, RB_RGB(20, 20, 20));
            rb_rect(&surface, llx, lly, llw, llh, RB_RGB(60, 60, 60));
            /* Right lens */
            int rlx = (rx1 < rx2 ? rx1 : rx2) - pad;
            int rly = ((ry1 + ry2) / 2) - 12;
            int rlw = abs(rx2 - rx1) + pad * 2;
            int rlh = 24;
            rb_rect_fill(&surface, rlx, rly, rlw, rlh, RB_RGB(20, 20, 20));
            rb_rect(&surface, rlx, rly, rlw, rlh, RB_RGB(60, 60, 60));
            /* Bridge */
            rb_line(&surface, llx + llw, lly + llh / 2, rlx, rly + rlh / 2, RB_RGB(60, 60, 60));
        }
        return;
    }

    if (face_mode == 5) {
        /* AR: Mustache — draw arcs below nose landmark */
        if (face_landmark_count >= 400) {
            /* Nose tip: landmark 1, upper lip center: landmark 0 */
            int nx = (int)(face_landmarks_x[1] * w);
            int ny = (int)(face_landmarks_y[1] * h);
            uint32_t mcolor = RB_RGB(60, 30, 10);
            /* Draw a simple mustache shape using filled rects */
            for (int dx = -20; dx <= 20; dx++) {
                int dy = (dx * dx) / 40 + 2; /* parabolic shape */
                int px = nx + dx, py = ny + dy;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    for (int t = 0; t < 3; t++) {
                        if (py + t < h)
                            p[(py + t) * w + px] = mcolor;
                    }
                }
            }
        }
        return;
    }

    /* Modes 6-16: JS overlay handles these generative art modes */
    if (face_mode >= 6 && face_mode <= 16) return;

    if (face_mode == 17) {
        /* Contour Blur: blur only within face_mask (sent from JS) */
        if (face_mask_buffer && face_mask_size > 0) {
            int total = w * h;
            int lim = total < face_mask_size ? total : face_mask_size;
            /* Box blur face region */
            uint32_t *blurred = (uint32_t *)malloc(total * sizeof(uint32_t));
            memcpy(blurred, p, total * sizeof(uint32_t));
            int rad = face_block > 2 ? face_block : 4;
            for (int y2 = rad; y2 < h - rad; y2++) {
                for (int x2 = rad; x2 < w - rad; x2++) {
                    int idx = y2 * w + x2;
                    if (idx >= lim || !face_mask_buffer[idx]) continue;
                    int rsum=0, gsum=0, bsum=0, cnt=0;
                    for (int dy=-rad; dy<=rad; dy++) {
                        for (int dx=-rad; dx<=rad; dx++) {
                            int ni = (y2+dy)*w + (x2+dx);
                            if (ni >= 0 && ni < total) {
                                uint32_t c = p[ni];
                                rsum += RB_R(c); gsum += RB_G(c); bsum += RB_B(c); cnt++;
                            }
                        }
                    }
                    if (cnt > 0) blurred[idx] = RB_RGB(rsum/cnt, gsum/cnt, bsum/cnt);
                }
            }
            for (int i = 0; i < lim; i++) {
                if (face_mask_buffer[i]) p[i] = blurred[i];
            }
            free(blurred);
        }
        return;
    }

    if (face_mode == 18) {
        /* Spotlight: darken everything, restore face region */
        int total = w * h;
        int lim = total < (face_mask_buffer ? face_mask_size : 0) ? total : (face_mask_buffer ? face_mask_size : 0);
        for (int i = 0; i < total; i++) {
            uint32_t c = p[i];
            if (i < lim && face_mask_buffer && face_mask_buffer[i]) {
                /* Keep original brightness */
            } else {
                /* Darken by 60% */
                int r = RB_R(c) * 40 / 100;
                int g = RB_G(c) * 40 / 100;
                int b = RB_B(c) * 40 / 100;
                p[i] = RB_RGB(r, g, b);
            }
        }
        return;
    }

    if (face_mode == 19) {
        /* Face Color Grade: brightness/contrast on face region only */
        if (face_mask_buffer && face_mask_size > 0) {
            int total = w * h;
            int lim = total < face_mask_size ? total : face_mask_size;
            for (int i = 0; i < lim; i++) {
                if (!face_mask_buffer[i]) continue;
                uint32_t c = p[i];
                /* Brighten + warm the face */
                int r = RB_R(c) + 15; if (r > 255) r = 255;
                int g = RB_G(c) + 10; if (g > 255) g = 255;
                int b = RB_B(c) + 5;  if (b > 255) b = 255;
                /* Slight contrast boost */
                r = (int)((r - 128) * 1.1f + 128); if (r < 0) r = 0; if (r > 255) r = 255;
                g = (int)((g - 128) * 1.1f + 128); if (g < 0) g = 0; if (g > 255) g = 255;
                b = (int)((b - 128) * 1.1f + 128); if (b < 0) b = 0; if (b > 255) b = 255;
                p[i] = RB_RGB(r, g, b);
            }
        }
        return;
    }

    if (face_mode == 20) {
        /* Beauty Filter: bilateral blur approximation on face skin */
        if (ml_mask && face_mask_buffer) {
            int total = w * h;
            int lim = total < face_mask_size ? total : face_mask_size;
            uint32_t *tmp2 = (uint32_t *)malloc(total * sizeof(uint32_t));
            memcpy(tmp2, p, total * sizeof(uint32_t));
            int rad = 3;
            for (int y2 = rad; y2 < h - rad; y2++) {
                for (int x2 = rad; x2 < w - rad; x2++) {
                    int idx = y2 * w + x2;
                    if (idx >= lim || !face_mask_buffer[idx]) continue;
                    /* Bilateral-like blur: only average similar-colored neighbors */
                    uint32_t center = p[idx];
                    int cr = RB_R(center), cg = RB_G(center), cb = RB_B(center);
                    float rsum = 0, gsum = 0, bsum = 0, wsum = 0;
                    for (int dy = -rad; dy <= rad; dy++) {
                        for (int dx = -rad; dx <= rad; dx++) {
                            int ni = (y2+dy)*w + (x2+dx);
                            if (ni < 0 || ni >= total) continue;
                            uint32_t nc = p[ni];
                            int nr = RB_R(nc), ng = RB_G(nc), nb = RB_B(nc);
                            int diff = abs(nr-cr) + abs(ng-cg) + abs(nb-cb);
                            float weight = (diff < 60) ? 1.0f : 0.1f;
                            rsum += nr * weight;
                            gsum += ng * weight;
                            bsum += nb * weight;
                            wsum += weight;
                        }
                    }
                    if (wsum > 0) {
                        tmp2[idx] = RB_RGB((int)(rsum/wsum), (int)(gsum/wsum), (int)(bsum/wsum));
                    }
                }
            }
            for (int i = 0; i < lim; i++) {
                if (face_mask_buffer[i]) p[i] = tmp2[i];
            }
            free(tmp2);
        }
        return;
    }

    if (face_mode == 21) {
        /* Anime Eyes: barrel distortion to enlarge eyes */
        if (face_landmark_count >= 400) {
            /* Process each eye */
            int eye_centers[2][2];
            /* Left eye center: avg of landmarks 362, 263 */
            eye_centers[0][0] = (int)((face_landmarks_x[362] + face_landmarks_x[263]) / 2.0f * w);
            eye_centers[0][1] = (int)((face_landmarks_y[362] + face_landmarks_y[263]) / 2.0f * h);
            /* Right eye center: avg of landmarks 33, 133 */
            eye_centers[1][0] = (int)((face_landmarks_x[33] + face_landmarks_x[133]) / 2.0f * w);
            eye_centers[1][1] = (int)((face_landmarks_y[33] + face_landmarks_y[133]) / 2.0f * h);

            uint32_t *tmp = (uint32_t *)malloc(w * h * sizeof(uint32_t));
            memcpy(tmp, p, w * h * sizeof(uint32_t));

            float magnify = 1.3f;
            int eye_radius = face_w / 4; /* approximate eye region radius */
            if (eye_radius < 10) eye_radius = 10;

            for (int e = 0; e < 2; e++) {
                int cx = eye_centers[e][0], cy = eye_centers[e][1];
                for (int dy = -eye_radius; dy <= eye_radius; dy++) {
                    for (int dx = -eye_radius; dx <= eye_radius; dx++) {
                        int px = cx + dx, py = cy + dy;
                        if (px < 0 || px >= w || py < 0 || py >= h) continue;
                        float dist = sqrtf((float)(dx * dx + dy * dy));
                        if (dist > eye_radius) continue;
                        /* Barrel distortion: map from output to input */
                        float norm_dist = dist / eye_radius;
                        float distorted = norm_dist / magnify;
                        if (distorted > 1.0f) continue;
                        float angle = atan2f((float)dy, (float)dx);
                        int sx = cx + (int)(distorted * eye_radius * cosf(angle));
                        int sy = cy + (int)(distorted * eye_radius * sinf(angle));
                        if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
                            p[py * w + px] = tmp[sy * w + sx];
                        }
                    }
                }
            }
            free(tmp);
        }
        return;
    }

    if (face_mode == 1) {
        /* Blur: box blur the face region */
        for (int by = fy1; by < fy2; by += block) {
            for (int bx = fx1; bx < fx2; bx += block) {
                int r = 0, g = 0, b = 0, cnt = 0;
                for (int y = by; y < by + block && y < fy2; y++)
                    for (int x = bx; x < bx + block && x < fx2; x++) {
                        uint32_t c = p[y * w + x];
                        r += RB_R(c); g += RB_G(c); b += RB_B(c); cnt++;
                    }
                if (cnt == 0) continue;
                uint32_t avg = RB_RGB(r / cnt, g / cnt, b / cnt);
                for (int y = by; y < by + block && y < fy2; y++)
                    for (int x = bx; x < bx + block && x < fx2; x++)
                        p[y * w + x] = avg;
            }
        }
    } else if (face_mode == 2) {
        /* Mask: solid color fill */
        uint32_t mask_color = RB_RGB(128, 128, 128);
        for (int y = fy1; y < fy2; y++)
            for (int x = fx1; x < fx2; x++)
                p[y * w + x] = mask_color;
    } else {
        /* Pixelate (default, mode 0) */
        for (int by = fy1; by < fy2; by += block) {
            for (int bx = fx1; bx < fx2; bx += block) {
                int r = 0, g = 0, b = 0, cnt = 0;
                for (int y = by; y < by + block && y < fy2; y++)
                    for (int x = bx; x < bx + block && x < fx2; x++) {
                        uint32_t c = p[y * w + x];
                        r += RB_R(c); g += RB_G(c); b += RB_B(c); cnt++;
                    }
                if (cnt == 0) continue;
                uint32_t avg = RB_RGB(r / cnt, g / cnt, b / cnt);
                for (int y = by; y < by + block && y < fy2; y++)
                    for (int x = bx; x < bx + block && x < fx2; x++)
                        p[y * w + x] = avg;
            }
        }
    }

    /* Draw face box (only for basic modes) */
    if (face_mode <= 3) {
        rb_rect(&surface, face_x, face_y, face_w, face_h, RB_RGB(255, 200, 0));
        /* Draw key landmarks if we have expanded data */
        if (face_landmark_count > 6) {
            /* Draw a few key landmarks as dots */
            int key_pts[] = {1, 33, 133, 263, 362, 61, 291};
            for (int k = 0; k < 7; k++) {
                int idx = key_pts[k];
                if (idx < face_landmark_count) {
                    int lx = (int)(face_landmarks_x[idx] * w);
                    int ly = (int)(face_landmarks_y[idx] * h);
                    if (lx > 0 && ly > 0)
                        rb_circle_fill(&surface, lx, ly, 2, RB_RGB(255, 0, 100));
                }
            }
        } else {
            /* Legacy 6-landmark drawing */
            for (int i = 0; i < 6; i++) {
                int lx = (int)face_landmarks[i * 2];
                int ly = (int)face_landmarks[i * 2 + 1];
                if (lx > 0 && ly > 0)
                    rb_circle_fill(&surface, lx, ly, 3, RB_RGB(255, 0, 100));
            }
        }
    }
}

/* ─── Effect: Depth Fog ─── */

static void effect_depth_fog(void) {
    if (!depth_map) return;
    int total = surface.width * surface.height;
    uint32_t *p = surface.back;
    uint32_t fog_color = RB_RGB(fog_r, fog_g, fog_b);
    int limit = total < depth_map_size ? total : depth_map_size;
    for (int i = 0; i < limit; i++) {
        float depth = depth_map[i] / 255.0f;
        /* More depth = more fog */
        p[i] = rb_lerp_color(p[i], fog_color, depth * fog_intensity);
    }
}

/* ─── Effect: Colorization (JS-driven) ─── */

static void effect_colorization(void) {
    /* Histogram equalization per channel */
    int w = surface.width, h = surface.height;
    int total = w * h;
    uint32_t *p = surface.back;
    int hist_r[256] = {0}, hist_g[256] = {0}, hist_b[256] = {0};

    /* Build histograms */
    for (int i = 0; i < total; i++) {
        uint32_t c = p[i];
        hist_r[RB_R(c)]++;
        hist_g[RB_G(c)]++;
        hist_b[RB_B(c)]++;
    }

    /* Compute CDFs */
    int cdf_r[256], cdf_g[256], cdf_b[256];
    cdf_r[0] = hist_r[0]; cdf_g[0] = hist_g[0]; cdf_b[0] = hist_b[0];
    for (int i = 1; i < 256; i++) {
        cdf_r[i] = cdf_r[i-1] + hist_r[i];
        cdf_g[i] = cdf_g[i-1] + hist_g[i];
        cdf_b[i] = cdf_b[i-1] + hist_b[i];
    }

    /* Find min CDFs */
    int min_r = 0, min_g = 0, min_b = 0;
    for (int i = 0; i < 256; i++) { if (cdf_r[i] > 0) { min_r = cdf_r[i]; break; } }
    for (int i = 0; i < 256; i++) { if (cdf_g[i] > 0) { min_g = cdf_g[i]; break; } }
    for (int i = 0; i < 256; i++) { if (cdf_b[i] > 0) { min_b = cdf_b[i]; break; } }

    /* Build lookup tables */
    uint8_t lut_r[256], lut_g[256], lut_b[256];
    int denom = total - 1;
    if (denom <= 0) denom = 1;
    for (int i = 0; i < 256; i++) {
        int vr = (total > min_r) ? (cdf_r[i] - min_r) * 255 / (total - min_r) : i;
        int vg = (total > min_g) ? (cdf_g[i] - min_g) * 255 / (total - min_g) : i;
        int vb = (total > min_b) ? (cdf_b[i] - min_b) * 255 / (total - min_b) : i;
        lut_r[i] = vr < 0 ? 0 : (vr > 255 ? 255 : vr);
        lut_g[i] = vg < 0 ? 0 : (vg > 255 ? 255 : vg);
        lut_b[i] = vb < 0 ? 0 : (vb > 255 ? 255 : vb);
    }

    /* Apply equalization */
    for (int i = 0; i < total; i++) {
        uint32_t c = p[i];
        p[i] = RB_RGB(lut_r[RB_R(c)], lut_g[RB_G(c)], lut_b[RB_B(c)]);
    }
}

/* ─── Effect: Hand Gesture (JS-driven) ─── */

static void effect_hand_gesture(void) {
    /* JS maps gestures to controls — no C drawing */
}

/* ─── Effect: PiP ─── */

static void effect_pip(void) {
    if (!pip_buffer || pip_w <= 0 || pip_h <= 0) return;
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;
    int dx, dy;
    if (pip_pos_x >= 0 && pip_pos_y >= 0) {
        dx = pip_pos_x;
        dy = pip_pos_y;
    } else {
        dx = w - pip_w - 10;
        dy = h - pip_h - 10;
    }
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    for (int y = 0; y < pip_h && dy + y < h; y++)
        for (int x = 0; x < pip_w && dx + x < w; x++) {
            if (pip_opacity >= 0.99f) {
                p[(dy + y) * w + (dx + x)] = pip_buffer[y * pip_w + x];
            } else {
                p[(dy + y) * w + (dx + x)] = rb_lerp_color(
                    p[(dy + y) * w + (dx + x)],
                    pip_buffer[y * pip_w + x],
                    pip_opacity
                );
            }
        }
    /* Draw border */
    rb_rect(&surface, dx - 1, dy - 1, pip_w + 2, pip_h + 2, RB_RGB(255, 255, 255));
}

/* ─── Effect: Webcam + Screen ─── */

static void effect_webcam_screen(void) {
    /* Same composite as PiP */
    effect_pip();
}

/* ─── Effect: VJ Tool (JS-driven) ─── */

static void effect_vj_tool(void) {
    /* JS chains multiple effects — this is a no-op, JS calls editor_apply_effect multiple times */
}

/* ─── Effect: Annotator ─── */

static void effect_annotator(void) {
    if (!annotation_buffer) return;
    int total = surface.width * surface.height;
    uint32_t *p = surface.back;
    int limit = total < annotation_size ? total : annotation_size;
    for (int i = 0; i < limit; i++) {
        if (annotation_buffer[i] & 0xFF000000) /* has alpha/content */
            p[i] = annotation_buffer[i];
    }
}

/* ─── Effect: Live Subtitles ─── */

static void effect_subtitles(void) {
    if (subtitle_text[0] == '\0') return;
    if (subtitle_timer > 0) subtitle_timer--;
    if (subtitle_timer == 0) { subtitle_text[0] = '\0'; return; }
    int w = surface.width, h = surface.height;
    int bar_h = 30 * subtitle_font_scale;
    /* Draw background bar at bottom */
    rb_rect_fill(&surface, 0, h - bar_h, w, bar_h, RB_RGB(0, 0, 0));
    /* Center text roughly */
    int text_len = (int)strlen(subtitle_text);
    int text_w = text_len * 6; /* approximate char width */
    int tx = (w - text_w) / 2;
    if (tx < 4) tx = 4;
    rb_text(&surface, tx, h - bar_h + (bar_h - 8) / 2, subtitle_text, RB_RGB(255, 255, 255));
}

/* ─── Exported setters ─── */

EMSCRIPTEN_KEEPALIVE
void editor_set_chroma(int kr, int kg, int kb, int threshold, int br, int bg, int bb) {
    chroma_key_r = kr;
    chroma_key_g = kg;
    chroma_key_b = kb;
    chroma_threshold = threshold;
    chroma_bg_color = RB_RGB(br, bg, bb);
}

EMSCRIPTEN_KEEPALIVE
void editor_set_channel_split(int mode, int offset) {
    channel_split_mode = mode;
    channel_split_offset = offset;
}

EMSCRIPTEN_KEEPALIVE
void editor_set_audio_data(float bass, float mid, float high, int beat) {
    audio_bass = bass;
    audio_mid = mid;
    audio_high = high;
    audio_beat = beat;
}

EMSCRIPTEN_KEEPALIVE
int editor_get_waveform_buffer(void) {
    return (int)audio_waveform;
}

EMSCRIPTEN_KEEPALIVE
int editor_get_mask_buffer(void) {
    int total = surface.width * surface.height;
    if (!ml_mask || ml_mask_size != total) {
        free(ml_mask);
        ml_mask = (uint8_t *)calloc(total, 1);
        ml_mask_size = total;
    }
    return (int)ml_mask;
}

EMSCRIPTEN_KEEPALIVE
void editor_set_ml_bg_color(int r, int g, int b) {
    ml_bg_color = RB_RGB(r, g, b);
}

EMSCRIPTEN_KEEPALIVE
void editor_set_det_box(int idx, int x, int y, int w, int h, const char *label) {
    if (idx < 0 || idx >= 20) return;
    det_boxes[idx].x = x;
    det_boxes[idx].y = y;
    det_boxes[idx].w = w;
    det_boxes[idx].h = h;
    strncpy(det_boxes[idx].label, label, 31);
    det_boxes[idx].label[31] = '\0';
}

EMSCRIPTEN_KEEPALIVE
void editor_set_det_count(int n) {
    det_count = n > 20 ? 20 : n;
}

EMSCRIPTEN_KEEPALIVE
void editor_set_pose(int idx, float x, float y) {
    if (idx >= 0 && idx < 33) {
        pose_x[idx] = x;
        pose_y[idx] = y;
    }
}

EMSCRIPTEN_KEEPALIVE
void editor_set_pose_valid(int v) { pose_valid = v; }

EMSCRIPTEN_KEEPALIVE
void editor_set_face(int x, int y, int w, int h) {
    face_x = x; face_y = y; face_w = w; face_h = h;
}

EMSCRIPTEN_KEEPALIVE
void editor_set_face_landmarks(int idx, float x, float y) {
    if (idx >= 0 && idx < 6) {
        face_landmarks[idx * 2] = x;
        face_landmarks[idx * 2 + 1] = y;
    }
}

EMSCRIPTEN_KEEPALIVE
void editor_set_face_valid(int v) { face_valid = v; }

EMSCRIPTEN_KEEPALIVE
int editor_get_depth_buffer(void) {
    int total = surface.width * surface.height;
    if (!depth_map || depth_map_size != total) {
        free(depth_map);
        depth_map = (uint8_t *)calloc(total, 1);
        depth_map_size = total;
    }
    return (int)depth_map;
}

EMSCRIPTEN_KEEPALIVE
int editor_get_pip_buffer(int w, int h) {
    int total = w * h;
    if (!pip_buffer || pip_w != w || pip_h != h) {
        free(pip_buffer);
        pip_buffer = (uint32_t *)calloc(total, sizeof(uint32_t));
        pip_w = w;
        pip_h = h;
    }
    return (int)pip_buffer;
}

EMSCRIPTEN_KEEPALIVE
int editor_get_annotation_buffer(void) {
    int total = surface.width * surface.height;
    if (!annotation_buffer || annotation_size != total) {
        free(annotation_buffer);
        annotation_buffer = (uint32_t *)calloc(total, sizeof(uint32_t));
        annotation_size = total;
    }
    return (int)annotation_buffer;
}

EMSCRIPTEN_KEEPALIVE
void editor_annotation_clear(void) {
    if (annotation_buffer && annotation_size > 0)
        memset(annotation_buffer, 0, annotation_size * sizeof(uint32_t));
}

EMSCRIPTEN_KEEPALIVE
void editor_annotation_line(int x0, int y0, int x1, int y1, int r, int g, int b) {
    /* Draw to annotation overlay buffer using a simple line algorithm */
    int w = surface.width, h = surface.height;
    if (!annotation_buffer) return;
    uint32_t color = RB_RGB(r, g, b) | 0xFF000000;
    /* Bresenham */
    int dx2 = abs(x1 - x0), dy2 = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx2 - dy2;
    while (1) {
        if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h) {
            int bs = annot_brush_size;
            for (int dy = -bs/2; dy <= bs/2; dy++)
                for (int dx = -bs/2; dx <= bs/2; dx++) {
                    int px = x0 + dx, py = y0 + dy;
                    if (px >= 0 && px < w && py >= 0 && py < h)
                        annotation_buffer[py * w + px] = color;
                }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy2) { err -= dy2; x0 += sx; }
        if (e2 < dx2) { err += dx2; y0 += sy; }
    }
}

EMSCRIPTEN_KEEPALIVE
void editor_set_subtitle(const char *text) {
    strncpy(subtitle_text, text, 255);
    subtitle_text[255] = '\0';
    subtitle_timer = subtitle_decay_frames;
}

/* ─── Effect parameter setters ─── */

EMSCRIPTEN_KEEPALIVE
void editor_set_thermal(int palette) { thermal_palette = palette; }

EMSCRIPTEN_KEEPALIVE
void editor_set_edge(int thr, int mono) { edge_threshold = thr; edge_mono = mono; }

EMSCRIPTEN_KEEPALIVE
void editor_set_nightvision(float gain, int noise) { nv_gain = gain; nv_noise = noise; }

EMSCRIPTEN_KEEPALIVE
void editor_set_vhs(int aber, int noise_freq) { vhs_aberration = aber; vhs_noise_freq = noise_freq; }

EMSCRIPTEN_KEEPALIVE
void editor_set_glitch(int intensity, int block) { glitch_intensity = intensity; glitch_block_size = block; }

EMSCRIPTEN_KEEPALIVE
void editor_set_pixelate(int block) { pixel_block = block; }

EMSCRIPTEN_KEEPALIVE
void editor_set_old_film(int dust, int scratch, float flicker) { film_dust = dust; film_scratch_freq = scratch; film_flicker = flicker; }

EMSCRIPTEN_KEEPALIVE
void editor_set_fft_gains(float b, float m, float h) { fft_bass_gain = b; fft_mid_gain = m; fft_high_gain = h; }

EMSCRIPTEN_KEEPALIVE
void editor_set_waveform(int r, int g, int b, int pos) { wave_color_r = r; wave_color_g = g; wave_color_b = b; wave_position = pos; }

EMSCRIPTEN_KEEPALIVE
void editor_set_pose_colors(int br, int bg, int bb, int jr, int jg, int jb) { pose_bone_r=br; pose_bone_g=bg; pose_bone_b=bb; pose_joint_r=jr; pose_joint_g=jg; pose_joint_b=jb; }

EMSCRIPTEN_KEEPALIVE
void editor_set_face_params(int mode, int block) { face_mode = mode; face_block = block; }

EMSCRIPTEN_KEEPALIVE
void editor_set_fog(int r, int g, int b, float intensity) { fog_r=r; fog_g=g; fog_b=b; fog_intensity=intensity; }

EMSCRIPTEN_KEEPALIVE
void editor_set_annot_brush(int size) { annot_brush_size = size; }

EMSCRIPTEN_KEEPALIVE
void editor_set_subtitle_scale(int scale) { subtitle_font_scale = scale; }

/* ─── Adjustment setters ─── */

EMSCRIPTEN_KEEPALIVE
void editor_set_brightness(int val) { adj_brightness = val; }

EMSCRIPTEN_KEEPALIVE
void editor_set_contrast(int val) { adj_contrast = val; }

EMSCRIPTEN_KEEPALIVE
void editor_set_saturation(int val) { adj_saturation = val; }

EMSCRIPTEN_KEEPALIVE
void editor_set_hue_rotate(int deg) { adj_hue = deg; }

EMSCRIPTEN_KEEPALIVE
void editor_flip_h(void) { flip_h = !flip_h; }

EMSCRIPTEN_KEEPALIVE
void editor_flip_v(void) { flip_v = !flip_v; }

/* ─── Adjustment application ─── */

EMSCRIPTEN_KEEPALIVE
void editor_apply_adjustments(void) {
    if (!adj_brightness && !adj_contrast && !adj_saturation && !adj_hue && !flip_h && !flip_v) return;
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;
    int total = w * h;

    float bright_add = adj_brightness * 2.55f; /* map -100..100 to -255..255 */
    float cont_factor = (adj_contrast + 100) / 100.0f; /* 0..2 */
    cont_factor *= cont_factor; /* non-linear for better feel */
    float sat_factor = (adj_saturation + 100) / 100.0f;

    if (adj_brightness || adj_contrast || adj_saturation || adj_hue) {
        for (int i = 0; i < total; i++) {
            uint32_t c = p[i];
            float r = RB_R(c), g = RB_G(c), b = RB_B(c);

            /* Brightness */
            r += bright_add; g += bright_add; b += bright_add;

            /* Contrast (around 128) */
            r = (r - 128) * cont_factor + 128;
            g = (g - 128) * cont_factor + 128;
            b = (b - 128) * cont_factor + 128;

            /* Saturation */
            float gray = r * 0.299f + g * 0.587f + b * 0.114f;
            r = gray + (r - gray) * sat_factor;
            g = gray + (g - gray) * sat_factor;
            b = gray + (b - gray) * sat_factor;

            /* Hue rotation (simplified RGB rotation) */
            if (adj_hue) {
                float angle = adj_hue * 3.14159265f / 180.0f;
                float cosA = cosf(angle), sinA = sinf(angle);
                float nr = r * (cosA + (1-cosA)/3.0f) + g * ((1-cosA)/3.0f - sqrtf(1/3.0f)*sinA) + b * ((1-cosA)/3.0f + sqrtf(1/3.0f)*sinA);
                float ng = r * ((1-cosA)/3.0f + sqrtf(1/3.0f)*sinA) + g * (cosA + (1-cosA)/3.0f) + b * ((1-cosA)/3.0f - sqrtf(1/3.0f)*sinA);
                float nb = r * ((1-cosA)/3.0f - sqrtf(1/3.0f)*sinA) + g * ((1-cosA)/3.0f + sqrtf(1/3.0f)*sinA) + b * (cosA + (1-cosA)/3.0f);
                r = nr; g = ng; b = nb;
            }

            /* Clamp */
            int ri = (int)r; if (ri < 0) ri = 0; if (ri > 255) ri = 255;
            int gi = (int)g; if (gi < 0) gi = 0; if (gi > 255) gi = 255;
            int bi = (int)b; if (bi < 0) bi = 0; if (bi > 255) bi = 255;
            p[i] = RB_RGB(ri, gi, bi);
        }
    }

    /* Flip horizontal */
    if (flip_h) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w / 2; x++) {
                uint32_t tmp = p[y * w + x];
                p[y * w + x] = p[y * w + (w - 1 - x)];
                p[y * w + (w - 1 - x)] = tmp;
            }
        }
    }

    /* Flip vertical */
    if (flip_v) {
        for (int y = 0; y < h / 2; y++) {
            for (int x = 0; x < w; x++) {
                uint32_t tmp = p[y * w + x];
                p[y * w + x] = p[(h - 1 - y) * w + x];
                p[(h - 1 - y) * w + x] = tmp;
            }
        }
    }
}

/* ─── VJ buffer save/blend ─── */

EMSCRIPTEN_KEEPALIVE
void editor_save_buffer(void) {
    int total = surface.width * surface.height;
    if (!saved_buffer || saved_buffer_size != total) {
        free(saved_buffer);
        saved_buffer = (uint32_t *)calloc(total, sizeof(uint32_t));
        saved_buffer_size = total;
    }
    memcpy(saved_buffer, surface.back, total * sizeof(uint32_t));
}

static inline uint32_t blend_pixel(uint32_t base, uint32_t top, int mode) {
    int br = RB_R(base), bg2 = RB_G(base), bb = RB_B(base);
    int tr = RB_R(top), tg = RB_G(top), tb = RB_B(top);
    int rr, rg, rb_v;
    switch (mode) {
        case 1: /* Multiply */
            rr = br * tr / 255;
            rg = bg2 * tg / 255;
            rb_v = bb * tb / 255;
            break;
        case 2: /* Screen */
            rr = 255 - (255 - br) * (255 - tr) / 255;
            rg = 255 - (255 - bg2) * (255 - tg) / 255;
            rb_v = 255 - (255 - bb) * (255 - tb) / 255;
            break;
        case 3: /* Overlay */
            rr = br < 128 ? 2 * br * tr / 255 : 255 - 2 * (255 - br) * (255 - tr) / 255;
            rg = bg2 < 128 ? 2 * bg2 * tg / 255 : 255 - 2 * (255 - bg2) * (255 - tg) / 255;
            rb_v = bb < 128 ? 2 * bb * tb / 255 : 255 - 2 * (255 - bb) * (255 - tb) / 255;
            break;
        case 4: { /* Soft Light */
            float fr = tr / 255.0f, fg = tg / 255.0f, fb = tb / 255.0f;
            rr = (int)(br + (2 * fr - 1) * (br < 128 ? br : 255 - br) * (fr < 0.5f ? -1 : 1));
            rg = (int)(bg2 + (2 * fg - 1) * (bg2 < 128 ? bg2 : 255 - bg2) * (fg < 0.5f ? -1 : 1));
            rb_v = (int)(bb + (2 * fb - 1) * (bb < 128 ? bb : 255 - bb) * (fb < 0.5f ? -1 : 1));
            if (rr < 0) rr = 0; if (rr > 255) rr = 255;
            if (rg < 0) rg = 0; if (rg > 255) rg = 255;
            if (rb_v < 0) rb_v = 0; if (rb_v > 255) rb_v = 255;
            break;
        }
        default: /* Normal */
            return top;
    }
    return RB_RGB(rr, rg, rb_v);
}

EMSCRIPTEN_KEEPALIVE
void editor_blend_buffer(float intensity) {
    if (!saved_buffer) return;
    int total = surface.width * surface.height;
    uint32_t *p = surface.back;
    int limit = total < saved_buffer_size ? total : saved_buffer_size;
    for (int i = 0; i < limit; i++) {
        uint32_t blended = blend_mode > 0 ? blend_pixel(saved_buffer[i], p[i], blend_mode) : p[i];
        p[i] = rb_lerp_color(saved_buffer[i], blended, intensity);
    }
}

/* ─── Effect: Hair Color Change ─── */

static void effect_hair_color(void) {
    if (!ml_mask) return;
    int total = surface.width * surface.height;
    uint32_t *p = surface.back;
    int limit = total < ml_mask_size ? total : ml_mask_size;
    for (int i = 0; i < limit; i++) {
        if (ml_mask[i] == 1) /* hair class from multi-class segmenter */
            p[i] = rb_lerp_color(p[i], hair_color, hair_intensity);
    }
}

/* ─── Effect: Body Silhouette / Outline ─── */

static void effect_body_outline(void) {
    if (!ml_mask) return;
    int w = surface.width, h = surface.height;
    uint32_t *p = surface.back;
    int limit = w * h < ml_mask_size ? w * h : ml_mask_size;
    int thick = outline_thickness;

    /* Morphological erosion: erode mask by 1px to stabilize boundary */
    uint8_t *eroded = (uint8_t *)malloc(limit);
    memcpy(eroded, ml_mask, limit);
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int idx = y * w + x;
            if (idx >= limit) break;
            if (ml_mask[idx] > 0) {
                /* Check 4-connected neighbors */
                if (ml_mask[idx - 1] == 0 || ml_mask[idx + 1] == 0 ||
                    ml_mask[idx - w] == 0 || ml_mask[idx + w] == 0) {
                    eroded[idx] = 0;
                }
            }
        }
    }

    /* Edge detection on eroded mask */
    for (int y = thick; y < h - thick; y++) {
        for (int x = thick; x < w - thick; x++) {
            int idx = y * w + x;
            if (idx >= limit) break;
            int cur = eroded[idx] > 0 ? 1 : 0;
            int edge = 0;
            for (int dy = -thick; dy <= thick && !edge; dy++) {
                for (int dx = -thick; dx <= thick && !edge; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int ni = (y + dy) * w + (x + dx);
                    if (ni >= 0 && ni < limit) {
                        int nb = eroded[ni] > 0 ? 1 : 0;
                        if (nb != cur) edge = 1;
                    }
                }
            }
            if (edge) p[idx] = outline_color;
        }
    }
    free(eroded);
}

/* ─── Effect: Tilt-Shift (depth-based blur) ─── */

static void effect_tilt_shift(void) {
    if (!depth_map) return;
    int w = surface.width, h = surface.height;
    int total = w * h;
    uint32_t *p = surface.back;
    uint32_t *tmp = (uint32_t *)malloc(total * sizeof(uint32_t));
    memcpy(tmp, p, total * sizeof(uint32_t));

    float focus = tiltshift_focus / 100.0f;
    float range = tiltshift_range / 100.0f;
    float half_range = range / 2.0f;
    int limit = total < depth_map_size ? total : depth_map_size;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            if (idx >= limit) break;
            float d = depth_map[idx] / 255.0f;
            float dist_from_focus = fabsf(d - focus);
            if (dist_from_focus <= half_range) continue; /* in focus */
            float blur_amount = (dist_from_focus - half_range) / (1.0f - half_range + 0.001f);
            if (blur_amount > 1.0f) blur_amount = 1.0f;
            int rad = (int)(blur_amount * 6) + 1;
            /* Simple box blur at this pixel */
            int rsum = 0, gsum = 0, bsum = 0, cnt = 0;
            for (int dy = -rad; dy <= rad; dy++) {
                for (int dx = -rad; dx <= rad; dx++) {
                    int ny = y + dy, nx = x + dx;
                    if (ny >= 0 && ny < h && nx >= 0 && nx < w) {
                        uint32_t c = tmp[ny * w + nx];
                        rsum += RB_R(c); gsum += RB_G(c); bsum += RB_B(c); cnt++;
                    }
                }
            }
            if (cnt > 0)
                p[idx] = RB_RGB(rsum / cnt, gsum / cnt, bsum / cnt);
        }
    }
    free(tmp);
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
        case 8: effect_chroma_key(); break;
        case 9: effect_channel_split(); break;
        case 10: effect_old_film(); break;
        case 11: effect_time_freeze(); break;
        case 12: effect_fft_color_grade(); break;
        case 13: effect_beat_glitch(); break;
        case 14: effect_waveform_overlay(); break;
        case 15: effect_bg_removal(); break;
        case 16: effect_object_detect(); break;
        case 17: effect_pose_skeleton(); break;
        case 18: effect_face_filter(); break;
        case 19: effect_depth_fog(); break;
        case 20: effect_colorization(); break;
        case 21: effect_hand_gesture(); break;
        case 22: effect_pip(); break;
        case 23: effect_webcam_screen(); break;
        case 24: effect_vj_tool(); break;
        case 25: effect_annotator(); break;
        case 26: effect_subtitles(); break;
        case 27: effect_hair_color(); break;
        case 28: effect_body_outline(); break;
        case 29: effect_tilt_shift(); break;
    }
}

/* ─── New exported setters ─── */

EMSCRIPTEN_KEEPALIVE
void editor_set_bg_mode(int mode) { bg_mode = mode; }

EMSCRIPTEN_KEEPALIVE
void editor_set_bg_blur(int radius) { bg_blur_radius = radius; }

EMSCRIPTEN_KEEPALIVE
int editor_get_bg_image_buffer(void) {
    int total = surface.width * surface.height;
    if (!bg_image_buffer) {
        bg_image_buffer = (uint32_t *)calloc(total, sizeof(uint32_t));
    }
    return (int)bg_image_buffer;
}

EMSCRIPTEN_KEEPALIVE
void editor_set_face_landmark(int idx, float x, float y) {
    if (idx >= 0 && idx < 478) {
        face_landmarks_x[idx] = x;
        face_landmarks_y[idx] = y;
    }
}

EMSCRIPTEN_KEEPALIVE
void editor_set_face_landmark_count(int n) {
    face_landmark_count = n > 478 ? 478 : n;
}

EMSCRIPTEN_KEEPALIVE
void editor_set_pip_position(int x, int y) { pip_pos_x = x; pip_pos_y = y; }

EMSCRIPTEN_KEEPALIVE
void editor_set_pip_opacity(float op) { pip_opacity = op; }

EMSCRIPTEN_KEEPALIVE
void editor_set_blend_mode(int mode) { blend_mode = mode; }

EMSCRIPTEN_KEEPALIVE
void editor_set_hair_color(int r, int g, int b) { hair_color = RB_RGB(r, g, b); }

EMSCRIPTEN_KEEPALIVE
void editor_set_hair_intensity(float val) {
    hair_intensity = val < 0.1f ? 0.1f : (val > 1.0f ? 1.0f : val);
}

EMSCRIPTEN_KEEPALIVE
void editor_set_subtitle_decay(int frames) {
    if (frames > 0) subtitle_decay_frames = frames;
}

EMSCRIPTEN_KEEPALIVE
int editor_get_face_mask_buffer(void) {
    int total = surface.width * surface.height;
    if (!face_mask_buffer || face_mask_size != total) {
        free(face_mask_buffer);
        face_mask_buffer = (uint8_t *)calloc(total, 1);
        face_mask_size = total;
    }
    return (int)face_mask_buffer;
}

EMSCRIPTEN_KEEPALIVE
void editor_set_outline_color(int r, int g, int b) { outline_color = RB_RGB(r, g, b); }

EMSCRIPTEN_KEEPALIVE
void editor_set_outline_thickness(int t) { outline_thickness = t > 4 ? 4 : (t < 1 ? 1 : t); }

EMSCRIPTEN_KEEPALIVE
void editor_set_tiltshift(int focus, int range) { tiltshift_focus = focus; tiltshift_range = range; }

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
