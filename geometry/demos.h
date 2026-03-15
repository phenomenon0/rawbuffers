/*
 * demos.h — Demo registry for Geometry Is Math
 *
 * Each demo is a frame function: void demo_xxx(float t);
 * Categories group demos with labels and thesis strings.
 */
#ifndef DEMOS_H
#define DEMOS_H

typedef void (*demo_frame_fn)(float t);

typedef struct {
    const char    *id;
    const char    *title;
    demo_frame_fn  frame;   /* NULL = static/no-live-demo */
} demo_item_t;

typedef struct {
    const char      *key;
    const char      *label;
    const char      *thesis;
    const demo_item_t *items;
    int               count;
} demo_category_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Demo frame functions — implemented across demos_*.c files
 * ═══════════════════════════════════════════════════════════════════════════ */

/* --- Terrain (demos_1.c) --- */
void demo_heightfield(float t);
void demo_erosion(float t);
void demo_dunes(float t);

/* --- Organic (demos_1.c) --- */
void demo_metaballs(float t);
void demo_voronoi_cells(float t);
void demo_coral(float t);

/* --- Architecture (demos_1.c) --- */
void demo_pillars(float t);
void demo_arch(float t);
void demo_staircase(float t);

/* --- Fluid (demos_1.c) --- */
void demo_waves(float t);
void demo_vortex(float t);
void demo_ripples(float t);

/* --- Crystal (demos_2.c) --- */
void demo_gem(float t);
void demo_voronoi_growth(float t);
void demo_snowflake(float t);

/* --- Industrial (demos_2.c) --- */
void demo_gear(float t);
void demo_helix(float t);
void demo_pipes(float t);

/* --- Mathematical (demos_2.c) --- */
void demo_mandelbrot(float t);
void demo_julia(float t);
void demo_gyroid(float t);

/* --- Atmospheric (demos_2.c) --- */
void demo_fog(float t);
void demo_aurora(float t);
void demo_lightning(float t);

/* --- Textile (demos_3.c) --- */
void demo_woven(float t);
void demo_moire(float t);
void demo_chainmail(float t);

/* --- Typography (demos_3.c) --- */
void demo_sdf_glow(float t);
void demo_neon(float t);
void demo_liquid_text(float t);

/* --- Particle (demos_3.c) --- */
void demo_galaxy_pts(float t);
void demo_boids(float t);
void demo_smoke(float t);

/* --- Medical (demos_3.c) --- */
void demo_ct_slice(float t);
void demo_ecg(float t);
void demo_molecular(float t);

/* --- Game (demos_4.c) --- */
void demo_marching_sq(float t);
void demo_dungeon(float t);
void demo_destruct(float t);

/* --- Signal (demos_4.c) --- */
void demo_waveform(float t);
void demo_fft_butterfly(float t);
void demo_convolution(float t);

/* --- Manufacturing (demos_4.c) --- */
void demo_cnc(float t);
void demo_slicing(float t);
void demo_kerf(float t);

/* --- Cosmological (demos_4.c) --- */
void demo_galaxy_spiral(float t);
void demo_lensing(float t);
void demo_cosmic_web(float t);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Category tables — defined in main.c
 * ═══════════════════════════════════════════════════════════════════════════ */
extern const demo_category_t g_categories[];
extern const int g_num_categories;

#endif /* DEMOS_H */
