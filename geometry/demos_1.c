/*
 * demos_1.c — Terrain, Organic, Architecture, Fluid demos
 *
 * 12 demo functions for the "Geometry Is Math" viewer.
 */
#include "geometry_math.h"

/* File-scope time for scene/color callbacks that can't receive extra params */
static float s_time;

/* ═══════════════════════════════════════════════════════════════════════════
 *  TERRAIN
 * ═══════════════════════════════════════════════════════════════════════════ */

void demo_heightfield(float t) {
    int W = g_W, H = g_H;

    for (int layer = 2; layer >= 0; layer--) {
        float speed = 0.3f * (1.0f - layer * 0.3f);
        int yOff = layer * 12;
        float haze = layer * 0.15f;

        for (int x = 0; x < W; x++) {
            float fx = (float)x / W * 6.0f + layer * 2.0f;
            float h = 0, amp = 0.5f;
            for (int o = 0; o < 6; o++) {
                h += gm_noise2(fx * (1 << o) + t * speed, 0.5f + o * 10.0f + layer * 50.0f) * amp;
                amp *= 0.5f;
            }
            int gy = (int)floorf(H * (0.45f + layer * 0.08f) - h * H * 0.35f);

            int ystart = gy + yOff;
            if (ystart < 0) ystart = 0;
            for (int y = ystart; y < H; y++) {
                if (layer > 0 && y < gy + yOff + 3) continue;
                float d = (float)(y - gy - yOff) / (float)(H - gy - yOff);
                float snow = (gy + yOff < (int)(H * 0.2f))
                    ? gm_smoothstep(0.0f, 3.0f, (float)(y - gy - yOff)) : 0.0f;
                int r = (int)(gm_lerp(80, 50, d) + snow * 170 + haze * 40);
                int g = (int)(gm_lerp(120, 70, d) + snow * 130 + haze * 30);
                int b = (int)(gm_lerp(55, 35, d) + snow * 140 + haze * 50);
                gm_pixel(x, y, r, g, b);
            }

            /* Sky behind the farthest layer */
            if (layer == 2) {
                int sky_end = gy + yOff;
                if (sky_end > H) sky_end = H;
                for (int y = 0; y < sky_end; y++) {
                    float s = (float)y / H;
                    gm_pixel(x, y, (int)(15 + s * 10), (int)(14 + s * 8), (int)(25 + s * 12));
                }
            }
        }
    }
    gm_label(5, 3, "H=F(X)", RB_RGB(200, 220, 180));
    gm_param(5, 14, "OCT", "6", RB_RGB(160, 180, 140));
}

void demo_erosion(float t) {
    int W = g_W, H = g_H;

    for (int x = 0; x < W; x++) {
        float fx = (float)x / W * 5.0f;
        float wx = gm_noise2(fx * 1.5f + t * 0.1f, 3.7f) * 0.4f;
        float h = 0, amp = 0.6f;
        for (int o = 0; o < 5; o++) {
            float n = gm_noise2((fx + wx) * (1 << o) + o * 10.0f, 1.5f + t * 0.05f);
            h += (1.0f - fabsf(n)) * amp;
            amp *= 0.45f;
        }
        int gy = (int)floorf(H * 0.5f - h * H * 0.25f);

        for (int y = 0; y < H; y++) {
            if (y < gy) {
                gm_pixel(x, y, 18, 17, 22);
            } else {
                float d = (float)(y - gy) / (float)(H - gy);
                gm_pixel(x, y,
                    (int)gm_lerp(140, 80, d),
                    (int)gm_lerp(100, 55, d),
                    (int)gm_lerp(70, 40, d));
            }
        }
    }
}

void demo_dunes(float t) {
    int W = g_W, H = g_H;

    for (int x = 0; x < W; x++) {
        float fx = (float)x / W * 8.0f;
        float h = sinf(fx * 2.0f + t * 0.5f) * 0.3f
                + sinf(fx * 5.0f - t * 0.65f) * 0.15f
                + sinf(fx * 11.0f + t * 0.35f) * 0.08f;
        h += gm_noise2(fx * 3.0f + t * 0.1f, 0.0f) * 0.05f;
        int gy = (int)floorf(H * 0.5f - h * H * 0.3f);

        for (int y = 0; y < H; y++) {
            if (y < gy) {
                float s = (float)y / H;
                gm_pixel(x, y,
                    (int)(40 + s * 30),
                    (int)(50 + s * 20),
                    (int)(80 + s * 10));
            } else {
                float d = (float)(y - gy) / (float)(H - gy);
                float shade = 0.8f + gm_noise2((float)x / W * 20.0f, (float)y / H * 10.0f) * 0.15f;
                gm_pixel(x, y,
                    (int)(gm_lerp(220, 180, d) * shade),
                    (int)(gm_lerp(190, 140, d) * shade),
                    (int)(gm_lerp(130, 90, d) * shade));
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ORGANIC
 * ═══════════════════════════════════════════════════════════════════════════ */

static float metaballs_scene(float x, float y, float z) {
    float d = 999.0f;
    for (int i = 0; i < 4; i++) {
        float a = s_time * 0.8f + i * 1.57f;
        float sx = x - sinf(a) * 1.5f;
        float sy = y - cosf(a * 0.7f) * 0.8f;
        float sz = z - cosf(a) * 1.5f;
        d = gm_op_smooth_union(d, gm_sd_sphere(sx, sy, sz, 0.6f), 0.5f);
    }
    return gm_op_union(d, y + 1.5f);
}

static void metaballs_color(float px, float py, float pz,
                             float nx, float ny, float nz,
                             float light, float dist, float spec,
                             int *r, int *g, int *b) {
    if (py < -1.49f) {
        int ch = (((int)floorf(px * 2) + (int)floorf(pz * 2)) & 1) ? 153 : 102;
        float f = ch / 255.0f;
        *r = (int)(light * f * 200);
        *g = (int)(light * f * 180);
        *b = (int)(light * f * 160);
        return;
    }
    float hval = py * 0.2f + atan2f(nx, nz) / 6.2832f * 0.3f + s_time * 0.08f;
    uint8_t hr, hg, hb;
    gm_hue(hval, &hr, &hg, &hb);
    float wet = spec * 0.7f;
    *r = (int)(hr / 255.0f * light * 255 + wet * 255);
    *g = (int)(hg / 255.0f * light * 255 + wet * 240);
    *b = (int)(hb / 255.0f * light * 255 + wet * 220);
}

void demo_metaballs(float t) {
    s_time = t;
    gm_render_opts opts = gm_default_opts;
    opts.spec_pow = 32;
    opts.spec_str = 0.5f;
    gm_render3d(t, metaballs_scene, 5.0f, metaballs_color, &opts);
}

void demo_voronoi_cells(float t) {
    int W = g_W, H = g_H;

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float x = (float)px / H * 4.0f + t * 0.15f;
            float y = (float)py / H * 4.0f;
            float d, id;
            gm_voronoi(x, y, &d, &id);
            float edge = gm_voronoi_edge(x, y);
            uint8_t hr, hg, hb;
            gm_hue(id, &hr, &hg, &hb);
            float e = gm_smoothstep(0.02f, 0.08f, edge);
            gm_pixel(px, py,
                (int)(hr / 255.0f * 0.4f * e * 255 + 20),
                (int)(hg / 255.0f * 0.4f * e * 255 + 18),
                (int)(hb / 255.0f * 0.3f * e * 255 + 16));
        }
    }
}

static float coral_branch(float px, float py, float cx, float cy,
                           float r, float angle, int depth, float t) {
    float dx = px - cx, dy = py - cy;
    float d = sqrtf(dx * dx + dy * dy) - r;
    if (depth <= 0 || r < 2.0f) return d;

    float a1 = angle + 0.45f + sinf(t + depth) * 0.1f;
    float a2 = angle - 0.45f - sinf(t * 1.1f + depth) * 0.1f;
    float len = r * 2.5f;
    float nx1 = cx + sinf(a1) * len, ny1 = cy - cosf(a1) * len;
    float nx2 = cx + sinf(a2) * len, ny2 = cy - cosf(a2) * len;
    float d1 = coral_branch(px, py, nx1, ny1, r * 0.62f, a1, depth - 1, t);
    float d2 = coral_branch(px, py, nx2, ny2, r * 0.62f, a2, depth - 1, t);
    d = fminf(d, d1);
    d = fminf(d, d2);
    return d;
}

void demo_coral(float t) {
    int W = g_W, H = g_H;

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float d = coral_branch((float)px, (float)py,
                                    W / 2.0f, H * 0.85f, 12.0f, 0.0f, 5, t);
            if (d < 0.0f) {
                gm_pixel(px, py, 200, 120, 100);
            } else {
                float g = expf(-d * 0.08f);
                gm_pixel(px, py,
                    (int)(g * 80 + 15),
                    (int)(g * 40 + 14),
                    (int)(g * 50 + 12));
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ARCHITECTURE
 * ═══════════════════════════════════════════════════════════════════════════ */

static float pillars_scene(float x, float y, float z) {
    float floor_d = y + 1.5f;
    float rx = gm_op_repeat(x, 3.0f);
    float rz = gm_op_repeat(z, 3.0f);
    float pillar = gm_sd_box3(rx, y, rz, 0.25f, 1.5f, 0.25f);
    return gm_op_union(floor_d, pillar);
}

static void pillars_color(float px, float py, float pz,
                           float nx, float ny, float nz,
                           float light, float dist, float spec,
                           int *r, int *g, int *b) {
    if (py < -1.49f) {
        int check = ((int)floorf(px + 50) + (int)floorf(pz + 50)) & 1;
        float ch = check ? 0.6f : 0.45f;
        *r = (int)(light * ch * 230);
        *g = (int)(light * ch * 210);
        *b = (int)(light * ch * 180);
        return;
    }
    float stone = gm_noise2(px * 5.0f, pz * 5.0f) * 0.08f;
    *r = (int)(light * (200 + stone * 60) + spec * 120);
    *g = (int)(light * (185 + stone * 40) + spec * 100);
    *b = (int)(light * (160 + stone * 20) + spec * 80);
}

void demo_pillars(float t) {
    s_time = t;
    gm_render_opts opts = gm_default_opts;
    opts.shadow = 1;
    opts.spec_pow = 24;
    opts.spec_str = 0.2f;
    gm_render3d(t, pillars_scene, 6.0f, pillars_color, &opts);
}

void demo_arch(float t) {
    int W = g_W, H = g_H;

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float x = ((float)px / W - 0.5f) * 3.0f;
            float y = (0.5f - (float)py / H) * 3.0f;
            float w = 0.8f, rad = w * 1.2f;

            float ax1 = x + w * 0.3f, ax2 = x - w * 0.3f;
            float d1 = sqrtf(ax1 * ax1 + y * y) - rad;
            float d2 = sqrtf(ax2 * ax2 + y * y) - rad;
            float d = fmaxf(d1, d2);
            d = fmaxf(d, -y - 0.8f);
            d = fmaxf(d, y - 1.2f);
            float thick = fabsf(d) - 0.06f;
            float contour = sinf(d * 40.0f + t) * 0.5f + 0.5f;

            if (thick < 0.0f) {
                float stone = gm_noise2(px * 0.15f, py * 0.15f) * 0.12f
                            + gm_noise2(px * 0.4f, py * 0.4f) * 0.06f;
                float depth = 0.85f + fabsf(d) * 3.0f;
                gm_pixel(px, py,
                    (int)((190 + stone * 100) * depth),
                    (int)((175 + stone * 80) * depth),
                    (int)((155 + stone * 60) * depth));
            } else {
                float g = expf(-fmaxf(0.0f, thick) * 8.0f);
                gm_pixel(px, py,
                    (int)(g * 80 + contour * 25 + 15),
                    (int)(g * 60 + contour * 20 + 14),
                    (int)(g * 50 + contour * 15 + 12));
            }
        }
    }
    gm_label(5, 3, "2 ARCS", RB_RGB(200, 180, 140));
}

static float staircase_scene(float x, float y, float z) {
    float col = sqrtf(x * x + z * z) - 0.12f;
    float r = sqrtf(x * x + z * z);
    float a = atan2f(z, x);
    float spiralY = a / 6.2832f * 2.0f;
    float dy = y - spiralY;
    float mdy = dy - roundf(dy / 2.0f) * 2.0f;
    float step = fmaxf(fabsf(mdy) - 0.05f, fmaxf(r - 1.2f, 0.2f - r));
    return fminf(col, step);
}

static void staircase_color(float px, float py, float pz,
                              float nx, float ny, float nz,
                              float light, float dist, float spec,
                              int *r, int *g, int *b) {
    float rad = sqrtf(px * px + pz * pz);
    if (rad < 0.15f) {
        *r = (int)(light * 100 + spec * 80);
        *g = (int)(light * 90 + spec * 70);
        *b = (int)(light * 85 + spec * 60);
    } else {
        *r = (int)(light * 200 + spec * 120);
        *g = (int)(light * 185 + spec * 100);
        *b = (int)(light * 165 + spec * 80);
    }
}

void demo_staircase(float t) {
    s_time = t;
    gm_render_opts opts = gm_default_opts;
    opts.spec_pow = 20;
    opts.spec_str = 0.25f;
    gm_render3d(t, staircase_scene, 4.0f, staircase_color, &opts);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  FLUID
 * ═══════════════════════════════════════════════════════════════════════════ */

void demo_waves(float t) {
    int W = g_W, H = g_H;
    gm_clear(15, 20, 35);

    /* Gerstner wave parameters: amplitude, wavenumber, speed */
    static const float waves[][3] = {
        {0.25f, 1.5f,  1.2f},
        {0.15f, 2.5f, -0.8f},
        {0.10f, 4.0f,  1.5f},
        {0.08f, 6.0f, -1.1f},
        {0.05f, 9.0f,  0.7f}
    };

    for (int x = 0; x < W; x++) {
        float fx = (float)x / W * 8.0f;
        float h = 0;
        for (int i = 0; i < 5; i++) {
            h += waves[i][0] * cosf(fx * waves[i][1] - t * waves[i][2]);
        }
        int gy = (int)floorf(H * 0.45f - h * H * 0.25f);

        for (int y = gy; y < H; y++) {
            if (y < 0) continue;
            float d = (float)(y - gy) / (float)(H - gy);
            int foam = (d < 0.02f) ? 1 : 0;
            gm_pixel(x, y,
                (int)(30 + foam * 200 + d * 5),
                (int)(60 + foam * 180 - d * 20),
                (int)(120 + foam * 130 - d * 40));
        }
    }
}

void demo_vortex(float t) {
    int W = g_W, H = g_H;
    float e = 0.05f;

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float x = (float)px / H * 4.0f;
            float y = (float)py / H * 4.0f;
            float n0 = gm_fbm(x + t * 0.2f, y, 3);
            float dx = gm_fbm(x + e + t * 0.2f, y, 3) - n0;
            float dy = gm_fbm(x + t * 0.2f, y + e, 3) - n0;
            float vx = dy / e, vy = -dx / e;
            float mag = sqrtf(vx * vx + vy * vy);

            float hval = atan2f(vy, vx) / 6.28f + 0.5f;
            uint8_t hr, hg, hb;
            gm_hue(hval, &hr, &hg, &hb);
            float br = fminf(1.0f, mag * 0.3f);
            gm_pixel(px, py,
                (int)(hr / 255.0f * br * 255 + 15),
                (int)(hg / 255.0f * br * 255 + 14),
                (int)(hb / 255.0f * br * 255 + 12));
        }
    }
}

typedef struct {
    float x, y, birth;
} ripple_drop_t;

void demo_ripples(float t) {
    int W = g_W, H = g_H;
    static ripple_drop_t drops[8];
    static int init = 0;

    if (!init) {
        for (int i = 0; i < 8; i++) {
            drops[i].x = gm_ihash(i * 17, i * 31) * W;
            drops[i].y = gm_ihash(i * 53, i * 79) * H;
            drops[i].birth = -gm_ihash(i * 13, i * 41) * 4.0f;
        }
        init = 1;
    }

    /* Reset drops that have aged out */
    for (int i = 0; i < 8; i++) {
        if (t - drops[i].birth > 4.0f) {
            drops[i].x = gm_ihash((int)(t * 100) + i * 17, i * 31 + (int)(t * 37)) * W;
            drops[i].y = gm_ihash(i * 53 + (int)(t * 59), (int)(t * 100) + i * 79) * H;
            drops[i].birth = t;
        }
    }

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float val = 0;
            for (int i = 0; i < 8; i++) {
                float dx = (float)px - drops[i].x;
                float dy = (float)py - drops[i].y;
                float r = sqrtf(dx * dx + dy * dy);
                float age = t - drops[i].birth;
                if (age < 0) continue;
                val += cosf(r * 0.3f - age * 5.0f) * expf(-age * 0.8f) * expf(-r * 0.02f);
            }
            /* Map wave intensity to blue/cyan water */
            float bright = val * 0.5f + 0.5f;
            gm_pixel(px, py,
                (int)(15 + bright * 30),
                (int)(30 + bright * 60),
                (int)(80 + bright * 120));
        }
    }
}
