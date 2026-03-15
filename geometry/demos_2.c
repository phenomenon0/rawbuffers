/*
 * demos_2.c — Crystal, Industrial, Mathematical, Atmospheric demos
 *
 * 12 demos ported from JS/HTML originals to C using geometry_math.h
 */
#include "geometry_math.h"

/* File-scope time for scene/color callbacks that can't capture locals */
static float s_time;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CRYSTAL — gem, voronoi_growth, snowflake
 * ═══════════════════════════════════════════════════════════════════════════ */

/* --- Gem cutting planes (normalized at init) --- */
static float s_gem_cuts[14][4];
static int   s_gem_init = 0;

static void gem_init_cuts(void) {
    if (s_gem_init) return;
    float raw[14][4] = {
        { 0, 1, 0,.55f},{ 0,-1, 0,.85f},{ 1, 1, 0,.75f},{-1, 1, 0,.75f},
        { 0, 1, 1,.75f},{ 0, 1,-1,.75f},{ 1,-1, 0,.9f},{-1,-1, 0,.9f},
        { 0,-1, 1,.9f},{ 0,-1,-1,.9f},{ 1, 0, 1,.8f},{-1, 0, 1,.8f},
        { 1, 0,-1,.8f},{-1, 0,-1,.8f}
    };
    for (int i = 0; i < 14; i++) {
        float l = sqrtf(raw[i][0]*raw[i][0]+raw[i][1]*raw[i][1]+raw[i][2]*raw[i][2]);
        if (l < 1e-10f) l = 1.0f;
        s_gem_cuts[i][0] = raw[i][0]/l;
        s_gem_cuts[i][1] = raw[i][1]/l;
        s_gem_cuts[i][2] = raw[i][2]/l;
        s_gem_cuts[i][3] = raw[i][3];
    }
    s_gem_init = 1;
}

static float gem_scene(float x, float y, float z) {
    float d = gm_sd_sphere(x, y, z, 1.1f);
    for (int i = 0; i < 14; i++) {
        float plane = x*s_gem_cuts[i][0] + y*s_gem_cuts[i][1] +
                      z*s_gem_cuts[i][2] - s_gem_cuts[i][3];
        d = fmaxf(d, plane);
    }
    return d;
}

static void gem_color(float px, float py, float pz,
                      float nx, float ny, float nz,
                      float light, float dist, float spec,
                      int *r, int *g, int *b) {
    float dispersion = nx*0.3f + ny*0.2f + nz*0.1f + s_time*0.05f;

    uint8_t h1r, h1g, h1b, h2r, h2g, h2b, h3r, h3g, h3b;
    gm_hue(dispersion + 0.55f, &h1r, &h1g, &h1b);
    gm_hue(dispersion + 0.65f, &h2r, &h2g, &h2b);
    gm_hue(dispersion + 0.45f, &h3r, &h3g, &h3b);

    float blend = sinf(py*8.0f + s_time)*0.5f + 0.5f;

    *r = (int)(gm_lerp((float)h1r, (float)h2r, blend)/255.0f * light * 255.0f + spec*280.0f);
    *g = (int)(gm_lerp((float)h1g, (float)h2g, blend)/255.0f * light * 255.0f + spec*280.0f);
    *b = (int)(gm_lerp((float)h1b, (float)h2b, blend)/255.0f * light * 255.0f + spec*280.0f);
}

void demo_gem(float t) {
    s_time = t;
    gem_init_cuts();
    gm_render_opts opts = gm_default_opts;
    opts.spec_pow = 64;
    opts.spec_str = 0.8f;
    gm_render3d(t, gem_scene, 3.5f, gem_color, &opts);
    gm_bloom(120, 4, 0.6f);
}

/* ─────────────────────────────────────────────────────────────────────────── */

void demo_voronoi_growth(float t) {
    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float x = (float)px / g_H * 5.0f + t * 0.05f;
            float y = (float)py / g_H * 5.0f;
            float d, id;
            gm_voronoi(x, y, &d, &id);
            float edge = gm_voronoi_edge(x, y);
            float glow = expf(-edge * 25.0f) * (sinf(t*3.0f)*0.3f + 0.7f);
            uint8_t hr, hg, hb;
            gm_hue(id * 0.7f, &hr, &hg, &hb);
            int r = (int)(hr * 0.25f + glow * 60.0f + 15.0f);
            int g = (int)(hg * 0.2f  + glow * 130.0f + 14.0f);
            int b = (int)(hb * 0.15f + glow * 255.0f + 12.0f);
            gm_pixel(px, py, r, g, b);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */

void demo_snowflake(float t) {
    float PI = 3.14159265f;
    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float x = (px - g_W/2) / 80.0f;
            float y = (py - g_H/2) / 80.0f;
            float r = sqrtf(x*x + y*y);
            float a = atan2f(y, x);
            float sixth = PI / 3.0f;
            a = gm_mod(a, sixth);
            if (a > PI / 6.0f) a = sixth - a;
            x = r * cosf(a);
            y = r * sinf(a);

            float d = fabsf(y) - 0.06f;
            for (int i = 1; i <= 3; i++) {
                float bx = i * 0.35f;
                float branch = fmaxf(fabsf(y - (x - bx)*0.5f) - 0.03f,
                                     fabsf(x - bx) - 0.18f);
                d = fminf(d, branch);
            }
            d = fminf(d, r - 0.12f);

            if (d < 0) {
                float c = 0.8f + sinf(r*30.0f + t*2.0f)*0.15f;
                gm_pixel(px, py, (int)(c*180+40), (int)(c*200+50), (int)(c*255));
            } else {
                float g = expf(-d * 12.0f) * (sinf(t*1.5f)*0.2f + 0.8f);
                gm_pixel(px, py, (int)(g*60+15), (int)(g*100+14), (int)(g*180+12));
            }
        }
    }
    gm_bloom(140, 3, 0.5f);
    gm_label(5, 3, "FOLD=6", RB_RGB(180, 220, 255));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  INDUSTRIAL — gear, helix, pipes
 * ═══════════════════════════════════════════════════════════════════════════ */

void demo_gear(float t) {
    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float x = (px - g_W/2) / 80.0f;
            float y = (py - g_H/2) / 80.0f;
            float r = sqrtf(x*x + y*y);
            float a = atan2f(y, x) + t;
            float tooth = cosf(a * 12.0f) * 0.12f;
            float body = r - (0.8f + tooth);
            float hole = 0.18f - r;
            float spoke = fminf(fabsf(sinf(a * 3.0f)) * r - 0.02f, r - 0.2f);

            if (body < 0 && hole < 0) {
                float shade = 0.55f + cosf(a * 12.0f) * 0.2f;
                float hl = powf(fmaxf(0.0f, cosf(a - t*0.5f)), 8.0f) * 0.3f;
                gm_pixel(px, py, (int)(shade*210+hl*200),
                                 (int)(shade*200+hl*180),
                                 (int)(shade*180+hl*150));
            } else if (body < 0 && spoke > 0) {
                gm_pixel(px, py, 20, 19, 17);
            } else if (body < 0) {
                float hl = powf(fmaxf(0.0f, cosf(a - t*0.5f)), 4.0f) * 0.2f;
                gm_pixel(px, py, (int)(175+hl*150),
                                 (int)(165+hl*130),
                                 (int)(145+hl*100));
            } else {
                float g = expf(-fmaxf(0.0f, body) * 12.0f);
                gm_pixel(px, py, (int)(g*40+15), (int)(g*35+14), (int)(g*30+12));
            }
        }
    }
    gm_label(5, 3, "TEETH=12", RB_RGB(200, 200, 180));
}

/* ─────────────────────────────────────────────────────────────────────────── */

static float helix_scene(float x, float y, float z) {
    /* Cylinder core */
    float core = gm_sd_cylinder(x, y, z, 0.45f, 1.5f);

    /* Helix thread: wind around the cylinder */
    float pitch = 0.6f;
    float a = atan2f(x, z) + s_time * 0.5f;
    float helixY = gm_mod(y + a * pitch / (2.0f * 3.14159265f), pitch) - pitch * 0.5f;
    float helixR = sqrtf(x*x + z*z) - 0.6f;
    float thread = sqrtf(helixR*helixR + helixY*helixY) - 0.12f;

    return gm_op_union(core, thread);
}

static void helix_color(float px, float py, float pz,
                        float nx, float ny, float nz,
                        float light, float dist, float spec,
                        int *r, int *g, int *b) {
    /* Steel-blue metallic */
    *r = (int)(light * 140.0f + spec * 200.0f);
    *g = (int)(light * 155.0f + spec * 220.0f);
    *b = (int)(light * 180.0f + spec * 255.0f);
}

void demo_helix(float t) {
    s_time = t;
    gm_render_opts opts = gm_default_opts;
    opts.spec_pow = 48;
    opts.spec_str = 0.6f;
    gm_render3d(t, helix_scene, 4.0f, helix_color, &opts);
}

/* ─────────────────────────────────────────────────────────────────────────── */

static float pipes_scene(float x, float y, float z) {
    float c1 = gm_sd_cylinder(x, y, z, 0.35f, 2.0f);              /* vertical */
    float c2 = gm_sd_cylinder(y, z, x, 0.35f, 2.0f);              /* X-axis */
    float c3 = gm_sd_cylinder(z, x, y, 0.35f, 2.0f);              /* Z-axis */
    float d = gm_op_smooth_union(c1, c2, 0.25f);
    d = gm_op_smooth_union(d, c3, 0.25f);
    return d;
}

static void pipes_color(float px, float py, float pz,
                        float nx, float ny, float nz,
                        float light, float dist, float spec,
                        int *r, int *g, int *b) {
    float rust = gm_noise2(px*5.0f, pz*5.0f)*0.2f +
                 gm_noise2(px*12.0f, py*12.0f)*0.1f;
    *r = (int)(light * (180.0f + rust*100.0f) + spec*100.0f);
    *g = (int)(light * (130.0f + rust*30.0f)  + spec*70.0f);
    *b = (int)(light * (80.0f  - rust*50.0f)  + spec*50.0f);
}

void demo_pipes(float t) {
    s_time = t;
    gm_render_opts opts = gm_default_opts;
    opts.spec_pow = 20;
    opts.spec_str = 0.3f;
    gm_render3d(t, pipes_scene, 4.0f, pipes_color, &opts);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MATHEMATICAL — mandelbrot, julia, gyroid
 * ═══════════════════════════════════════════════════════════════════════════ */

void demo_mandelbrot(float t) {
    float cx = -0.5f + sinf(t * 0.1f) * 0.3f;
    float cy = cosf(t * 0.13f) * 0.3f;
    float zoom = 2.5f - sinf(t * 0.07f) * 1.2f;

    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float cr = cx + ((float)px/g_W - 0.5f) * zoom * ((float)g_W / g_H);
            float ci = cy + ((float)py/g_H - 0.5f) * zoom;
            float zr = 0, zi = 0;
            int i = 0;
            for (; i < 128; i++) {
                float zr2 = zr*zr, zi2 = zi*zi;
                if (zr2 + zi2 > 256.0f) break;
                zi = 2.0f*zr*zi + ci;
                zr = zr2 - zi2 + cr;
            }
            if (i >= 128) {
                gm_pixel(px, py, 15, 14, 12);
            } else {
                float sm = i + 1.0f - log2f(log2f(zr*zr + zi*zi));
                float hv = gm_mod(sm * 0.03f + 0.6f, 1.0f);
                float sat = 0.7f + sinf(sm * 0.1f) * 0.15f;
                uint8_t hr, hg, hb;
                gm_hue(hv, &hr, &hg, &hb);
                float br = 0.5f + 0.5f * sinf(sm * 0.15f + 1.5f);
                gm_pixel(px, py, (int)(hr * br * sat / 255.0f * 255.0f),
                                 (int)(hg * br * 0.85f),
                                 (int)(hb * br));
            }
        }
    }
    gm_label(5, 3, "ITER=128", RB_RGB(200, 180, 140));
    char zbuf[32];
    snprintf(zbuf, sizeof(zbuf), "%.2f", zoom);
    gm_param(5, 15, "ZOOM", zbuf, RB_RGB(180, 180, 160));
}

/* ─────────────────────────────────────────────────────────────────────────── */

void demo_julia(float t) {
    float jcr = -0.7269f + sinf(t * 0.3f) * 0.15f;
    float jci =  0.1889f + cosf(t * 0.23f) * 0.15f;

    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float zr = ((float)px / g_W - 0.5f) * 3.0f * ((float)g_W / g_H);
            float zi = ((float)py / g_H - 0.5f) * 3.0f;
            int i = 0;
            for (; i < 128; i++) {
                float zr2 = zr*zr, zi2 = zi*zi;
                if (zr2 + zi2 > 4.0f) break;
                zi = 2.0f*zr*zi + jci;
                zr = zr2 - zi2 + jcr;
            }
            if (i >= 128) {
                gm_pixel(px, py, 15, 14, 12);
            } else {
                float sm = i + 1.0f - log2f(log2f(zr*zr + zi*zi + 1.0f));
                float hv = gm_mod(sm * 0.03f + 0.6f, 1.0f);
                float sat = 0.7f + sinf(sm * 0.1f) * 0.15f;
                uint8_t hr, hg, hb;
                gm_hue(hv, &hr, &hg, &hb);
                float br = 0.5f + 0.5f * sinf(sm * 0.15f + 1.5f);
                gm_pixel(px, py, (int)(hr * br * sat / 255.0f * 255.0f),
                                 (int)(hg * br * 0.85f),
                                 (int)(hb * br));
            }
        }
    }
    gm_label(5, 3, "JULIA SET", RB_RGB(200, 180, 140));
}

/* ─────────────────────────────────────────────────────────────────────────── */

static float gyroid_scene(float x, float y, float z) {
    float g = sinf(x*2.5f)*cosf(y*2.5f) +
              sinf(y*2.5f)*cosf(z*2.5f) +
              sinf(z*2.5f)*cosf(x*2.5f);
    return fabsf(g) / 2.5f - 0.08f;
}

static void gyroid_color(float px, float py, float pz,
                         float nx, float ny, float nz,
                         float light, float dist, float spec,
                         int *r, int *g, int *b) {
    float hv = nx*0.3f + ny*0.4f + nz*0.3f + s_time*0.03f + 0.55f;
    uint8_t hr, hg, hb;
    gm_hue(hv, &hr, &hg, &hb);
    *r = (int)(hr/255.0f * light * 255.0f + spec * 200.0f);
    *g = (int)(hg/255.0f * light * 255.0f + spec * 200.0f);
    *b = (int)(hb/255.0f * light * 255.0f + spec * 200.0f);
}

void demo_gyroid(float t) {
    s_time = t;
    gm_render_opts opts = gm_default_opts;
    opts.spec_pow = 32;
    opts.spec_str = 0.45f;
    gm_render3d(t, gyroid_scene, 4.5f, gyroid_color, &opts);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ATMOSPHERIC — fog, aurora, lightning
 * ═══════════════════════════════════════════════════════════════════════════ */

void demo_fog(float t) {
    int RW = g_W / 2, RH = g_H / 2;
    for (int py = 0; py < RH; py++) {
        for (int px = 0; px < RW; px++) {
            float u = ((float)px / RW - 0.5f) * 2.0f;
            float v = (0.5f - (float)py / RH) * 1.2f;

            /* Ray direction */
            float rdx = u, rdy = v * 0.5f + 0.2f, rdz = -1.0f;
            float rl = sqrtf(rdx*rdx + rdy*rdy + rdz*rdz);
            rdx /= rl; rdy /= rl; rdz /= rl;

            /* March through volume */
            float T = 1.0f;  /* transmittance */
            float cr = 0, cg = 0, cb = 0;
            for (int s = 0; s < 40; s++) {
                float st = s * 0.25f;
                float sx = rdx * st;
                float sy = rdy * st + 0.5f;
                float sz = rdz * st;

                float density = gm_fbm3(sx*0.8f + t*0.1f, sy*0.8f, sz*0.8f - t*0.05f, 4);
                density = fmaxf(0.0f, density * 0.6f + 0.1f);

                if (density > 0.01f) {
                    float dt = density * 0.25f;
                    float absorption = expf(-dt * 2.0f);

                    /* Fog color: warm orange-ish */
                    float lr = 0.9f, lg = 0.7f, lb = 0.4f;
                    /* Add height-dependent blue tint */
                    float hfac = gm_clamp(sy * 0.5f + 0.5f, 0.0f, 1.0f);
                    lr = gm_lerp(lr, 0.3f, hfac);
                    lg = gm_lerp(lg, 0.4f, hfac);
                    lb = gm_lerp(lb, 0.8f, hfac);

                    cr += T * (1.0f - absorption) * lr;
                    cg += T * (1.0f - absorption) * lg;
                    cb += T * (1.0f - absorption) * lb;
                    T *= absorption;
                }
                if (T < 0.01f) break;
            }

            /* Sky background */
            float sky_t = gm_clamp(v * 0.5f + 0.5f, 0.0f, 1.0f);
            float skyr = gm_lerp(0.06f, 0.15f, sky_t);
            float skyg = gm_lerp(0.05f, 0.1f, sky_t);
            float skyb = gm_lerp(0.08f, 0.25f, sky_t);

            int r = (int)((cr + T * skyr) * 255.0f);
            int g = (int)((cg + T * skyg) * 255.0f);
            int b = (int)((cb + T * skyb) * 255.0f);

            /* Write 2x2 block */
            int sx2 = px*2, sy2 = py*2;
            gm_pixel(sx2, sy2, r, g, b);
            gm_pixel(sx2+1, sy2, r, g, b);
            gm_pixel(sx2, sy2+1, r, g, b);
            gm_pixel(sx2+1, sy2+1, r, g, b);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */

void demo_aurora(float t) {
    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float u = (float)px / g_W;
            float v = (float)py / g_H;

            /* Dark sky background with stars */
            float r = 8.0f, g = 6.0f, b = 18.0f;

            /* Stars */
            float star = gm_ihash(px * 7, py * 13);
            if (star > 0.997f) {
                float bright = (star - 0.997f) / 0.003f;
                float twinkle = sinf(t * 2.0f + star * 100.0f) * 0.3f + 0.7f;
                r += bright * twinkle * 200.0f;
                g += bright * twinkle * 200.0f;
                b += bright * twinkle * 220.0f;
            }

            /* 3 curtain layers */
            for (int layer = 0; layer < 3; layer++) {
                float speed = 0.3f + layer * 0.15f;
                float freq = 3.0f + layer * 1.5f;
                float phase = layer * 2.094f; /* ~2*PI/3 */

                float wave = sinf(u * freq * 3.14159f + t * speed + phase) * 0.15f;
                wave += gm_noise2(u * 4.0f + t * speed * 0.5f, (float)layer * 10.0f) * 0.08f;

                float center = 0.25f + wave;
                float dist = fabsf(v - center);
                float curtain = expf(-dist * dist * 80.0f);

                /* Height-dependent fade */
                float hfade = gm_clamp(1.0f - v * 1.5f, 0.0f, 1.0f);
                curtain *= hfade;

                /* Ripple detail */
                float ripple = sinf(u * 30.0f + t * (1.0f + layer*0.5f)) * 0.3f + 0.7f;
                curtain *= ripple;

                /* Color: green → purple gradient per layer */
                if (layer == 0) {
                    r += curtain * 30.0f;
                    g += curtain * 200.0f;
                    b += curtain * 60.0f;
                } else if (layer == 1) {
                    r += curtain * 60.0f;
                    g += curtain * 180.0f;
                    b += curtain * 120.0f;
                } else {
                    r += curtain * 120.0f;
                    g += curtain * 80.0f;
                    b += curtain * 200.0f;
                }
            }

            gm_pixel(px, py, (int)r, (int)g, (int)b);
        }
    }
    gm_bloom(100, 4, 0.5f);
}

/* ─────────────────────────────────────────────────────────────────────────── */

#define LIGHTNING_MAX_SEGS 256

typedef struct {
    float x0, y0, x1, y1;
    float brightness;
} bolt_seg_t;

static bolt_seg_t s_bolt_segs[LIGHTNING_MAX_SEGS];
static int s_bolt_count;
static float s_bolt_time;  /* last regeneration time */

static void lightning_gen_bolt(float x0, float y0, float x1, float y1,
                               float brightness, int depth) {
    if (depth <= 0 || s_bolt_count >= LIGHTNING_MAX_SEGS - 2) {
        if (s_bolt_count < LIGHTNING_MAX_SEGS) {
            bolt_seg_t *s = &s_bolt_segs[s_bolt_count++];
            s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1;
            s->brightness = brightness;
        }
        return;
    }

    float mx = (x0 + x1) * 0.5f + (gm_ihash((int)(x0*100), (int)(y0*100+depth*37)) - 0.5f) * 0.15f;
    float my = (y0 + y1) * 0.5f + (gm_ihash((int)(x1*100+depth*53), (int)(y1*100)) - 0.5f) * 0.04f;

    lightning_gen_bolt(x0, y0, mx, my, brightness, depth - 1);
    lightning_gen_bolt(mx, my, x1, y1, brightness, depth - 1);

    /* Branch with probability */
    if (depth >= 2 && gm_ihash((int)(mx*200+depth*11), (int)(my*200)) > 0.6f) {
        float bx = mx + (gm_ihash((int)(mx*300), (int)(my*300+depth*7)) - 0.3f) * 0.3f;
        float by = my + gm_ihash((int)(mx*400+depth*19), (int)(my*400)) * 0.2f;
        lightning_gen_bolt(mx, my, bx, by, brightness * 0.5f, depth - 2);
    }
}

void demo_lightning(float t) {
    /* Regenerate bolt every 0.8s */
    float period = 0.8f;
    float cycle = t - floorf(t / period) * period;
    float gen_time = t - cycle;

    if (s_bolt_count == 0 || fabsf(gen_time - s_bolt_time) > 0.01f) {
        s_bolt_count = 0;
        s_bolt_time = gen_time;
        /* Random start position on top */
        float sx = 0.3f + gm_ihash((int)(gen_time * 100), 999) * 0.4f;
        lightning_gen_bolt(sx, 0.05f, sx + (gm_ihash((int)(gen_time*200), 888) - 0.5f) * 0.3f,
                          0.85f, 1.0f, 5);
    }

    /* Flash decay */
    float flash = expf(-cycle * 6.0f);

    /* Dark blue background with flash */
    gm_clear((int)(12 + flash * 30), (int)(12 + flash * 25), (int)(22 + flash * 40));

    /* Draw bolt segments with glow */
    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float u = (float)px / g_W;
            float v = (float)py / g_H;

            float glow = 0;
            for (int i = 0; i < s_bolt_count; i++) {
                float d = gm_sd_segment(u, v,
                    s_bolt_segs[i].x0, s_bolt_segs[i].y0,
                    s_bolt_segs[i].x1, s_bolt_segs[i].y1);
                float g = expf(-d * d * (float)g_W * 40.0f) * s_bolt_segs[i].brightness;
                glow += g;
            }

            glow *= flash;
            if (glow > 0.01f) {
                uint32_t c = rb_peek(g_surf, px, py);
                int r = (int)(RB_R(c) + glow * 200.0f);
                int g = (int)(RB_G(c) + glow * 180.0f);
                int b = (int)(RB_B(c) + glow * 255.0f);
                gm_pixel(px, py, r, g, b);
            }
        }
    }
}
