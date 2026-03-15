/*
 * demos_3.c — Textile, Typography, Particle, Medical demos
 *
 * 12 demo functions for the "Geometry Is Math" viewer.
 */
#include "geometry_math.h"

/* File-scope time for scene/color callbacks that can't receive extra params */
static float s_time;

/* ═══════════════════════════════════════════════════════════════════════════
 *  TEXTILE
 * ═══════════════════════════════════════════════════════════════════════════ */

void demo_woven(float t) {
    int W = g_W, H = g_H;

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float sc = 12.0f;
            float x = px / sc + t * 0.5f, y = py / sc;
            int ix = (int)floorf(x), iy = (int)floorf(y);
            float fx = x - ix - 0.5f, fy = y - iy - 0.5f;
            int over = ((ix + iy) & 1) == 0;
            float warpD = fabsf(fx) - 0.35f;
            float weftD = fabsf(fy) - 0.35f;
            float lift = over ? sinf(fy * 3.14f) * 0.15f : -sinf(fx * 3.14f) * 0.15f;

            if (warpD < 0 && (over || weftD > 0)) {
                float shade = 0.6f + lift + gm_noise2((float)(ix * 7), (float)(iy * 3)) * 0.1f;
                gm_pixel(px, py, (int)(shade * 180), (int)(shade * 140), (int)(shade * 100));
            } else if (weftD < 0) {
                float shade = 0.55f - lift + gm_noise2((float)(ix * 3), (float)(iy * 7)) * 0.1f;
                gm_pixel(px, py, (int)(shade * 100), (int)(shade * 130), (int)(shade * 170));
            } else {
                gm_pixel(px, py, 20, 18, 16);
            }
        }
    }
    gm_label(5, 3, "WOVEN", RB_RGB(180, 160, 120));
}

void demo_moire(float t) {
    int W = g_W, H = g_H;
    float a1 = t * 0.1f, a2 = t * 0.1f + 0.15f;
    float c1 = cosf(a1), s1 = sinf(a1), c2 = cosf(a2), s2 = sinf(a2);

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float x = (px - W / 2.0f) / 5.0f, y = (py - H / 2.0f) / 5.0f;
            float r1x = x * c1 - y * s1;
            float r2x = x * c2 - y * s2;
            float g1 = sinf(r1x * 1.5f) * 0.5f + 0.5f;
            float g2 = sinf(r2x * 1.5f) * 0.5f + 0.5f;
            float v = g1 * g2;
            gm_pixel(px, py,
                (int)(v * 200 + 15),
                (int)(v * 180 + 14),
                (int)(v * 160 + 12));
        }
    }
    gm_label(5, 3, "MOIRE", RB_RGB(200, 190, 160));
}

/* --- Chainmail: interlocked tori on a grid --- */

static float chainmail_scene(float x, float y, float z) {
    float cell = 1.0f;
    float rx = gm_op_repeat(x, cell);
    float ry = y;
    float rz = gm_op_repeat(z, cell);
    int ix = (int)floorf(x / cell + 0.5f);
    int iz = (int)floorf(z / cell + 0.5f);

    float d;
    if ((ix + iz) & 1) {
        /* Torus aligned along X axis: swap x and z */
        d = gm_sd_torus(rz, ry, rx, 0.32f, 0.06f);
    } else {
        /* Torus aligned along Z axis: normal orientation */
        d = gm_sd_torus(rx, ry, rz, 0.32f, 0.06f);
    }
    return d;
}

static void chainmail_color(float px, float py, float pz,
                              float nx, float ny, float nz,
                              float light, float dist, float spec,
                              int *r, int *g, int *b) {
    /* Warm gold metallic */
    *r = (int)(light * 210 + spec * 200);
    *g = (int)(light * 185 + spec * 160);
    *b = (int)(light * 120 + spec * 80);
}

void demo_chainmail(float t) {
    s_time = t;
    gm_render_opts opts = gm_default_opts;
    opts.spec_pow = 40;
    opts.spec_str = 0.6f;
    gm_render3d(t, chainmail_scene, 4.0f, chainmail_color, &opts);
    gm_label(5, 3, "CHAINMAIL", RB_RGB(210, 185, 120));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TYPOGRAPHY
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Helper: min distance to letter "S" (5 segments) centered at ox */
static float letter_S(float x, float y, float ox) {
    float px = x - ox, py = y;
    float d = gm_sd_segment(px, py,  0.25f,  0.5f, -0.25f,  0.5f);  /* top bar */
    d = fminf(d, gm_sd_segment(px, py, -0.25f,  0.5f, -0.25f,  0.0f));  /* left upper */
    d = fminf(d, gm_sd_segment(px, py, -0.25f,  0.0f,  0.25f,  0.0f));  /* middle bar */
    d = fminf(d, gm_sd_segment(px, py,  0.25f,  0.0f,  0.25f, -0.5f));  /* right lower */
    d = fminf(d, gm_sd_segment(px, py,  0.25f, -0.5f, -0.25f, -0.5f));  /* bottom bar */
    return d;
}

/* Helper: min distance to letter "D" (5 segments) centered at ox */
static float letter_D(float x, float y, float ox) {
    float px = x - ox, py = y;
    float d = gm_sd_segment(px, py, -0.25f,  0.5f, -0.25f, -0.5f);  /* left vertical */
    d = fminf(d, gm_sd_segment(px, py, -0.25f,  0.5f,  0.15f,  0.35f));  /* top angle */
    d = fminf(d, gm_sd_segment(px, py,  0.15f,  0.35f,  0.25f,  0.0f));  /* right upper curve */
    d = fminf(d, gm_sd_segment(px, py,  0.25f,  0.0f,  0.15f, -0.35f));  /* right lower curve */
    d = fminf(d, gm_sd_segment(px, py,  0.15f, -0.35f, -0.25f, -0.5f));  /* bottom angle */
    return d;
}

/* Helper: min distance to letter "F" (3 segments) centered at ox */
static float letter_F(float x, float y, float ox) {
    float px = x - ox, py = y;
    float d = gm_sd_segment(px, py, -0.25f,  0.5f, -0.25f, -0.5f);  /* left vertical */
    d = fminf(d, gm_sd_segment(px, py, -0.25f,  0.5f,  0.25f,  0.5f));  /* top bar */
    d = fminf(d, gm_sd_segment(px, py, -0.25f,  0.05f,  0.15f,  0.05f));  /* middle bar */
    return d;
}

void demo_sdf_glow(float t) {
    int W = g_W, H = g_H;

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            /* Map pixel coords to [-2,1.25] x [-1.25,1.25] */
            float x = (float)px / W * 3.25f - 2.0f;
            float y = (0.5f - (float)py / H) * 2.5f;

            /* Compute SDF to all three letters */
            float d = letter_S(x, y, -1.2f);
            d = fminf(d, letter_D(x, y, -0.3f));
            d = fminf(d, letter_F(x, y, 0.6f));

            /* Subtract thickness */
            d -= 0.04f;

            int r, g, b;
            if (d < 0.0f) {
                /* Bright fill */
                float inner = gm_clamp(-d * 10.0f, 0.0f, 1.0f);
                r = (int)(200 + inner * 55);
                g = (int)(220 + inner * 35);
                b = (int)(255);
            } else {
                /* Exponential glow falloff with pulse */
                float pulse = 0.8f + 0.2f * sinf(t * 3.0f);
                float glow = expf(-d * 6.0f) * pulse;
                r = (int)(glow * 120);
                g = (int)(glow * 180);
                b = (int)(glow * 255);
            }
            /* Background */
            if (d >= 0.0f) {
                r += 12; g += 12; b += 15;
            }
            gm_pixel(px, py, r, g, b);
        }
    }
    gm_label(5, 3, "SDF GLOW", RB_RGB(160, 200, 255));
}

/* --- Neon: "HI" + circle with bloom --- */

/* Letter H segments */
static float letter_H(float x, float y, float ox) {
    float px = x - ox, py = y;
    float d = gm_sd_segment(px, py, -0.25f, 0.5f, -0.25f, -0.5f);  /* left */
    d = fminf(d, gm_sd_segment(px, py, 0.25f, 0.5f, 0.25f, -0.5f));  /* right */
    d = fminf(d, gm_sd_segment(px, py, -0.25f, 0.0f, 0.25f, 0.0f));  /* cross */
    return d;
}

/* Letter I segments */
static float letter_I(float x, float y, float ox) {
    float px = x - ox, py = y;
    float d = gm_sd_segment(px, py, 0.0f, 0.5f, 0.0f, -0.5f);  /* vertical */
    d = fminf(d, gm_sd_segment(px, py, -0.15f, 0.5f, 0.15f, 0.5f));  /* top serif */
    d = fminf(d, gm_sd_segment(px, py, -0.15f, -0.5f, 0.15f, -0.5f));  /* bottom serif */
    return d;
}

void demo_neon(float t) {
    int W = g_W, H = g_H;

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float x = ((float)px / W - 0.5f) * 3.0f;
            float y = (0.5f - (float)py / H) * 3.0f;

            /* Shape SDF: HI + circle */
            float d = letter_H(x, y, -0.6f);
            d = fminf(d, letter_I(x, y, 0.1f));
            d = fminf(d, fabsf(gm_sd_circle(x - 0.6f, y, 0.4f)));

            /* Organic flicker per section */
            float flicker = 0.85f + 0.15f * sinf(t * 8.0f + x * 2.0f)
                           + 0.1f * sinf(t * 13.0f + y * 3.0f);
            flicker = gm_clamp(flicker, 0.0f, 1.0f);

            /* Tube + glow */
            float tube = gm_smoothstep(0.06f, 0.03f, fabsf(d)) * flicker;
            float glow = expf(-fabsf(d) * 4.0f) * 0.5f * flicker;
            float v = tube + glow;

            /* Hot pink */
            gm_pixel(px, py,
                (int)(v * 255 + 8),
                (int)(v * 80 + 8),
                (int)(v * 140 + 10));
        }
    }
    gm_bloom(100, 5, 0.7f);
    gm_label(5, 3, "NEON", RB_RGB(255, 80, 140));
}

/* --- Liquid text: letters "FE" morphing with blobs --- */

/* Letter E segments */
static float letter_E(float x, float y, float ox) {
    float px = x - ox, py = y;
    float d = gm_sd_segment(px, py, -0.25f, 0.5f, -0.25f, -0.5f);  /* left */
    d = fminf(d, gm_sd_segment(px, py, -0.25f, 0.5f, 0.25f, 0.5f));  /* top */
    d = fminf(d, gm_sd_segment(px, py, -0.25f, 0.0f, 0.15f, 0.0f));  /* middle */
    d = fminf(d, gm_sd_segment(px, py, -0.25f, -0.5f, 0.25f, -0.5f));  /* bottom */
    return d;
}

void demo_liquid_text(float t) {
    int W = g_W, H = g_H;

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float x = ((float)px / W - 0.5f) * 3.0f;
            float y = (0.5f - (float)py / H) * 3.0f;

            /* Base SDF: letters F and E */
            float d = letter_F(x, y, -0.5f);
            d = fminf(d, letter_E(x, y, 0.5f));
            d -= 0.06f;  /* thickness */

            /* 4 orbiting blobs with smooth union */
            for (int i = 0; i < 4; i++) {
                float a = t * (0.8f + i * 0.3f) + i * 1.57f;
                float bx = cosf(a) * 0.8f;
                float by = sinf(a * 0.7f + i * 0.5f) * 0.5f;
                float bd = gm_sd_circle(x - bx, y - by, 0.15f);
                d = gm_op_smooth_union(d, bd, 0.3f);
            }

            int r, g, b;
            if (d < 0.0f) {
                /* Liquid interior with sin ripple */
                float ripple = sinf(x * 15.0f + t * 4.0f) * 0.1f
                             + sinf(y * 12.0f - t * 3.0f) * 0.1f;
                float v = gm_clamp(0.6f + ripple - d * 2.0f, 0.0f, 1.0f);
                r = (int)(v * 80);
                g = (int)(v * 180);
                b = (int)(v * 255);
            } else {
                /* Exponential glow falloff */
                float glow = expf(-d * 5.0f) * 0.6f;
                r = (int)(glow * 40 + 12);
                g = (int)(glow * 100 + 12);
                b = (int)(glow * 200 + 15);
            }
            gm_pixel(px, py, r, g, b);
        }
    }
    gm_label(5, 3, "LIQUID", RB_RGB(80, 180, 255));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  PARTICLE
 * ═══════════════════════════════════════════════════════════════════════════ */

#define GALAXY_NSTARS 500

void demo_galaxy_pts(float t) {
    int W = g_W, H = g_H;

    static float sx[GALAXY_NSTARS], sy[GALAXY_NSTARS];
    static float bright[GALAXY_NSTARS], sz[GALAXY_NSTARS];
    static int init = 0;

    if (!init) {
        init = 1;
        for (int i = 0; i < GALAXY_NSTARS; i++) {
            /* 3 spiral arms, logarithmic placement */
            int arm = i % 3;
            float fi = (float)i / GALAXY_NSTARS;
            float r = 0.1f + fi * 0.9f;
            float angle = arm * 2.094f + logf(r + 0.01f) * 2.5f;
            /* Add scatter */
            float scatter = gm_ihash(i * 37, i * 73) * 0.15f;
            float scat_a = gm_ihash(i * 113, i * 29) * 6.2832f;
            sx[i] = r * cosf(angle) + scatter * cosf(scat_a);
            sy[i] = r * sinf(angle) + scatter * sinf(scat_a);
            bright[i] = 0.3f + gm_ihash(i * 53, i * 97) * 0.7f;
            sz[i] = (gm_ihash(i * 71, i * 43) > 0.7f) ? 2.0f : 1.0f;  /* size: 1 or 2 */
        }
    }

    gm_clear(8, 8, 12);

    float rot = t * 0.1f;
    float cr = cosf(rot), sr = sinf(rot);
    float cx = W / 2.0f, cy = H / 2.0f;
    float scale = fminf((float)W, (float)H) * 0.42f;

    for (int i = 0; i < GALAXY_NSTARS; i++) {
        /* Rotate */
        float rx = sx[i] * cr - sy[i] * sr;
        float ry = sx[i] * sr + sy[i] * cr;
        int px = (int)(cx + rx * scale);
        int py = (int)(cy + ry * scale);

        if (px < 0 || px >= W || py < 0 || py >= H) continue;

        /* Hue based on distance from center */
        float dist = sqrtf(sx[i] * sx[i] + sy[i] * sy[i]);
        float hue = dist * 0.6f + 0.55f;
        uint8_t hr, hg, hb;
        gm_hue(hue, &hr, &hg, &hb);
        float b = bright[i];
        int r = (int)(hr * b);
        int g = (int)(hg * b);
        int bl = (int)(hb * b);

        gm_pixel(px, py, r, g, bl);
        if (sz[i] > 1.5f) {
            gm_pixel(px + 1, py, r, g, bl);
            gm_pixel(px, py + 1, r, g, bl);
            gm_pixel(px + 1, py + 1, r, g, bl);
        }
    }
    gm_label(5, 3, "GALAXY", RB_RGB(180, 160, 220));
}

#define BOID_COUNT 120

void demo_boids(float t) {
    int W = g_W, H = g_H;

    static float bx[BOID_COUNT], by[BOID_COUNT];
    static float bvx[BOID_COUNT], bvy[BOID_COUNT];
    static float last_t = -1.0f;
    static int init = 0;

    if (!init) {
        init = 1;
        for (int i = 0; i < BOID_COUNT; i++) {
            bx[i] = gm_ihash(i * 17, i * 31) * W;
            by[i] = gm_ihash(i * 53, i * 79) * H;
            bvx[i] = (gm_ihash(i * 41, i * 67) - 0.5f) * 2.0f;
            bvy[i] = (gm_ihash(i * 89, i * 23) - 0.5f) * 2.0f;
        }
        last_t = t;
    }

    float dt = t - last_t;
    if (dt < 0.0f) dt = 0.016f;
    if (dt > 0.1f) dt = 0.1f;
    last_t = t;

    /* Flocking rules */
    float sep_r = 15.0f, ali_r = 35.0f, coh_r = 50.0f;
    for (int i = 0; i < BOID_COUNT; i++) {
        float sep_x = 0, sep_y = 0;
        float ali_x = 0, ali_y = 0; int ali_n = 0;
        float coh_x = 0, coh_y = 0; int coh_n = 0;

        for (int j = 0; j < BOID_COUNT; j++) {
            if (i == j) continue;
            float dx = bx[j] - bx[i], dy = by[j] - by[i];
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < sep_r && dist > 0.01f) {
                sep_x -= dx / dist;
                sep_y -= dy / dist;
            }
            if (dist < ali_r) {
                ali_x += bvx[j]; ali_y += bvy[j]; ali_n++;
            }
            if (dist < coh_r) {
                coh_x += bx[j]; coh_y += by[j]; coh_n++;
            }
        }

        float steer_x = sep_x * 1.5f;
        float steer_y = sep_y * 1.5f;
        if (ali_n > 0) {
            steer_x += (ali_x / ali_n - bvx[i]) * 0.05f;
            steer_y += (ali_y / ali_n - bvy[i]) * 0.05f;
        }
        if (coh_n > 0) {
            steer_x += (coh_x / coh_n - bx[i]) * 0.005f;
            steer_y += (coh_y / coh_n - by[i]) * 0.005f;
        }

        bvx[i] += steer_x * dt * 60.0f;
        bvy[i] += steer_y * dt * 60.0f;

        /* Speed limit */
        float spd = sqrtf(bvx[i] * bvx[i] + bvy[i] * bvy[i]);
        if (spd > 3.0f) {
            bvx[i] = bvx[i] / spd * 3.0f;
            bvy[i] = bvy[i] / spd * 3.0f;
        }

        bx[i] += bvx[i] * dt * 60.0f;
        by[i] += bvy[i] * dt * 60.0f;

        /* Wrap at edges */
        if (bx[i] < 0) bx[i] += W;
        if (bx[i] >= W) bx[i] -= W;
        if (by[i] < 0) by[i] += H;
        if (by[i] >= H) by[i] -= H;
    }

    gm_clear(12, 14, 18);

    /* Draw as small triangles (~4 pixels) */
    for (int i = 0; i < BOID_COUNT; i++) {
        float spd = sqrtf(bvx[i] * bvx[i] + bvy[i] * bvy[i]);
        float nx = (spd > 0.01f) ? bvx[i] / spd : 1.0f;
        float ny = (spd > 0.01f) ? bvy[i] / spd : 0.0f;

        /* Hue based on index */
        float hue = (float)i / BOID_COUNT;
        uint8_t hr, hg, hb;
        gm_hue(hue, &hr, &hg, &hb);

        /* Head pixel */
        int hx = (int)(bx[i] + nx * 2);
        int hy = (int)(by[i] + ny * 2);
        gm_pixel(hx, hy, hr, hg, hb);
        /* Body pixels */
        gm_pixel((int)bx[i], (int)by[i], hr * 3 / 4, hg * 3 / 4, hb * 3 / 4);
        gm_pixel((int)(bx[i] - ny), (int)(by[i] + nx), hr / 2, hg / 2, hb / 2);
        gm_pixel((int)(bx[i] + ny), (int)(by[i] - nx), hr / 2, hg / 2, hb / 2);
    }
    gm_label(5, 3, "BOIDS", RB_RGB(180, 200, 220));
}

void demo_smoke(float t) {
    int W = g_W, H = g_H;
    int N = W * H;

    static float *density = NULL;
    static int init = 0;

    if (!init) {
        init = 1;
        density = (float *)calloc(N, sizeof(float));
    }
    if (!density) return;

    /* Curl noise velocity field and advection */
    float eps = 0.5f;
    for (int py = 1; py < H - 1; py++) {
        for (int px = 1; px < W - 1; px++) {
            /* Curl noise: velocity = (-dN/dy, dN/dx) */
            float nx0 = gm_noise2(px * 0.02f + t * 0.3f, (py - eps) * 0.02f);
            float nx1 = gm_noise2(px * 0.02f + t * 0.3f, (py + eps) * 0.02f);
            float ny0 = gm_noise2((px - eps) * 0.02f + t * 0.3f, py * 0.02f);
            float ny1 = gm_noise2((px + eps) * 0.02f + t * 0.3f, py * 0.02f);

            float vx = -(nx1 - nx0) / (2.0f * eps) * 80.0f;
            float vy = (ny1 - ny0) / (2.0f * eps) * 80.0f - 1.5f;  /* upward bias */

            /* Trace backward */
            int sx = (int)(px - vx);
            int sy = (int)(py - vy);
            if (sx < 0) sx = 0; if (sx >= W) sx = W - 1;
            if (sy < 0) sy = 0; if (sy >= H) sy = H - 1;

            density[py * W + px] = density[sy * W + sx] * 0.995f;
        }
    }

    /* Emit from bottom center */
    for (int dy = -8; dy <= 0; dy++) {
        for (int dx = -12; dx <= 12; dx++) {
            int ex = W / 2 + dx;
            int ey = H - 1 + dy;
            if (ex >= 0 && ex < W && ey >= 0 && ey < H) {
                density[ey * W + ex] = 1.0f;
            }
        }
    }

    /* Draw */
    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float d = gm_clamp(density[py * W + px], 0.0f, 1.0f);
            int v = (int)(d * 220);
            gm_pixel(px, py, v + 10, v + 10, v + 12);
        }
    }
    gm_label(5, 3, "SMOKE", RB_RGB(200, 200, 210));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MEDICAL
 * ═══════════════════════════════════════════════════════════════════════════ */

void demo_ct_slice(float t) {
    int W = g_W, H = g_H;
    float rot = t * 0.15f;
    float cr = cosf(rot), sr = sinf(rot);

    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            float x = ((float)px / W - 0.5f) * 2.8f;
            float y = ((float)py / H - 0.5f) * 2.8f;
            float rx = x * cr - y * sr, ry = x * sr + y * cr;
            float r = sqrtf(rx * rx + ry * ry);

            /* Body outline */
            float d = gm_smoothstep(1.2f, 1.0f, r) * 0.3f;
            /* Outer bone ring */
            d += gm_smoothstep(0.05f, 0.0f, fabsf(r - 0.85f) - 0.04f) * 0.55f;
            /* Inner bone ring */
            d += gm_smoothstep(0.05f, 0.0f, fabsf(r - 0.8f) - 0.03f) * 0.15f;
            /* Spine */
            float spine = sqrtf(rx * rx + (ry + 0.3f) * (ry + 0.3f));
            d += gm_smoothstep(0.15f, 0.05f, spine) * 0.6f;
            /* Organ 1 */
            float org1 = sqrtf((rx - 0.3f) * (rx - 0.3f) + (ry - 0.1f) * (ry - 0.1f));
            d += gm_smoothstep(0.25f, 0.1f, org1) * 0.25f;
            /* Organ 2 */
            float org2 = sqrtf((rx + 0.35f) * (rx + 0.35f) + (ry - 0.15f) * (ry - 0.15f));
            d += gm_smoothstep(0.2f, 0.08f, org2) * 0.2f;
            /* Noise texture */
            d += gm_noise2((float)px / W * 15.0f, (float)py / H * 15.0f) * 0.03f;

            d = gm_clamp(d, 0.0f, 1.0f);
            gm_pixel(px, py, (int)(d * 240), (int)(d * 235), (int)(d * 220));
        }
    }
    gm_label(5, 3, "CT SLICE", RB_RGB(220, 215, 200));
}

void demo_ecg(float t) {
    int W = g_W, H = g_H;
    gm_clear(15, 20, 15);

    /* Green grid lines every 20 px */
    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            if (px % 20 == 0 || py % 20 == 0) {
                gm_pixel(px, py, 25, 45, 25);
            }
        }
    }

    /* ECG trace using Gaussian peaks */
    int prev_ey = -1;
    for (int px = 0; px < W; px++) {
        float phase = (float)px / W * 2.0f - t * 0.8f;
        /* Wrap to [0,1] */
        phase = phase - floorf(phase);

        /* P wave */
        float dp = (phase - 0.1f) / 0.035f;
        float p = 0.1f * expf(-dp * dp);
        /* Q wave */
        float dq = (phase - 0.22f) / 0.015f;
        float q = -0.08f * expf(-dq * dq);
        /* R wave (tall spike) */
        float dr = (phase - 0.25f) / 0.012f;
        float r_wave = 1.0f * expf(-dr * dr);
        /* S wave */
        float ds = (phase - 0.28f) / 0.015f;
        float s = -0.12f * expf(-ds * ds);
        /* T wave */
        float dtt = (phase - 0.42f) / 0.04f;
        float tw = 0.18f * expf(-dtt * dtt);

        float ecg = p + q + r_wave + s + tw;
        int ey = (int)(H / 2.0f - ecg * H * 0.35f);
        ey = gm_clamp(ey, 0, H - 1);

        /* Draw connected line */
        if (prev_ey >= 0) {
            int y0 = prev_ey, y1 = ey;
            if (y0 > y1) { int tmp = y0; y0 = y1; y1 = tmp; }
            for (int y = y0; y <= y1; y++) {
                gm_pixel(px, y, 50, 255, 50);
            }
        }
        gm_pixel(px, ey, 50, 255, 50);
        prev_ey = ey;
    }

    /* Wave labels */
    int cy = H / 2;
    gm_label(W / 10, cy - (int)(H * 0.035f) - 12, "P", RB_RGB(100, 200, 100));
    gm_label(W / 4 - 8, cy - (int)(H * 0.35f) - 12, "QRS", RB_RGB(100, 200, 100));
    gm_label((int)(W * 0.42f), cy - (int)(H * 0.063f) - 12, "T", RB_RGB(100, 200, 100));
    gm_label(5, 3, "ECG", RB_RGB(50, 200, 50));
}

/* --- Molecular: water molecules as smooth-union spheres --- */

/* 6 atoms: 2 oxygen (red), 4 hydrogen (white) forming 2 water molecules */
static const float mol_atoms[][4] = {
    /* x, y, z, radius — oxygen */
    { -0.8f,  0.0f,  0.0f,  0.7f },
    {  0.8f,  0.2f,  0.0f,  0.7f },
    /* hydrogen for molecule 1 */
    { -1.4f,  0.5f,  0.3f,  0.45f },
    { -1.4f, -0.5f, -0.3f,  0.45f },
    /* hydrogen for molecule 2 */
    {  1.4f,  0.7f,  0.3f,  0.45f },
    {  1.4f, -0.3f, -0.3f,  0.45f },
};
#define MOL_NATOMS 6

static float molecular_scene(float x, float y, float z) {
    float cr = cosf(s_time * 0.3f), sr = sinf(s_time * 0.3f);
    float d = 999.0f;
    for (int i = 0; i < MOL_NATOMS; i++) {
        /* Slight bobbing per atom */
        float bob = sinf(s_time * 1.2f + i * 1.0f) * 0.1f;
        float ax = mol_atoms[i][0];
        float ay = mol_atoms[i][1] + bob;
        float az = mol_atoms[i][2];
        /* Rotate around Y */
        float rax = ax * cr + az * sr;
        float raz = -ax * sr + az * cr;
        float sd = gm_sd_sphere(x - rax, y - ay, z - raz, mol_atoms[i][3]);
        d = gm_op_smooth_union(d, sd, 0.3f);
    }
    return d;
}

static void molecular_color(float px, float py, float pz,
                              float nx, float ny, float nz,
                              float light, float dist, float spec,
                              int *r, int *g, int *b) {
    /* Find closest atom to determine color */
    float cr = cosf(s_time * 0.3f), sr = sinf(s_time * 0.3f);
    float min_d = 999.0f;
    int closest = 0;
    for (int i = 0; i < MOL_NATOMS; i++) {
        float bob = sinf(s_time * 1.2f + i * 1.0f) * 0.1f;
        float ax = mol_atoms[i][0];
        float ay = mol_atoms[i][1] + bob;
        float az = mol_atoms[i][2];
        float rax = ax * cr + az * sr;
        float raz = -ax * sr + az * cr;
        float dx = px - rax, dy = py - ay, dz = pz - raz;
        float dd = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dd < min_d) { min_d = dd; closest = i; }
    }

    if (closest < 2) {
        /* Oxygen: red */
        *r = (int)(light * 220 + spec * 180);
        *g = (int)(light * 60 + spec * 60);
        *b = (int)(light * 50 + spec * 50);
    } else {
        /* Hydrogen: white */
        *r = (int)(light * 220 + spec * 180);
        *g = (int)(light * 220 + spec * 180);
        *b = (int)(light * 230 + spec * 180);
    }
}

void demo_molecular(float t) {
    s_time = t;
    gm_render_opts opts = gm_default_opts;
    opts.spec_pow = 24;
    opts.spec_str = 0.4f;
    gm_render3d(t, molecular_scene, 5.0f, molecular_color, &opts);
    gm_label(5, 3, "H2O", RB_RGB(200, 100, 100));
}
