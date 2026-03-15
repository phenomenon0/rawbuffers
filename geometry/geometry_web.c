/*
 * geometry_web.c — WASM entry point for Geometry Is Math
 *
 * Compiled with emcc. Exports render_frame/get_buffer/set_demo/set_mouse
 * plus metadata accessors and tunable parameters.
 *
 * Pixel buffer: up to 800×500 RGBA, resolution selectable from JS.
 */
#include <emscripten.h>

/* RAWBUF_GEOMETRY defined via -D flag — selects geometry_math_web.h */
#include "geometry_math.h"
#include "demos.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  Static pixel buffer + surface (max resolution)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define MAX_W 800
#define MAX_H 500
#define DEFAULT_W 640
#define DEFAULT_H 400

static uint32_t pixels[MAX_W * MAX_H];
static gm_surface_t surface = {
    .back   = pixels,
    .width  = DEFAULT_W,
    .height = DEFAULT_H,
    .stride = DEFAULT_W
};

/* ═══════════════════════════════════════════════════════════════════════════
 *  Globals the math lib expects
 * ═══════════════════════════════════════════════════════════════════════════ */
gm_surface_t *g_surf  = &surface;
int           g_W     = DEFAULT_W;
int           g_H     = DEFAULT_H;
float         g_mouseX = 0;
float         g_mouseY = -0.3f;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Tunable parameters
 * ═══════════════════════════════════════════════════════════════════════════ */
static float time_scale  = 1.0f;
static int   auto_rotate = 0;

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
    {"galaxy_pts", "Galaxy Spiral",  demo_galaxy_pts},
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
    {"cnc",     "CNC Toolpath",     demo_cnc},
    {"slicing", "3D Print Slicing", demo_slicing},
    {"kerf",    "Laser Kerf",       demo_kerf},
};
static const demo_item_t cat_cosmo[] = {
    {"galaxy_spiral", "Galaxy Spiral",         demo_galaxy_spiral},
    {"lensing",       "Gravitational Lensing", demo_lensing},
    {"cosmic_web",    "Cosmic Web",            demo_cosmic_web},
};

#define CAT(key, label, thesis, arr) { key, label, thesis, arr, sizeof(arr)/sizeof(arr[0]) }

const demo_category_t g_categories[] = {
    CAT("terrain",    "TERRAIN",     "The ground is a function call.",        cat_terrain),
    CAT("organic",    "ORGANIC",     "Life is a smooth union.",               cat_organic),
    CAT("arch",       "ARCH",        "Every column is a repeated SDF.",       cat_arch),
    CAT("fluid",      "FLUID",       "Water is a sum of sines.",             cat_fluid),
    CAT("crystal",    "CRYSTAL",     "Crystals are intersections.",           cat_crystal),
    CAT("industrial", "INDUSTRIAL",  "Machines are math made metal.",         cat_industrial),
    CAT("math",       "MATH",        "Math objects are their own geometry.",   cat_math),
    CAT("atmo",       "ATMO",        "The sky is a volume integral.",         cat_atmo),
    CAT("textile",    "TEXTILE",     "Fabric is function composition.",       cat_textile),
    CAT("typo",       "TYPO",        "Letters are distance fields.",          cat_typo),
    CAT("particle",   "PARTICLE",    "Swarms are emergent geometry.",         cat_particle),
    CAT("medical",    "MEDICAL",     "The body is a volume.",                 cat_medical),
    CAT("game",       "GAME",        "Levels are implicit.",                  cat_game),
    CAT("signal",     "SIGNAL",      "Signals are geometry in time.",         cat_signal),
    CAT("mfg",        "MFG",         "Toolpaths are Minkowski sums.",         cat_mfg),
    CAT("cosmo",      "COSMO",       "The universe is a density field.",      cat_cosmo),
};
const int g_num_categories = sizeof(g_categories) / sizeof(g_categories[0]);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Engine state
 * ═══════════════════════════════════════════════════════════════════════════ */
static int cur_cat  = 0;
static int cur_item = 0;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Exported functions
 * ═══════════════════════════════════════════════════════════════════════════ */

EMSCRIPTEN_KEEPALIVE
uint8_t* get_buffer(void) { return (uint8_t*)pixels; }

EMSCRIPTEN_KEEPALIVE
int get_width(void) { return g_W; }

EMSCRIPTEN_KEEPALIVE
int get_height(void) { return g_H; }

EMSCRIPTEN_KEEPALIVE
void set_resolution(int w, int h) {
    if (w < 160) w = 160;
    if (h < 100) h = 100;
    if (w > MAX_W) w = MAX_W;
    if (h > MAX_H) h = MAX_H;
    surface.width  = w;
    surface.height = h;
    surface.stride = w;
    g_W = w;
    g_H = h;
}

EMSCRIPTEN_KEEPALIVE
void set_demo(int cat, int item) {
    if (cat >= 0 && cat < g_num_categories &&
        item >= 0 && item < g_categories[cat].count) {
        cur_cat = cat;
        cur_item = item;
    }
}

EMSCRIPTEN_KEEPALIVE
void set_mouse(float mx, float my) {
    g_mouseX = mx;
    g_mouseY = my;
}

EMSCRIPTEN_KEEPALIVE
void set_time_scale(float s) {
    if (s < 0.0f) s = 0.0f;
    if (s > 5.0f) s = 5.0f;
    time_scale = s;
}

EMSCRIPTEN_KEEPALIVE
float get_time_scale(void) { return time_scale; }

EMSCRIPTEN_KEEPALIVE
void set_auto_rotate(int on) { auto_rotate = on; }

EMSCRIPTEN_KEEPALIVE
int get_auto_rotate(void) { return auto_rotate; }

EMSCRIPTEN_KEEPALIVE
void render_frame(float t) {
    float scaled_t = t * time_scale;
    if (auto_rotate) {
        g_mouseX = scaled_t * 0.3f;
    }
    const demo_item_t *item = &g_categories[cur_cat].items[cur_item];
    gm_clear(15, 14, 12);
    if (item->frame) {
        item->frame(scaled_t);
    }
}

EMSCRIPTEN_KEEPALIVE
int get_num_categories(void) { return g_num_categories; }

EMSCRIPTEN_KEEPALIVE
int get_category_count(int cat) {
    if (cat >= 0 && cat < g_num_categories)
        return g_categories[cat].count;
    return 0;
}

EMSCRIPTEN_KEEPALIVE
const char* get_category_label(int cat) {
    if (cat >= 0 && cat < g_num_categories)
        return g_categories[cat].label;
    return "";
}

EMSCRIPTEN_KEEPALIVE
const char* get_category_thesis(int cat) {
    if (cat >= 0 && cat < g_num_categories)
        return g_categories[cat].thesis;
    return "";
}

EMSCRIPTEN_KEEPALIVE
const char* get_item_title(int cat, int item) {
    if (cat >= 0 && cat < g_num_categories &&
        item >= 0 && item < g_categories[cat].count)
        return g_categories[cat].items[item].title;
    return "";
}

EMSCRIPTEN_KEEPALIVE
const char* get_item_id(int cat, int item) {
    if (cat >= 0 && cat < g_num_categories &&
        item >= 0 && item < g_categories[cat].count)
        return g_categories[cat].items[item].id;
    return "";
}

EMSCRIPTEN_KEEPALIVE
int get_current_cat(void) { return cur_cat; }

EMSCRIPTEN_KEEPALIVE
int get_current_item(void) { return cur_item; }

int main(void) { return 0; }
