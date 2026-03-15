/*
 * geometry_math_web.h — Standalone math library for Geometry Is Math (WASM build)
 *
 * Drop-in replacement for geometry_math.h that removes the rawbuf.h dependency.
 * Provides its own surface type, pixel ops, color macros, and 8px bitmap font.
 *
 * Pixel format: ABGR uint32 (0xAABBGGRR) so bytes read as R,G,B,A — matching
 * Canvas2D ImageData and WebGPU rgba8unorm without any swizzle.
 */
#ifndef GEOMETRY_MATH_WEB_H
#define GEOMETRY_MATH_WEB_H

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Surface — minimal pixel buffer (replaces rb_surface_t)
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    uint32_t *back;
    int       width;
    int       height;
    int       stride;
} gm_surface_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Color macros — ABGR pack order (bytes = R,G,B,A on little-endian / WASM)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define GM_RGBA(r,g,b,a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(g) << 8)  |  (uint32_t)(r))
#define GM_RGB(r,g,b) GM_RGBA(r, g, b, 0xFF)

/* Extract channels from ABGR packed uint32 */
#define GM_R(c) ((c) & 0xFF)
#define GM_G(c) (((c) >> 8) & 0xFF)
#define GM_B(c) (((c) >> 16) & 0xFF)

/* Alias RB_ macros to GM_ so demo code compiles unchanged */
#define RB_RGB(r,g,b) GM_RGB(r,g,b)
#define RB_R(c)       GM_R(c)
#define RB_G(c)       GM_G(c)
#define RB_B(c)       GM_B(c)

/* ═══════════════════════════════════════════════════════════════════════════
 *  Globals shared across demo files (defined in geometry_web.c)
 * ═══════════════════════════════════════════════════════════════════════════ */
extern gm_surface_t *g_surf;
extern int           g_W;
extern int           g_H;
extern float         g_mouseX;
extern float         g_mouseY;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Pixel helpers
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline void rb_pixel(gm_surface_t *s, int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)s->width && (unsigned)y < (unsigned)s->height)
        s->back[y * s->stride + x] = c;
}

static inline uint32_t rb_peek(gm_surface_t *s, int x, int y) {
    if ((unsigned)x < (unsigned)s->width && (unsigned)y < (unsigned)s->height)
        return s->back[y * s->stride + x];
    return 0;
}

static inline void rb_clear(gm_surface_t *s, uint32_t color) {
    int total = s->width * s->height;
    for (int i = 0; i < total; i++)
        s->back[i] = color;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  8×8 Bitmap Font (ASCII 32-126, 760 bytes)
 * ═══════════════════════════════════════════════════════════════════════════ */
static const uint8_t gm__font[95][8] = {
    /* 32 ' ' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 33 '!' */ {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    /* 34 '"' */ {0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00},
    /* 35 '#' */ {0x24,0x7E,0x24,0x24,0x7E,0x24,0x00,0x00},
    /* 36 '$' */ {0x18,0x3E,0x58,0x3C,0x1A,0x7C,0x18,0x00},
    /* 37 '%' */ {0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00},
    /* 38 '&' */ {0x30,0x48,0x30,0x56,0x88,0x76,0x00,0x00},
    /* 39 ''' */ {0x18,0x18,0x08,0x00,0x00,0x00,0x00,0x00},
    /* 40 '(' */ {0x08,0x10,0x20,0x20,0x20,0x10,0x08,0x00},
    /* 41 ')' */ {0x20,0x10,0x08,0x08,0x08,0x10,0x20,0x00},
    /* 42 '*' */ {0x00,0x24,0x18,0x7E,0x18,0x24,0x00,0x00},
    /* 43 '+' */ {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    /* 44 ',' */ {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x08},
    /* 45 '-' */ {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    /* 46 '.' */ {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    /* 47 '/' */ {0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00},
    /* 48 '0' */ {0x3C,0x46,0x4A,0x52,0x62,0x3C,0x00,0x00},
    /* 49 '1' */ {0x18,0x38,0x18,0x18,0x18,0x3C,0x00,0x00},
    /* 50 '2' */ {0x3C,0x42,0x02,0x0C,0x30,0x7E,0x00,0x00},
    /* 51 '3' */ {0x3C,0x42,0x0C,0x02,0x42,0x3C,0x00,0x00},
    /* 52 '4' */ {0x0C,0x14,0x24,0x44,0x7E,0x04,0x00,0x00},
    /* 53 '5' */ {0x7E,0x40,0x7C,0x02,0x42,0x3C,0x00,0x00},
    /* 54 '6' */ {0x1C,0x20,0x7C,0x42,0x42,0x3C,0x00,0x00},
    /* 55 '7' */ {0x7E,0x02,0x04,0x08,0x10,0x10,0x00,0x00},
    /* 56 '8' */ {0x3C,0x42,0x3C,0x42,0x42,0x3C,0x00,0x00},
    /* 57 '9' */ {0x3C,0x42,0x42,0x3E,0x04,0x38,0x00,0x00},
    /* 58 ':' */ {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    /* 59 ';' */ {0x00,0x18,0x18,0x00,0x18,0x18,0x08,0x00},
    /* 60 '<' */ {0x04,0x08,0x10,0x20,0x10,0x08,0x04,0x00},
    /* 61 '=' */ {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00},
    /* 62 '>' */ {0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x00},
    /* 63 '?' */ {0x3C,0x42,0x04,0x08,0x08,0x00,0x08,0x00},
    /* 64 '@' */ {0x3C,0x42,0x4E,0x52,0x4C,0x40,0x3C,0x00},
    /* 65 'A' */ {0x18,0x24,0x42,0x7E,0x42,0x42,0x00,0x00},
    /* 66 'B' */ {0x7C,0x42,0x7C,0x42,0x42,0x7C,0x00,0x00},
    /* 67 'C' */ {0x3C,0x42,0x40,0x40,0x42,0x3C,0x00,0x00},
    /* 68 'D' */ {0x78,0x44,0x42,0x42,0x44,0x78,0x00,0x00},
    /* 69 'E' */ {0x7E,0x40,0x7C,0x40,0x40,0x7E,0x00,0x00},
    /* 70 'F' */ {0x7E,0x40,0x7C,0x40,0x40,0x40,0x00,0x00},
    /* 71 'G' */ {0x3C,0x42,0x40,0x4E,0x42,0x3C,0x00,0x00},
    /* 72 'H' */ {0x42,0x42,0x7E,0x42,0x42,0x42,0x00,0x00},
    /* 73 'I' */ {0x3C,0x18,0x18,0x18,0x18,0x3C,0x00,0x00},
    /* 74 'J' */ {0x1E,0x06,0x06,0x06,0x46,0x3C,0x00,0x00},
    /* 75 'K' */ {0x44,0x48,0x70,0x48,0x44,0x42,0x00,0x00},
    /* 76 'L' */ {0x40,0x40,0x40,0x40,0x40,0x7E,0x00,0x00},
    /* 77 'M' */ {0x42,0x66,0x5A,0x42,0x42,0x42,0x00,0x00},
    /* 78 'N' */ {0x42,0x62,0x52,0x4A,0x46,0x42,0x00,0x00},
    /* 79 'O' */ {0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00},
    /* 80 'P' */ {0x7C,0x42,0x42,0x7C,0x40,0x40,0x00,0x00},
    /* 81 'Q' */ {0x3C,0x42,0x42,0x4A,0x44,0x3A,0x00,0x00},
    /* 82 'R' */ {0x7C,0x42,0x42,0x7C,0x44,0x42,0x00,0x00},
    /* 83 'S' */ {0x3C,0x40,0x3C,0x02,0x42,0x3C,0x00,0x00},
    /* 84 'T' */ {0x7E,0x18,0x18,0x18,0x18,0x18,0x00,0x00},
    /* 85 'U' */ {0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00},
    /* 86 'V' */ {0x42,0x42,0x42,0x24,0x24,0x18,0x00,0x00},
    /* 87 'W' */ {0x42,0x42,0x42,0x5A,0x66,0x42,0x00,0x00},
    /* 88 'X' */ {0x42,0x24,0x18,0x18,0x24,0x42,0x00,0x00},
    /* 89 'Y' */ {0x42,0x42,0x24,0x18,0x18,0x18,0x00,0x00},
    /* 90 'Z' */ {0x7E,0x04,0x08,0x10,0x20,0x7E,0x00,0x00},
    /* 91 '[' */ {0x1C,0x10,0x10,0x10,0x10,0x1C,0x00,0x00},
    /* 92 '\' */ {0x40,0x20,0x10,0x08,0x04,0x02,0x00,0x00},
    /* 93 ']' */ {0x38,0x08,0x08,0x08,0x08,0x38,0x00,0x00},
    /* 94 '^' */ {0x10,0x28,0x44,0x00,0x00,0x00,0x00,0x00},
    /* 95 '_' */ {0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00},
    /* 96 '`' */ {0x10,0x08,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 97 'a' */ {0x00,0x00,0x3C,0x02,0x3E,0x42,0x3E,0x00},
    /* 98 'b' */ {0x40,0x40,0x7C,0x42,0x42,0x42,0x7C,0x00},
    /* 99 'c' */ {0x00,0x00,0x3C,0x40,0x40,0x40,0x3C,0x00},
    /*100 'd' */ {0x02,0x02,0x3E,0x42,0x42,0x42,0x3E,0x00},
    /*101 'e' */ {0x00,0x00,0x3C,0x42,0x7E,0x40,0x3C,0x00},
    /*102 'f' */ {0x0C,0x12,0x10,0x3C,0x10,0x10,0x10,0x00},
    /*103 'g' */ {0x00,0x00,0x3E,0x42,0x42,0x3E,0x02,0x3C},
    /*104 'h' */ {0x40,0x40,0x7C,0x42,0x42,0x42,0x42,0x00},
    /*105 'i' */ {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00},
    /*106 'j' */ {0x06,0x00,0x06,0x06,0x06,0x06,0x46,0x3C},
    /*107 'k' */ {0x40,0x40,0x44,0x48,0x70,0x48,0x44,0x00},
    /*108 'l' */ {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    /*109 'm' */ {0x00,0x00,0x64,0x5A,0x42,0x42,0x42,0x00},
    /*110 'n' */ {0x00,0x00,0x7C,0x42,0x42,0x42,0x42,0x00},
    /*111 'o' */ {0x00,0x00,0x3C,0x42,0x42,0x42,0x3C,0x00},
    /*112 'p' */ {0x00,0x00,0x7C,0x42,0x42,0x7C,0x40,0x40},
    /*113 'q' */ {0x00,0x00,0x3E,0x42,0x42,0x3E,0x02,0x02},
    /*114 'r' */ {0x00,0x00,0x5C,0x62,0x40,0x40,0x40,0x00},
    /*115 's' */ {0x00,0x00,0x3E,0x40,0x3C,0x02,0x7C,0x00},
    /*116 't' */ {0x10,0x10,0x3C,0x10,0x10,0x12,0x0C,0x00},
    /*117 'u' */ {0x00,0x00,0x42,0x42,0x42,0x46,0x3A,0x00},
    /*118 'v' */ {0x00,0x00,0x42,0x42,0x42,0x24,0x18,0x00},
    /*119 'w' */ {0x00,0x00,0x42,0x42,0x42,0x5A,0x24,0x00},
    /*120 'x' */ {0x00,0x00,0x42,0x24,0x18,0x24,0x42,0x00},
    /*121 'y' */ {0x00,0x00,0x42,0x42,0x42,0x3E,0x02,0x3C},
    /*122 'z' */ {0x00,0x00,0x7E,0x04,0x18,0x20,0x7E,0x00},
    /*123 '{' */ {0x0C,0x10,0x10,0x20,0x10,0x10,0x0C,0x00},
    /*124 '|' */ {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    /*125 '}' */ {0x30,0x08,0x08,0x04,0x08,0x08,0x30,0x00},
    /*126 '~' */ {0x00,0x32,0x4C,0x00,0x00,0x00,0x00,0x00},
};

static inline void rb_text(gm_surface_t *s, int x, int y, const char *str, uint32_t c) {
    while (*str) {
        int ch = (unsigned char)*str;
        if (ch >= 32 && ch <= 126) {
            const uint8_t *glyph = gm__font[ch - 32];
            for (int row = 0; row < 8; row++) {
                uint8_t bits = glyph[row];
                for (int col = 0; col < 8; col++) {
                    if (bits & (0x80 >> col))
                        rb_pixel(s, x + col, y + row, c);
                }
            }
        }
        x += 8;
        str++;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Basic math helpers
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline float gm_lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float gm_clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float gm_smoothstep(float a, float b, float x) {
    float t = gm_clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
static inline float gm_fract(float x) { return x - floorf(x); }
static inline float gm_mod(float a, float b) { return a - b * floorf(a / b); }

/* ═══════════════════════════════════════════════════════════════════════════
 *  Noise
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline float gm_ihash(int x, int y) {
    unsigned n = (unsigned)x * 1597334677u ^ (unsigned)y * 3812015801u;
    n = (n ^ (n >> 16)) * 0x45d9f3bu;
    n = (n ^ (n >> 16)) * 0x45d9f3bu;
    return (float)((n ^ (n >> 16)) & 0x7FFFFFFFu) / 2147483647.0f;
}

static inline float gm_noise2(float x, float y) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    float a = gm_ihash(ix, iy), b = gm_ihash(ix + 1, iy);
    float c = gm_ihash(ix, iy + 1), d = gm_ihash(ix + 1, iy + 1);
    return (a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy) * 2.0f - 1.0f;
}

static inline float gm_noise3(float x, float y, float z) {
    int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z);
    float fx = x - ix, fy = y - iy, fz = z - iz;
    float ux = fx*fx*(3-2*fx), uy = fy*fy*(3-2*fy), uz = fz*fz*(3-2*fz);
    float n000 = gm_ihash(ix+iz*137, iy), n100 = gm_ihash(ix+1+iz*137, iy);
    float n010 = gm_ihash(ix+iz*137, iy+1), n110 = gm_ihash(ix+1+iz*137, iy+1);
    float n001 = gm_ihash(ix+(iz+1)*137, iy), n101 = gm_ihash(ix+1+(iz+1)*137, iy);
    float n011 = gm_ihash(ix+(iz+1)*137, iy+1), n111 = gm_ihash(ix+1+(iz+1)*137, iy+1);
    float n00 = n000+(n100-n000)*ux, n10 = n010+(n110-n010)*ux;
    float n01 = n001+(n101-n001)*ux, n11 = n011+(n111-n011)*ux;
    float n0 = n00+(n10-n00)*uy, n1 = n01+(n11-n01)*uy;
    return (n0+(n1-n0)*uz)*2.0f-1.0f;
}

static inline float gm_fbm(float x, float y, int oct) {
    float v = 0, amp = 0.5f, freq = 1.0f;
    for (int i = 0; i < oct; i++) { v += gm_noise2(x*freq, y*freq)*amp; freq *= 2; amp *= 0.5f; }
    return v;
}

static inline float gm_fbm3(float x, float y, float z, int oct) {
    float v = 0, amp = 0.5f, freq = 1.0f;
    for (int i = 0; i < oct; i++) { v += gm_noise3(x*freq, y*freq, z*freq)*amp; freq *= 2; amp *= 0.5f; }
    return v;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Voronoi
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline void gm_voronoi(float x, float y, float *out_d, float *out_id) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float md = 999.0f, mid = 0.0f;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int cx = ix+dx, cy = iy+dy;
        float px = cx + gm_ihash(cx, cy), py = cy + gm_ihash(cx+100, cy+100);
        float d = (x-px)*(x-px) + (y-py)*(y-py);
        if (d < md) { md = d; mid = gm_ihash(cx+200, cy+200); }
    }
    *out_d = sqrtf(md); *out_id = mid;
}

static inline float gm_voronoi_edge(float x, float y) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float md = 999.0f, md2 = 999.0f;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int cx = ix+dx, cy = iy+dy;
        float px = cx + gm_ihash(cx, cy), py = cy + gm_ihash(cx+100, cy+100);
        float d = (x-px)*(x-px)+(y-py)*(y-py);
        if (d < md) { md2 = md; md = d; } else if (d < md2) { md2 = d; }
    }
    return sqrtf(md2) - sqrtf(md);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  2D SDFs
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline float gm_sd_circle(float px, float py, float r) {
    return sqrtf(px*px + py*py) - r;
}
static inline float gm_sd_box2(float px, float py, float bx, float by) {
    float dx = fabsf(px) - bx, dy = fabsf(py) - by;
    float mx = dx > 0 ? dx : 0, my = dy > 0 ? dy : 0;
    return sqrtf(mx*mx + my*my) + fminf(fmaxf(dx, dy), 0.0f);
}
static inline float gm_sd_segment(float px, float py, float ax, float ay, float bx, float by) {
    float bax = bx-ax, bay = by-ay, pax = px-ax, pay = py-ay;
    float h = gm_clamp((pax*bax+pay*bay)/(bax*bax+bay*bay+1e-10f), 0.0f, 1.0f);
    float dx = pax-bax*h, dy = pay-bay*h;
    return sqrtf(dx*dx+dy*dy);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  3D SDFs
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline float gm_sd_sphere(float px, float py, float pz, float r) {
    return sqrtf(px*px+py*py+pz*pz)-r;
}
static inline float gm_sd_torus(float px, float py, float pz, float R, float r) {
    float q = sqrtf(px*px+pz*pz)-R;
    return sqrtf(q*q+py*py)-r;
}
static inline float gm_sd_box3(float px, float py, float pz, float bx, float by, float bz) {
    float dx=fabsf(px)-bx, dy=fabsf(py)-by, dz=fabsf(pz)-bz;
    float mx=dx>0?dx:0, my=dy>0?dy:0, mz=dz>0?dz:0;
    return sqrtf(mx*mx+my*my+mz*mz)+fminf(fmaxf(dx,fmaxf(dy,dz)),0.0f);
}
static inline float gm_sd_cylinder(float px, float py, float pz, float r, float h) {
    float d = sqrtf(px*px+pz*pz)-r, dy = fabsf(py)-h;
    float md = d>0?d:0, mdy = dy>0?dy:0;
    return fminf(fmaxf(d,dy),0.0f)+sqrtf(md*md+mdy*mdy);
}
static inline float gm_sd_plane(float py, float h) { return py - h; }

/* ═══════════════════════════════════════════════════════════════════════════
 *  CSG Ops
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline float gm_op_union(float a, float b) { return fminf(a, b); }
static inline float gm_op_smooth_union(float d1, float d2, float k) {
    float h = fmaxf(k - fabsf(d1-d2), 0.0f) / k;
    return fminf(d1, d2) - h*h*k*0.25f;
}
static inline float gm_op_subtract(float d1, float d2) { return fmaxf(-d1, d2); }
static inline float gm_op_intersect(float d1, float d2) { return fmaxf(d1, d2); }
static inline float gm_op_repeat(float p, float s) {
    return gm_mod(p + s*0.5f, s) - s*0.5f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Color helpers
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline void gm_hue(float h, uint8_t *r, uint8_t *g, uint8_t *b) {
    h = h - floorf(h);
    float f = h * 6.0f;
    int i = (int)floorf(f) % 6;
    float fr = f - floorf(f);
    float rv, gv, bv;
    switch (i) {
        case 0: rv=1; gv=fr; bv=0; break;
        case 1: rv=1-fr; gv=1; bv=0; break;
        case 2: rv=0; gv=1; bv=fr; break;
        case 3: rv=0; gv=1-fr; bv=1; break;
        case 4: rv=fr; gv=0; bv=1; break;
        default: rv=1; gv=0; bv=1-fr; break;
    }
    *r = (uint8_t)(rv * 255); *g = (uint8_t)(gv * 255); *b = (uint8_t)(bv * 255);
}

static inline uint32_t gm_hue_rgb(float h) {
    uint8_t r, g, b;
    gm_hue(h, &r, &g, &b);
    return GM_RGB(r, g, b);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Pixel helpers (write through global surface)
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline void gm_pixel(int x, int y, int r, int g, int b) {
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    rb_pixel(g_surf, x, y, GM_RGB(r, g, b));
}

static inline void gm_clear(int r, int g, int b) {
    rb_clear(g_surf, GM_RGB(r, g, b));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Ray marching + render3D
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef float (*gm_scene_fn)(float x, float y, float z);
typedef void (*gm_color_fn)(float px, float py, float pz,
                            float nx, float ny, float nz,
                            float light, float dist, float spec,
                            int *r, int *g, int *b);

static float gm_march(float ox, float oy, float oz,
                       float dx, float dy, float dz,
                       gm_scene_fn fn, int steps) {
    float t = 0;
    for (int i = 0; i < steps; i++) {
        float d = fn(ox+dx*t, oy+dy*t, oz+dz*t);
        if (d < 0.005f) return t;
        t += d;
        if (t > 20.0f) break;
    }
    return -1.0f;
}

static void gm_calc_norm(float x, float y, float z, gm_scene_fn fn,
                          float *nx, float *ny, float *nz) {
    const float e = 0.0005f;
    float d = fn(x, y, z);
    float gx = fn(x+e, y, z) - d;
    float gy = fn(x, y+e, z) - d;
    float gz = fn(x, y, z+e) - d;
    float l = sqrtf(gx*gx + gy*gy + gz*gz);
    if (l < 1e-10f) l = 1.0f;
    *nx = gx/l; *ny = gy/l; *nz = gz/l;
}

typedef struct {
    float spec_pow;
    float spec_str;
    int   shadow;
    int   ao;
    float light_dir[3];
} gm_render_opts;

static const gm_render_opts gm_default_opts = {
    .spec_pow = 16, .spec_str = 0.3f, .shadow = 0, .ao = 1,
    .light_dir = {0.577f, 0.577f, -0.577f}
};

static void gm_render3d(float t, gm_scene_fn scene_fn,
                         float cam_dist, gm_color_fn color_fn,
                         const gm_render_opts *opts) {
    if (!opts) opts = &gm_default_opts;
    int W = g_W, H = g_H;
    int RW = W/2, RH = H/2;
    float sp = opts->spec_pow, ss = opts->spec_str;
    float lx = opts->light_dir[0], ly = opts->light_dir[1], lz = opts->light_dir[2];
    float ll = sqrtf(lx*lx+ly*ly+lz*lz);
    if (ll < 1e-10f) ll = 1.0f;
    lx /= ll; ly /= ll; lz /= ll;

    float ax = g_mouseX + t*0.15f;
    float ay = gm_clamp(g_mouseY, -1.2f, -0.1f);
    float camX = cam_dist*sinf(ax)*cosf(ay);
    float camY = cam_dist*sinf(-ay);
    float camZ = cam_dist*cosf(ax)*cosf(ay);
    float fl = sqrtf(camX*camX+camY*camY+camZ*camZ);
    if (fl < 1e-10f) fl = 1.0f;
    float fwX=-camX/fl, fwY=-camY/fl, fwZ=-camZ/fl;
    float rtX=fwZ, rtY=0, rtZ=-fwX;
    float rl = sqrtf(rtX*rtX+rtZ*rtZ);
    if (rl < 1e-10f) rl = 1.0f;
    rtX /= rl; rtZ /= rl;
    float upX=rtY*fwZ-rtZ*fwY, upY=rtZ*fwX-rtX*fwZ, upZ=rtX*fwY-rtY*fwX;
    float fov = 1.5f, asp = (float)RW/(float)RH;

    for (int py = 0; py < RH; py++) for (int px = 0; px < RW; px++) {
        float u = (px/(float)RW - 0.5f)*fov*asp;
        float v = (0.5f - py/(float)RH)*fov;
        float rdx = fwX+rtX*u+upX*v, rdy = fwY+rtY*u+upY*v, rdz = fwZ+rtZ*u+upZ*v;
        float rdl = sqrtf(rdx*rdx+rdy*rdy+rdz*rdz);
        rdx /= rdl; rdy /= rdl; rdz /= rdl;
        float mt = gm_march(camX, camY, camZ, rdx, rdy, rdz, scene_fn, 48);
        int r, g, b;
        if (mt > 0) {
            float hx=camX+rdx*mt, hy=camY+rdy*mt, hz=camZ+rdz*mt;
            float nx, ny, nz;
            gm_calc_norm(hx, hy, hz, scene_fn, &nx, &ny, &nz);
            float diff = fmaxf(0, nx*lx+ny*ly+nz*lz);
            float reflX=2*diff*nx-lx, reflY=2*diff*ny-ly, reflZ=2*diff*nz-lz;
            float viewDot = fmaxf(0, -(reflX*rdx+reflY*rdy+reflZ*rdz));
            float spec = powf(viewDot, sp) * ss;
            float ao = 1.0f;
            if (opts->ao) {
                ao = 0;
                for (int i = 1; i <= 5; i++) {
                    float step = i * 0.06f;
                    float sd = scene_fn(hx+nx*step, hy+ny*step, hz+nz*step);
                    ao += fmaxf(0, (step-sd)/step) * (1.0f/(1<<i));
                }
                ao = gm_clamp(1.0f - ao*1.5f, 0.15f, 1.0f);
            }
            float shadow = 1.0f;
            if (opts->shadow) {
                float st = 0.02f;
                for (int i = 0; i < 24; i++) {
                    float sd = scene_fn(hx+lx*st, hy+ly*st, hz+lz*st);
                    if (sd < 0.002f) { shadow = 0.15f; break; }
                    shadow = fminf(shadow, 8*sd/st);
                    st += fmaxf(sd, 0.01f);
                    if (st > 5) break;
                }
                shadow = gm_clamp(shadow, 0.15f, 1.0f);
            }
            float li = 0.12f + 0.88f * diff * ao * shadow;
            if (color_fn) {
                color_fn(hx, hy, hz, nx, ny, nz, li, mt, spec, &r, &g, &b);
            } else {
                r = (int)(li*200+spec*255); g = (int)(li*180+spec*230); b = (int)(li*160+spec*200);
            }
            float fog = 1.0f - expf(-mt*mt*0.008f);
            r = (int)(r*(1-fog)+15*fog); g = (int)(g*(1-fog)+14*fog); b = (int)(b*(1-fog)+12*fog);
        } else { r=15; g=14; b=12; }
        int sx=px*2, sy=py*2;
        gm_pixel(sx,sy,r,g,b); gm_pixel(sx+1,sy,r,g,b);
        gm_pixel(sx,sy+1,r,g,b); gm_pixel(sx+1,sy+1,r,g,b);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Bloom post-processing
 * ═══════════════════════════════════════════════════════════════════════════ */
static void gm_bloom(int threshold, int radius, float strength) {
    int W = g_W, H = g_H;
    float *buf = (float *)calloc(W * H, sizeof(float));
    float *tmp = (float *)calloc(W * H, sizeof(float));
    if (!buf || !tmp) { free(buf); free(tmp); return; }
    for (int i = 0; i < W * H; i++) {
        uint32_t c = g_surf->back[i];
        float lum = (GM_R(c) + GM_G(c) + GM_B(c)) / 3.0f;
        buf[i] = lum > threshold ? (lum - threshold) / (255.0f - threshold) : 0;
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float sum = 0, wt = 0;
            for (int dx = -radius; dx <= radius; dx++) {
                int sx = x + dx;
                if (sx < 0 || sx >= W) continue;
                float w = expf(-(float)(dx*dx) / (2.0f * radius * radius / 4.0f));
                sum += buf[y * W + sx] * w; wt += w;
            }
            tmp[y * W + x] = sum / wt;
        }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float sum = 0, wt = 0;
            for (int dy = -radius; dy <= radius; dy++) {
                int sy = y + dy;
                if (sy < 0 || sy >= H) continue;
                float w = expf(-(float)(dy*dy) / (2.0f * radius * radius / 4.0f));
                sum += tmp[sy * W + x] * w; wt += w;
            }
            float bloom = sum / wt * strength * 255.0f;
            if (bloom > 0.5f) {
                uint32_t c = g_surf->back[y * W + x];
                int r = (int)fminf(255, GM_R(c) + bloom * 1.0f);
                int g = (int)fminf(255, GM_G(c) + bloom * 0.9f);
                int b = (int)fminf(255, GM_B(c) + bloom * 0.8f);
                g_surf->back[y * W + x] = GM_RGB(r, g, b);
            }
        }
    free(buf); free(tmp);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Annotation helpers
 * ═══════════════════════════════════════════════════════════════════════════ */
static inline void gm_label(int x, int y, const char *text, uint32_t color) {
    int len = (int)strlen(text);
    int w = len * 6 + 4, h = 10;
    for (int py = y-1; py < y+h; py++)
        for (int px = x-1; px < x+w; px++) {
            uint32_t c = rb_peek(g_surf, px, py);
            int r = GM_R(c) * 4 / 10;
            int g = GM_G(c) * 4 / 10;
            int b = GM_B(c) * 4 / 10;
            rb_pixel(g_surf, px, py, GM_RGB(r, g, b));
        }
    rb_text(g_surf, x, y, text, color);
}

static inline void gm_param(int x, int y, const char *name, const char *value, uint32_t color) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s=%s", name, value);
    rb_text(g_surf, x, y, buf, color);
}

#endif /* GEOMETRY_MATH_WEB_H */
