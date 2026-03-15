/*  demos_4.c  —  12 demos: GAME(3) + SIGNAL(3) + MANUFACTURING(3) + COSMOLOGICAL(3)
 *  Ported from JS canvas demos to C / rawbuf
 */
#include "geometry_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ───────────────────────── file-scope time for callbacks ───────────────────── */
static float s_time;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  GAME — demo_marching_sq, demo_dungeon, demo_destruct
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* ---------- demo_marching_sq: marching squares isoline extraction ---------- */

static void draw_line(int x0, int y0, int x1, int y1, int r, int g, int b)
{
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        gm_pixel(x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void demo_marching_sq(float t)
{
    gm_clear(15, 14, 12);
    const int gs = 10;
    int gw = g_W / gs, gh = g_H / gs;

    /* edge table: each entry is pairs of edge indices to connect.
       edges: 0=bottom(v0-v1), 1=right(v1-v2), 2=top(v2-v3), 3=left(v3-v0) */
    static const int edge_table[16][4] = {
        {-1,-1,-1,-1}, { 3, 0,-1,-1}, { 0, 1,-1,-1}, { 3, 1,-1,-1},
        { 1, 2,-1,-1}, { 0, 1, 2, 3}, { 0, 2,-1,-1}, { 3, 2,-1,-1},
        { 2, 3,-1,-1}, { 0, 2,-1,-1}, { 0, 3, 1, 2}, { 1, 3,-1,-1},
        { 1, 3,-1,-1}, { 0, 3,-1,-1}, { 0, 1,-1,-1}, {-1,-1,-1,-1}
    };
    static const int edge_count[16] = {0,2,2,2,2,4,2,2,2,2,4,2,2,2,2,0};

    /* edge midpoint offsets (in grid units): edge0=bottom, edge1=right, edge2=top, edge3=left */
    static const float edge_mid[4][2] = {
        {0.5f, 0.0f}, {1.0f, 0.5f}, {0.5f, 1.0f}, {0.0f, 0.5f}
    };

    /* scalar field helper */
    #define FIELD(gx, gy) ({ \
        float fx = (float)(gx) / (float)gw - 0.5f; \
        float fy = (float)(gy) / (float)gh - 0.5f; \
        float fx4 = fx * 4.0f, fy4 = fy * 4.0f; \
        float rr = sqrtf(fx4*fx4 + fy4*fy4); \
        sinf(fx4*2.0f + t) * cosf(fy4*2.0f) + sinf(rr*3.0f - t*1.5f)*0.5f; \
    })

    /* draw background tint */
    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float fx = ((float)px / g_W - 0.5f) * 4.0f;
            float fy = ((float)py / g_H - 0.5f) * 4.0f;
            float rr = sqrtf(fx*fx + fy*fy);
            float v = sinf(fx*2.0f + t) * cosf(fy*2.0f) + sinf(rr*3.0f - t*1.5f)*0.5f;
            if (v > 0) {
                int g_val = (int)(v * 40.0f);
                gm_pixel(px, py, 15 + g_val/3, 14 + g_val, 12 + g_val/4);
            } else {
                gm_pixel(px, py, 15, 14, 12);
            }
        }
    }

    /* march squares */
    for (int gy = 0; gy < gh - 1; gy++) {
        for (int gx = 0; gx < gw - 1; gx++) {
            float v0 = FIELD(gx,     gy);
            float v1 = FIELD(gx + 1, gy);
            float v2 = FIELD(gx + 1, gy + 1);
            float v3 = FIELD(gx,     gy + 1);
            int idx = 0;
            if (v0 > 0) idx |= 1;
            if (v1 > 0) idx |= 2;
            if (v2 > 0) idx |= 4;
            if (v3 > 0) idx |= 8;

            int n = edge_count[idx];
            for (int i = 0; i < n; i += 2) {
                int e0 = edge_table[idx][i];
                int e1 = edge_table[idx][i + 1];
                int x0 = (int)((gx + edge_mid[e0][0]) * gs);
                int y0 = (int)((gy + edge_mid[e0][1]) * gs);
                int x1 = (int)((gx + edge_mid[e1][0]) * gs);
                int y1 = (int)((gy + edge_mid[e1][1]) * gs);
                draw_line(x0, y0, x1, y1, 230, 210, 60);
            }
        }
    }
    #undef FIELD

    gm_label(4, 4, "MARCHING SQUARES", RB_RGB(230, 210, 60));
}

/* ---------- demo_dungeon: BSP procedural dungeon ---------- */

#define MAX_ROOMS 32
#define MAX_HALLS 64

typedef struct { int x, y, w, h; } rect_t;

static rect_t d_rooms[MAX_ROOMS];
static int    d_room_count;
static rect_t d_halls[MAX_HALLS];
static int    d_hall_count;
static float  d_gen_time;
static int    d_init;

static int d_rng_seed;
static int d_rand(void)
{
    d_rng_seed = d_rng_seed * 1103515245 + 12345;
    return (d_rng_seed >> 16) & 0x7fff;
}
static int d_rand_range(int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + d_rand() % (hi - lo);
}

static void bsp_split(int x, int y, int w, int h, int depth)
{
    if (depth >= 4 || w < 60 || h < 50) {
        /* leaf — place room */
        if (d_room_count < MAX_ROOMS) {
            int rw = d_rand_range(30, w - 4);
            int rh = d_rand_range(25, h - 4);
            int rx = x + d_rand_range(2, w - rw - 2);
            int ry = y + d_rand_range(2, h - rh - 2);
            d_rooms[d_room_count++] = (rect_t){rx, ry, rw, rh};
        }
        return;
    }
    int horiz = (d_rand() % 2);
    if (w > h * 2) horiz = 0;
    if (h > w * 2) horiz = 1;

    if (horiz) {
        int split = d_rand_range(h * 2 / 5, h * 3 / 5);
        bsp_split(x, y, w, split, depth + 1);
        bsp_split(x, y + split, w, h - split, depth + 1);
        /* corridor */
        if (d_hall_count < MAX_HALLS) {
            int cx = x + w / 2;
            d_halls[d_hall_count++] = (rect_t){cx - 1, y + split - 3, 3, 6};
        }
    } else {
        int split = d_rand_range(w * 2 / 5, w * 3 / 5);
        bsp_split(x, y, split, h, depth + 1);
        bsp_split(x + split, y, w - split, h, depth + 1);
        if (d_hall_count < MAX_HALLS) {
            int cy = y + h / 2;
            d_halls[d_hall_count++] = (rect_t){x + split - 3, cy - 1, 6, 3};
        }
    }
}

static void dungeon_generate(float t)
{
    d_rng_seed = (int)(t * 1000.0f) ^ 0xBEEF;
    d_room_count = 0;
    d_hall_count = 0;
    bsp_split(2, 2, g_W - 4, g_H - 4, 0);
    /* connect adjacent rooms with corridors */
    for (int i = 0; i + 1 < d_room_count && d_hall_count < MAX_HALLS; i++) {
        int cx0 = d_rooms[i].x + d_rooms[i].w / 2;
        int cy0 = d_rooms[i].y + d_rooms[i].h / 2;
        int cx1 = d_rooms[i+1].x + d_rooms[i+1].w / 2;
        int cy1 = d_rooms[i+1].y + d_rooms[i+1].h / 2;
        /* L-shaped corridor */
        if (d_hall_count < MAX_HALLS) {
            int lx = cx0 < cx1 ? cx0 : cx1;
            int hw = abs(cx1 - cx0) + 3;
            d_halls[d_hall_count++] = (rect_t){lx, cy0 - 1, hw, 3};
        }
        if (d_hall_count < MAX_HALLS) {
            int ly = cy0 < cy1 ? cy0 : cy1;
            int hh = abs(cy1 - cy0) + 3;
            d_halls[d_hall_count++] = (rect_t){cx1 - 1, ly, 3, hh};
        }
    }
    d_gen_time = t;
}

void demo_dungeon(float t)
{
    if (!d_init || t - d_gen_time > 5.0f) {
        dungeon_generate(t);
        d_init = 1;
    }

    gm_clear(10, 10, 15);

    /* draw corridors */
    for (int i = 0; i < d_hall_count; i++) {
        rect_t *h = &d_halls[i];
        for (int py = h->y; py < h->y + h->h; py++)
            for (int px = h->x; px < h->x + h->w; px++)
                gm_pixel(px, py, 50, 45, 55);
    }

    /* draw rooms */
    for (int i = 0; i < d_room_count; i++) {
        rect_t *rm = &d_rooms[i];
        for (int py = rm->y; py < rm->y + rm->h; py++) {
            for (int px = rm->x; px < rm->x + rm->w; px++) {
                int bx = (px == rm->x || px == rm->x + rm->w - 1);
                int by = (py == rm->y || py == rm->y + rm->h - 1);
                if (bx || by)
                    gm_pixel(px, py, 100, 90, 80);
                else
                    gm_pixel(px, py, 45, 42, 50);
            }
        }
    }

    /* player dot: cycle through rooms */
    if (d_room_count > 0) {
        int ri = ((int)(t * 2.0f)) % d_room_count;
        int px = d_rooms[ri].x + d_rooms[ri].w / 2;
        int py = d_rooms[ri].y + d_rooms[ri].h / 2;
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++)
                if (dx*dx + dy*dy <= 5)
                    gm_pixel(px+dx, py+dy, 230, 200, 60);
    }

    gm_label(4, 4, "BSP DUNGEON", RB_RGB(200, 180, 80));
}

/* ---------- demo_destruct: destructible terrain ---------- */

#define TERR_MAX_W 640
static float d_terrain[TERR_MAX_W];
static int   d_terr_init;
static float d_last_crater;

void demo_destruct(float t)
{
    int w = g_W < TERR_MAX_W ? g_W : TERR_MAX_W;

    if (!d_terr_init) {
        /* generate initial terrain using noise */
        for (int x = 0; x < w; x++) {
            float fx = (float)x / (float)w;
            d_terrain[x] = (float)g_H * 0.55f
                         + sinf(fx * 6.0f) * 20.0f
                         + sinf(fx * 14.0f) * 8.0f
                         + gm_noise2(fx * 5.0f, 0.0f) * 15.0f;
        }
        d_last_crater = t;
        d_terr_init = 1;
    }

    /* crater every 0.6s */
    if (t - d_last_crater > 0.6f) {
        d_last_crater = t;
        int cx = (int)(gm_ihash((int)(t * 100), 42) * (float)w);
        float rad = 8.0f + gm_ihash((int)(t * 100), 99) * 15.0f;
        float depth = rad * 1.2f;
        for (int x = 0; x < w; x++) {
            float dx = (float)(x - cx);
            if (fabsf(dx) < rad) {
                float f = 1.0f - (dx * dx) / (rad * rad);
                d_terrain[x] += f * depth;
                if (d_terrain[x] > (float)g_H - 1) d_terrain[x] = (float)g_H - 1;
            }
        }
    }

    /* draw */
    for (int px = 0; px < w; px++) {
        int th = (int)d_terrain[px];
        for (int py = 0; py < g_H; py++) {
            if (py < th) {
                /* sky gradient */
                float sf = (float)py / (float)g_H;
                int sr = (int)(20 + sf * 30);
                int sg = (int)(25 + sf * 40);
                int sb = (int)(60 + sf * 80);
                gm_pixel(px, py, sr, sg, sb);
            } else {
                /* terrain gradient */
                float df = (float)(py - th) / (float)(g_H - th + 1);
                int tr = (int)(80 - df * 40);
                int tg = (int)(130 - df * 60);
                int tb = (int)(50 - df * 30);
                gm_pixel(px, py, tr, tg, tb);
            }
        }
    }

    gm_label(4, 4, "DESTRUCTIBLE TERRAIN", RB_RGB(200, 220, 100));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  SIGNAL — demo_waveform, demo_fft_butterfly, demo_convolution
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* ---------- demo_waveform: additive synthesis ---------- */

void demo_waveform(float t)
{
    gm_clear(15, 14, 18);

    /* grid lines */
    for (int py = 0; py < g_H; py += 25)
        for (int px = 0; px < g_W; px++)
            gm_pixel(px, py, 30, 28, 35);
    for (int px = 0; px < g_W; px += 20)
        for (int py = 0; py < g_H; py++)
            gm_pixel(px, py, 30, 28, 35);

    int harmonics = (int)floorf(fmodf(t * 0.5f, 8.0f)) + 1;

    static const int colors[8][3] = {
        {230,80,80},{80,180,230},{80,230,120},{230,200,80},
        {200,120,230},{120,230,200},{230,150,100},{180,180,230}
    };

    float mid = (float)g_H * 0.5f;
    float amp = (float)g_H * 0.35f;

    /* draw individual harmonics (dim) */
    for (int k = 1; k <= harmonics; k++) {
        int prev_y = -1;
        for (int px = 0; px < g_W; px++) {
            float phase = (float)px / (float)g_W * 6.2832f * 2.0f + t;
            float v = sinf(phase * (float)k) / (float)k;
            int py = (int)(mid + v * amp);
            if (py >= 0 && py < g_H) {
                int ci = (k - 1) % 8;
                gm_pixel(px, py, colors[ci][0] / 3, colors[ci][1] / 3, colors[ci][2] / 3);
            }
            prev_y = py;
        }
    }

    /* draw sum as bright white */
    int prev_sy = -1;
    for (int px = 0; px < g_W; px++) {
        float phase = (float)px / (float)g_W * 6.2832f * 2.0f + t;
        float sum = 0.0f;
        for (int k = 1; k <= harmonics; k++)
            sum += sinf(phase * (float)k) / (float)k;
        int sy = (int)(mid + sum * amp);
        if (prev_sy >= 0 && px > 0) {
            /* connect with line */
            draw_line(px - 1, prev_sy, px, sy, 230, 230, 240);
        }
        prev_sy = sy;
    }

    char label[32];
    snprintf(label, sizeof(label), "HARMONICS=%d", harmonics);
    gm_label(4, 4, label, RB_RGB(230, 230, 240));
}

/* ---------- demo_fft_butterfly: FFT data flow ---------- */

void demo_fft_butterfly(float t)
{
    gm_clear(15, 14, 18);

    const int N = 16, stages = 4;
    float sx = (float)g_W / (float)(N + 1);
    float sy = (float)g_H / (float)(stages + 1);

    /* animate: current stage progress */
    float anim = fmodf(t * 0.4f, (float)stages);
    int cur_stage = (int)floorf(anim);
    float stage_frac = anim - (float)cur_stage;

    for (int s = 0; s < stages; s++) {
        int group_size = 1 << (s + 1);
        int half = group_size / 2;
        float y0 = sy * (float)(s + 1);
        float y1 = sy * (float)(s + 2);

        for (int g = 0; g < N; g += group_size) {
            for (int j = 0; j < half; j++) {
                int i0 = g + j;
                int i1 = g + j + half;
                float x0 = sx * (float)(i0 + 1);
                float x1 = sx * (float)(i1 + 1);

                int bright = (s <= cur_stage) ? 1 : 0;
                if (s == cur_stage) {
                    bright = (stage_frac > 0.3f) ? 1 : 0;
                }

                int cr = bright ? 100 : 40;
                int cg = bright ? 180 : 60;
                int cb = bright ? 230 : 80;

                /* straight through */
                draw_line((int)x0, (int)y0, (int)x0, (int)y1, cr, cg, cb);
                draw_line((int)x1, (int)y0, (int)x1, (int)y1, cr, cg, cb);
                /* cross connections */
                draw_line((int)x0, (int)y0, (int)x1, (int)y1, cr/2, cg/2, cb/2);
                draw_line((int)x1, (int)y0, (int)x0, (int)y1, cr/2, cg/2, cb/2);
            }
        }
    }

    /* draw nodes */
    for (int s = 0; s <= stages; s++) {
        float y = sy * (float)(s + 1);
        for (int i = 0; i < N; i++) {
            float x = sx * (float)(i + 1);
            int cx = (int)x, cy = (int)y;
            for (int dy = -3; dy <= 3; dy++)
                for (int dx = -3; dx <= 3; dx++)
                    if (dx*dx + dy*dy <= 9)
                        gm_pixel(cx+dx, cy+dy, 200, 210, 230);
        }
    }

    gm_label(4, 4, "FFT BUTTERFLY (N=16)", RB_RGB(200, 210, 230));
}

/* ---------- demo_convolution: kernel sliding over signal ---------- */

void demo_convolution(float t)
{
    gm_clear(15, 14, 18);

    /* generate signal */
    float signal[640];
    int w = g_W < 640 ? g_W : 640;
    for (int i = 0; i < w; i++) {
        float x = (float)i / (float)w;
        signal[i] = sinf(x * 12.0f) * 0.5f
                   + gm_noise2(x * 8.0f, 3.14f) * 0.3f
                   + (x > 0.45f && x < 0.55f ? 0.4f : 0.0f);
    }

    /* kernel types: 0=gaussian, 1=derivative, 2=box */
    int ktype = ((int)floorf(t * 0.3f)) % 3;
    const char *kname[3] = {"GAUSSIAN", "DERIVATIVE", "BOX"};
    int ksize = 21;
    float kernel[21];
    if (ktype == 0) {
        /* gaussian */
        float sigma = 3.0f, sum = 0;
        for (int i = 0; i < ksize; i++) {
            float x = (float)(i - ksize/2);
            kernel[i] = expf(-x*x / (2.0f*sigma*sigma));
            sum += kernel[i];
        }
        for (int i = 0; i < ksize; i++) kernel[i] /= sum;
    } else if (ktype == 1) {
        /* derivative (central difference, smoothed) */
        memset(kernel, 0, sizeof(kernel));
        float sigma = 2.0f, sum = 0;
        for (int i = 0; i < ksize; i++) {
            float x = (float)(i - ksize/2);
            kernel[i] = -x * expf(-x*x / (2.0f*sigma*sigma));
            sum += fabsf(kernel[i]);
        }
        if (sum > 0) for (int i = 0; i < ksize; i++) kernel[i] /= sum;
    } else {
        /* box */
        for (int i = 0; i < ksize; i++) kernel[i] = 1.0f / (float)ksize;
    }

    /* convolve */
    float output[640];
    for (int i = 0; i < w; i++) {
        float v = 0;
        for (int j = 0; j < ksize; j++) {
            int si = i + j - ksize/2;
            if (si < 0) si = 0;
            if (si >= w) si = w - 1;
            v += signal[si] * kernel[j];
        }
        output[i] = v;
    }

    /* kernel position */
    int kpos = (int)(fmodf(t * 40.0f, (float)w));

    float mid = (float)g_H * 0.5f;
    float amp = (float)g_H * 0.35f;

    /* draw input signal (blue) */
    for (int px = 1; px < w; px++) {
        int y0 = (int)(mid * 0.5f - signal[px-1] * amp * 0.5f);
        int y1 = (int)(mid * 0.5f - signal[px] * amp * 0.5f);
        draw_line(px-1, y0, px, y1, 80, 140, 220);
    }

    /* draw kernel highlight */
    for (int j = 0; j < ksize; j++) {
        int si = kpos + j - ksize/2;
        if (si >= 0 && si < w) {
            int y = (int)(mid * 0.5f - signal[si] * amp * 0.5f);
            gm_pixel(si, y, 230, 80, 80);
            if (y+1 < g_H) gm_pixel(si, y+1, 230, 80, 80);
            if (y-1 >= 0)  gm_pixel(si, y-1, 230, 80, 80);
        }
    }

    /* draw output signal (yellow) */
    for (int px = 1; px < w; px++) {
        int y0 = (int)(mid * 1.4f - output[px-1] * amp * 0.5f);
        int y1 = (int)(mid * 1.4f - output[px] * amp * 0.5f);
        draw_line(px-1, y0, px, y1, 220, 200, 60);
    }

    char label[64];
    snprintf(label, sizeof(label), "CONVOLUTION: %s", kname[ktype]);
    gm_label(4, 4, label, RB_RGB(220, 200, 60));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  MANUFACTURING — demo_cnc, demo_slicing, demo_kerf
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* ---------- demo_cnc: CNC toolpath around star shape ---------- */

void demo_cnc(float t)
{
    float toolAngle = t * 1.5f;
    float toolR = 0.15f;
    float ta5 = toolAngle * 5.0f;
    float ta3 = toolAngle * 3.0f;
    float toolDist = 0.6f + toolR + sinf(ta5 + 1.0f) * 0.15f + sinf(ta3) * 0.1f;
    float toolX = cosf(toolAngle) * toolDist;
    float toolY = sinf(toolAngle) * toolDist;

    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float x = ((float)px / (float)g_W - 0.5f) * 4.0f;
            float y = ((float)py / (float)g_H - 0.5f) * 2.5f;
            float a = atan2f(y, x);
            float r = sqrtf(x*x + y*y);
            float star = r - (0.6f + sinf(a*5.0f + 1.0f)*0.15f + sinf(a*3.0f)*0.1f);

            float offset1 = star - toolR;
            float offset2 = star - toolR * 2.0f;
            float offset3 = star - toolR * 3.0f;

            float tdx = x - toolX, tdy = y - toolY;
            float toolD = sqrtf(tdx*tdx + tdy*tdy) - toolR;

            if (star < 0.0f) {
                gm_pixel(px, py, 60, 80, 100);
            } else if (toolD < 0.0f) {
                gm_pixel(px, py, 180, 180, 190);
            } else {
                float p1 = expf(-fabsf(offset1) * 20.0f) * 0.6f;
                float p2 = expf(-fabsf(offset2) * 20.0f) * 0.4f;
                float p3 = expf(-fabsf(offset3) * 20.0f) * 0.3f;
                float v = p1 + p2 + p3;
                gm_pixel(px, py,
                    (int)(v * 200.0f + 15.0f),
                    (int)(v * 120.0f + 14.0f),
                    (int)(v *  50.0f + 12.0f));
            }
        }
    }

    gm_label(4, 4, "CNC TOOLPATH", RB_RGB(180, 180, 190));
}

/* ---------- demo_slicing: 3D print slicing ---------- */

static float slicing_scene(float x, float y, float z)
{
    float box = gm_sd_box3(x, y, z, 0.5f, 0.8f, 0.5f);
    float sph = gm_sd_sphere(x - 0.3f, y - 0.5f, z, 0.5f);
    float tor = gm_sd_torus(x + 0.3f, y + 0.4f, z, 0.4f, 0.15f);
    float d = gm_op_smooth_union(box, sph, 0.2f);
    d = gm_op_smooth_union(d, tor, 0.15f);
    return d;
}

void demo_slicing(float t)
{
    float zslice = sinf(t * 0.5f) * 0.5f;

    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float x = ((float)px / (float)g_W - 0.5f) * 3.0f;
            float y = ((float)py / (float)g_H - 0.5f) * 2.0f;

            float d = slicing_scene(x, y, zslice);

            if (d < -0.05f) {
                /* inside: infill pattern */
                int infill = (sinf(x * 30.0f) * sinf(y * 30.0f) > 0.0f) ? 1 : 0;
                if (infill)
                    gm_pixel(px, py, 60, 140, 200);
                else
                    gm_pixel(px, py, 40, 100, 160);
            } else if (d < 0.05f) {
                /* outline / shell */
                gm_pixel(px, py, 200, 210, 230);
            } else {
                /* outside: contour glow */
                float glow = expf(-d * 8.0f) * 0.4f;
                gm_pixel(px, py,
                    (int)(glow * 100.0f + 12.0f),
                    (int)(glow * 120.0f + 12.0f),
                    (int)(glow * 180.0f + 18.0f));
            }
        }
    }

    char label[32];
    snprintf(label, sizeof(label), "SLICE Z=%.2f", zslice);
    gm_label(4, 4, label, RB_RGB(200, 210, 230));
}

/* ---------- demo_kerf: laser cutting with kerf ---------- */

static float kerf_shape(float x, float y)
{
    /* rounded rectangle */
    float box = gm_sd_box2(x, y, 1.2f, 0.7f) - 0.08f;
    /* subtract two circles and one box */
    float hole1 = sqrtf((x - 0.4f)*(x - 0.4f) + y*y) - 0.2f;
    float hole2 = sqrtf((x + 0.4f)*(x + 0.4f) + y*y) - 0.15f;
    float hole3 = gm_sd_box2(x, y - 0.3f, 0.15f, 0.15f);
    /* subtraction: max(a, -b) */
    float d = box;
    d = fmaxf(d, -hole1);
    d = fmaxf(d, -hole2);
    d = fmaxf(d, -hole3);
    return d;
}

void demo_kerf(float t)
{
    float k = 0.04f + sinf(t) * 0.02f;

    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float x = ((float)px / (float)g_W - 0.5f) * 3.5f;
            float y = ((float)py / (float)g_H - 0.5f) * 2.2f;

            float d = kerf_shape(x, y);
            float inner = d + k * 0.5f;
            float outer = d - k * 0.5f;

            if (inner < 0.0f) {
                /* part: blue-ish */
                gm_pixel(px, py, 50, 80, 140);
            } else if (outer < 0.0f) {
                /* kerf zone: red laser glow + heat flicker */
                float heat = 0.6f + 0.4f * sinf(t * 20.0f + x * 50.0f);
                float flicker = gm_noise2(x * 10.0f + t * 5.0f, y * 10.0f) * 0.2f + 0.8f;
                float v = heat * flicker;
                gm_pixel(px, py,
                    (int)(230.0f * v),
                    (int)(60.0f * v),
                    (int)(30.0f * v));
            } else {
                /* outside: checkerboard grid */
                int cx = (int)floorf(x * 10.0f);
                int cy = (int)floorf(y * 10.0f);
                int chk = ((cx + cy) & 1);
                gm_pixel(px, py, chk ? 35 : 25, chk ? 35 : 25, chk ? 40 : 30);
            }
        }
    }

    char label[32];
    snprintf(label, sizeof(label), "KERF=%.3f", k);
    gm_label(4, 4, label, RB_RGB(230, 100, 60));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  COSMOLOGICAL — demo_galaxy_spiral, demo_lensing, demo_cosmic_web
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* ---------- demo_galaxy_spiral: density-field galaxy ---------- */

void demo_galaxy_spiral(float t)
{
    float pi = 3.14159265f;
    float tau = 6.28318530f;

    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float x = (float)(px - g_W/2) / 60.0f;
            float y = (float)(py - g_H/2) / 60.0f;
            float r = sqrtf(x*x + y*y);
            float a = atan2f(y, x) + t * 0.1f;

            int arms = 3;
            float twist = 2.5f;
            float armA = a - logf(r + 0.1f) * twist;
            float arm_period = tau / (float)arms;
            float armMod = fmodf(fmodf(armA, arm_period) + arm_period, arm_period) - pi / (float)arms;
            float armDist = fabsf(armMod) * r;

            float density = expf(-armDist * armDist * 3.0f) * expf(-r * 0.3f);
            float core = expf(-r * r * 2.0f) * 0.8f;
            float v = density + core;

            float dust = gm_noise2(x * 3.0f + t * 0.1f, y * 3.0f) * 0.1f * expf(-r * 0.5f);
            float star = gm_ihash(px, py) > 0.995f ? gm_ihash(px + 7, py + 13) * 0.5f : 0.0f;

            int cr = (int)gm_clamp(v * 200.0f + dust * 100.0f + star * 200.0f + 8.0f, 0, 255);
            int cg = (int)gm_clamp(v * 160.0f + dust * 150.0f + star * 180.0f + 8.0f, 0, 255);
            int cb = (int)gm_clamp(v * 220.0f + dust * 200.0f + star * 150.0f + 15.0f, 0, 255);
            gm_pixel(px, py, cr, cg, cb);
        }
    }

    gm_label(4, 4, "GALAXY SPIRAL", RB_RGB(200, 180, 230));
}

/* ---------- demo_lensing: gravitational lensing ---------- */

#define LENS_W 640
#define LENS_H 400
static unsigned char lens_bg[LENS_W * LENS_H * 3];
static int lens_init;

void demo_lensing(float t)
{
    int w = g_W < LENS_W ? g_W : LENS_W;
    int h = g_H < LENS_H ? g_H : LENS_H;

    /* lazy init background star field */
    if (!lens_init) {
        for (int i = 0; i < w * h; i++) {
            float v = gm_ihash(i % w, i / w);
            if (v > 0.993f) {
                float b = (v - 0.993f) / 0.007f;
                lens_bg[i*3+0] = (unsigned char)(b * 200 + 40);
                lens_bg[i*3+1] = (unsigned char)(b * 190 + 50);
                lens_bg[i*3+2] = (unsigned char)(b * 220 + 30);
            } else {
                /* dark background with subtle variation */
                lens_bg[i*3+0] = (unsigned char)(5 + gm_ihash(i % w + 1000, i / w) * 8);
                lens_bg[i*3+1] = (unsigned char)(5 + gm_ihash(i % w + 2000, i / w) * 8);
                lens_bg[i*3+2] = (unsigned char)(10 + gm_ihash(i % w + 3000, i / w) * 10);
            }
        }
        lens_init = 1;
    }

    /* lens position oscillates */
    float lx = (float)w * 0.5f + sinf(t * 0.3f) * (float)w * 0.15f;
    float ly = (float)h * 0.5f + cosf(t * 0.4f) * (float)h * 0.1f;
    float einsteinR = 50.0f;

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            float dx = (float)px - lx;
            float dy = (float)py - ly;
            float r2 = dx*dx + dy*dy;
            float r = sqrtf(r2 + 1.0f);

            /* deflection */
            float deflect = einsteinR * einsteinR / r;
            float nx = dx / r, ny = dy / r;
            int sx = (int)((float)px + nx * deflect);
            int sy = (int)((float)py + ny * deflect);

            /* wrap */
            sx = ((sx % w) + w) % w;
            sy = ((sy % h) + h) % h;

            int idx = (sy * w + sx) * 3;
            int cr = lens_bg[idx+0];
            int cg = lens_bg[idx+1];
            int cb = lens_bg[idx+2];

            /* Einstein ring glow */
            float ring = fabsf(r - einsteinR);
            float glow = expf(-ring * ring * 0.01f) * 0.5f;
            cr = (int)gm_clamp((float)cr + glow * 120.0f, 0, 255);
            cg = (int)gm_clamp((float)cg + glow * 100.0f, 0, 255);
            cb = (int)gm_clamp((float)cb + glow * 180.0f, 0, 255);

            gm_pixel(px, py, cr, cg, cb);
        }
    }

    gm_label(4, 4, "GRAVITATIONAL LENSING", RB_RGB(150, 140, 200));
}

/* ---------- demo_cosmic_web: voronoi filaments + galaxy clusters ---------- */

void demo_cosmic_web(float t)
{
    for (int py = 0; py < g_H; py++) {
        for (int px = 0; px < g_W; px++) {
            float x = (float)px / (float)g_H * 5.0f + t * 0.05f;
            float y = (float)py / (float)g_H * 5.0f;

            float edge = gm_voronoi_edge(x, y);
            float filament = gm_smoothstep(0.15f, 0.0f, edge);

            /* galaxy clusters at cell vertices */
            int ix = (int)floorf(x);
            int iy = (int)floorf(y);
            float cluster = 0.0f;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int cx = ix + dx, cy = iy + dy;
                    float px2 = (float)cx + gm_ihash(cx, cy);
                    float py2 = (float)cy + gm_ihash(cx + 100, cy + 100);
                    float ddx = x - px2, ddy = y - py2;
                    float d = sqrtf(ddx*ddx + ddy*ddy);
                    cluster += expf(-d * 8.0f) * 0.5f;
                }
            }

            float v = filament * 0.4f + cluster;
            float star = gm_ihash(px + 300, py + 300) > 0.997f ? 0.3f : 0.0f;

            int cr = (int)gm_clamp(v * 100.0f + star * 150.0f + 5.0f, 0, 255);
            int cg = (int)gm_clamp(v * 120.0f + star * 120.0f + 5.0f, 0, 255);
            int cb = (int)gm_clamp(v * 200.0f + star * 100.0f + 12.0f, 0, 255);
            gm_pixel(px, py, cr, cg, cb);
        }
    }

    gm_label(4, 4, "COSMIC WEB", RB_RGB(100, 130, 220));
}
