/*
 * Geometry Is Math — 48 live demos in pure C + rawbuf.h
 *
 * Terminal-rendered SDF demos. No GPU. No browser.
 * Arrow keys: switch demos. Tab/Shift-Tab: switch categories.
 * Mouse drag: orbit 3D demos. q/ESC: quit.
 *
 * Build: make geometry (from rawbuf/ root)
 */

#define RAWBUF_IMPLEMENTATION
#include "geometry_math.h"
#include "demos.h"
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Globals (declared extern in geometry_math.h)
 * ═══════════════════════════════════════════════════════════════════════════ */
rb_surface_t *g_surf  = NULL;
int           g_W     = 0;
int           g_H     = 0;
float         g_mouseX = 0;
float         g_mouseY = -0.3f;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Category + item tables
 * ═══════════════════════════════════════════════════════════════════════════ */

static const demo_item_t cat_terrain[] = {
    {"heightfield", "Heightfield Terrain", demo_heightfield},
    {"erosion",     "Erosion Ridges",      demo_erosion},
    {"dunes",       "Sand Dunes",          demo_dunes},
};
static const demo_item_t cat_organic[] = {
    {"metaballs",     "Metaballs",       demo_metaballs},
    {"voronoi_cells", "Voronoi Cells",   demo_voronoi_cells},
    {"coral",         "Coral Branching", demo_coral},
};
static const demo_item_t cat_arch[] = {
    {"pillars",   "Repeated Pillars", demo_pillars},
    {"arch",      "Gothic Arch",      demo_arch},
    {"staircase", "Spiral Staircase", demo_staircase},
};
static const demo_item_t cat_fluid[] = {
    {"waves",   "Wave Superposition", demo_waves},
    {"vortex",  "Vortex Flow",        demo_vortex},
    {"ripples", "Raindrop Ripples",   demo_ripples},
};
static const demo_item_t cat_crystal[] = {
    {"gem",            "Faceted Gem",     demo_gem},
    {"voronoi_growth", "Voronoi Growth",  demo_voronoi_growth},
    {"snowflake",      "Snowflake",       demo_snowflake},
};
static const demo_item_t cat_industrial[] = {
    {"gear",  "Gear Teeth",    demo_gear},
    {"helix", "Thread Helix",  demo_helix},
    {"pipes", "Pipe Junction", demo_pipes},
};
static const demo_item_t cat_math[] = {
    {"mandelbrot", "Mandelbrot Set",  demo_mandelbrot},
    {"julia",      "Julia Set",       demo_julia},
    {"gyroid",     "Gyroid Surface",  demo_gyroid},
};
static const demo_item_t cat_atmo[] = {
    {"fog",       "Volumetric Fog",   demo_fog},
    {"aurora",    "Aurora Borealis",  demo_aurora},
    {"lightning", "Lightning Bolt",   demo_lightning},
};
static const demo_item_t cat_textile[] = {
    {"woven",     "Woven Pattern",       demo_woven},
    {"moire",     "Moire Interference",  demo_moire},
    {"chainmail", "Chainmail",           demo_chainmail},
};
static const demo_item_t cat_typo[] = {
    {"sdf_glow",    "SDF Text Glow", demo_sdf_glow},
    {"neon",        "Neon Sign",     demo_neon},
    {"liquid_text", "Liquid Text",   demo_liquid_text},
};
static const demo_item_t cat_particle[] = {
    {"galaxy_pts", "Galaxy Spiral", demo_galaxy_pts},
    {"boids",      "Flocking Boids", demo_boids},
    {"smoke",      "Smoke Advection", demo_smoke},
};
static const demo_item_t cat_medical[] = {
    {"ct_slice",  "CT Slice",          demo_ct_slice},
    {"ecg",       "Heartbeat ECG",     demo_ecg},
    {"molecular", "Molecular Surface", demo_molecular},
};
static const demo_item_t cat_game[] = {
    {"marching_sq", "Marching Squares",    demo_marching_sq},
    {"dungeon",     "Procedural Dungeon",  demo_dungeon},
    {"destruct",    "Destructible Terrain", demo_destruct},
};
static const demo_item_t cat_signal[] = {
    {"waveform",       "Waveform Superposition", demo_waveform},
    {"fft_butterfly",  "FFT Butterfly",          demo_fft_butterfly},
    {"convolution",    "Convolution Kernel",     demo_convolution},
};
static const demo_item_t cat_mfg[] = {
    {"cnc",     "CNC Toolpath",    demo_cnc},
    {"slicing", "3D Print Slicing", demo_slicing},
    {"kerf",    "Laser Kerf",      demo_kerf},
};
static const demo_item_t cat_cosmo[] = {
    {"galaxy_spiral", "Galaxy Spiral",       demo_galaxy_spiral},
    {"lensing",       "Gravitational Lensing", demo_lensing},
    {"cosmic_web",    "Cosmic Web",          demo_cosmic_web},
};

#define CAT(key, label, thesis, arr) { key, label, thesis, arr, sizeof(arr)/sizeof(arr[0]) }

const demo_category_t g_categories[] = {
    CAT("terrain",    "TERRAIN",     "The ground is a function call.",                     cat_terrain),
    CAT("organic",    "ORGANIC",     "Life is a smooth union.",                            cat_organic),
    CAT("arch",       "ARCH",        "Every column is a repeated SDF.",                    cat_arch),
    CAT("fluid",      "FLUID",       "Water is a sum of sines.",                           cat_fluid),
    CAT("crystal",    "CRYSTAL",     "Crystals are intersections.",                        cat_crystal),
    CAT("industrial", "INDUSTRIAL",  "Machines are math made metal.",                      cat_industrial),
    CAT("math",       "MATH",        "Math objects are their own geometry.",                cat_math),
    CAT("atmo",       "ATMO",        "The sky is a volume integral.",                      cat_atmo),
    CAT("textile",    "TEXTILE",     "Fabric is function composition.",                    cat_textile),
    CAT("typo",       "TYPO",        "Letters are distance fields.",                       cat_typo),
    CAT("particle",   "PARTICLE",    "Swarms are emergent geometry.",                      cat_particle),
    CAT("medical",    "MEDICAL",     "The body is a volume.",                              cat_medical),
    CAT("game",       "GAME",        "Levels are implicit.",                               cat_game),
    CAT("signal",     "SIGNAL",      "Signals are geometry in time.",                      cat_signal),
    CAT("mfg",        "MFG",         "Toolpaths are Minkowski sums.",                      cat_mfg),
    CAT("cosmo",      "COSMO",       "The universe is a density field.",                   cat_cosmo),
};
const int g_num_categories = sizeof(g_categories) / sizeof(g_categories[0]);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Engine state
 * ═══════════════════════════════════════════════════════════════════════════ */
static int cur_cat  = 0;
static int cur_item = 0;
static int dragging = 0;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Draw — called every frame
 * ═══════════════════════════════════════════════════════════════════════════ */
static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;
    g_surf = s;
    g_W = s->width;
    g_H = s->height;
    float t = (float)ctx->time;

    /* --- Input --- */
    rb_input_t *inp = &ctx->input;

    /* Mouse orbit */
    if (rb_mouse_down(inp, RB_MOUSE_LEFT)) {
        g_mouseX += inp->mouse_dx * 0.01f;
        g_mouseY = gm_clamp(g_mouseY + inp->mouse_dy * 0.01f, -1.2f, 0.5f);
    }

    /* Tab / Shift-Tab: switch category */
    if (rb_key_pressed(inp, '\t')) {
        cur_cat = (cur_cat + 1) % g_num_categories;
        cur_item = 0; g_mouseX = 0; g_mouseY = -0.3f;
    }
    /* Number keys 1-9: jump to category */
    for (int k = '1'; k <= '9'; k++) {
        if (rb_key_pressed(inp, k)) {
            int idx = k - '1';
            if (idx < g_num_categories) { cur_cat = idx; cur_item = 0; g_mouseX = 0; g_mouseY = -0.3f; }
        }
    }
    /* Arrow keys: switch demo */
    if (rb_key_pressed(inp, RB_KEY_RIGHT)) {
        cur_item = (cur_item + 1) % g_categories[cur_cat].count;
        g_mouseX = 0; g_mouseY = -0.3f;
    }
    if (rb_key_pressed(inp, RB_KEY_LEFT)) {
        cur_item = (cur_item - 1 + g_categories[cur_cat].count) % g_categories[cur_cat].count;
        g_mouseX = 0; g_mouseY = -0.3f;
    }
    if (rb_key_pressed(inp, RB_KEY_UP)) {
        cur_cat = (cur_cat - 1 + g_num_categories) % g_num_categories;
        cur_item = 0; g_mouseX = 0; g_mouseY = -0.3f;
    }
    if (rb_key_pressed(inp, RB_KEY_DOWN)) {
        cur_cat = (cur_cat + 1) % g_num_categories;
        cur_item = 0; g_mouseX = 0; g_mouseY = -0.3f;
    }

    /* --- Render current demo --- */
    const demo_category_t *cat = &g_categories[cur_cat];
    const demo_item_t *item = &cat->items[cur_item];

    gm_clear(15, 14, 12);

    if (item->frame) {
        item->frame(t);
    } else {
        /* Placeholder grid */
        for (int x = 0; x < g_W; x += 16) for (int y = 0; y < g_H; y++) gm_pixel(x, y, 28, 24, 20);
        for (int y = 0; y < g_H; y += 16) for (int x = 0; x < g_W; x++) gm_pixel(x, y, 28, 24, 20);
    }

    /* --- HUD overlay --- */
    char hud[128];

    /* Category bar (top) */
    snprintf(hud, sizeof(hud), "[%s] %s", cat->label, cat->thesis);
    rb_text(s, 2, 2, hud, RB_RGB(230, 167, 86));

    /* Demo title */
    snprintf(hud, sizeof(hud), "%d/%d  %s", cur_item+1, cat->count, item->title);
    rb_text(s, 2, 12, hud, RB_RGB(240, 232, 216));

    /* Controls + FPS (bottom) */
    int bot = g_H - 10;
    snprintf(hud, sizeof(hud), "arrows:nav  1-9:cat  q:quit  %dFPS", ctx->fps);
    rb_text(s, 2, bot, hud, RB_RGB(138, 126, 106));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Main
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);
    rb_run(&ctx, (rb_callbacks_t){ .draw = draw });
    rb_destroy(&ctx);
    return 0;
}
