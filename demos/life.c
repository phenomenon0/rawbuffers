/*
 * life.c — Conway's Game of Life
 *
 * The cellular automaton IS double buffering:
 *   Read neighbors from front buffer (current generation)
 *   Write next state to back buffer (next generation)
 *   Swap.
 *
 * Click (space) to randomize. Arrow keys to scroll viewport.
 *
 * Build: gcc -O2 -o life demos/life.c -lm
 * Run:   ./life
 * Quit:  q or ESC
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <stdlib.h>
#include <string.h>

#define CELL_SIZE 2  /* pixels per cell */

static uint8_t *grid_a = NULL;
static uint8_t *grid_b = NULL;
static int gw, gh;
static int generation = 0;
static int alive_count = 0;
static bool paused = false;

static int cell(uint8_t *g, int x, int y) {
    /* Toroidal wrapping */
    x = (x + gw) % gw;
    y = (y + gh) % gh;
    return g[y * gw + x];
}

static void randomize(void) {
    for (int i = 0; i < gw * gh; i++)
        grid_a[i] = (rand() % 100) < 30 ? 1 : 0;
    generation = 0;
}

static void step(void) {
    alive_count = 0;
    for (int y = 0; y < gh; y++) {
        for (int x = 0; x < gw; x++) {
            int neighbors = cell(grid_a, x-1, y-1) + cell(grid_a, x, y-1) + cell(grid_a, x+1, y-1)
                          + cell(grid_a, x-1, y)                           + cell(grid_a, x+1, y)
                          + cell(grid_a, x-1, y+1) + cell(grid_a, x, y+1) + cell(grid_a, x+1, y+1);

            int alive = grid_a[y * gw + x];
            /* Conway's rules */
            if (alive && (neighbors == 2 || neighbors == 3))
                grid_b[y * gw + x] = 1;
            else if (!alive && neighbors == 3)
                grid_b[y * gw + x] = 1;
            else
                grid_b[y * gw + x] = 0;

            alive_count += grid_b[y * gw + x];
        }
    }
    /* Swap grids — the back buffer becomes the front */
    uint8_t *tmp = grid_a;
    grid_a = grid_b;
    grid_b = tmp;
    generation++;
}

static void tick(rb_ctx_t *ctx, float dt) {
    (void)dt;
    if (ctx->input.keys[RB_KEY_SPACE]) randomize();
    if (ctx->input.keys['p']) paused = !paused;

    if (!paused) step();
}

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;
    rb_clear(s, RB_RGB(8, 8, 12));

    /* Draw cells */
    for (int y = 0; y < gh; y++) {
        int py = y * CELL_SIZE;
        if (py >= s->height) break;
        for (int x = 0; x < gw; x++) {
            int px = x * CELL_SIZE;
            if (px >= s->width) break;
            if (grid_a[y * gw + x]) {
                /* Color by neighbor count for visual interest */
                int n = cell(grid_a, x-1, y-1) + cell(grid_a, x, y-1) + cell(grid_a, x+1, y-1)
                      + cell(grid_a, x-1, y)                           + cell(grid_a, x+1, y)
                      + cell(grid_a, x-1, y+1) + cell(grid_a, x, y+1) + cell(grid_a, x+1, y+1);
                uint32_t c;
                switch (n) {
                    case 2: c = RB_RGB(40, 180, 80); break;   /* stable = green */
                    case 3: c = RB_RGB(80, 220, 255); break;  /* birthing = cyan */
                    default: c = RB_RGB(200, 60, 60); break;  /* dying = red */
                }
                for (int dy = 0; dy < CELL_SIZE && py+dy < s->height; dy++)
                    for (int dx = 0; dx < CELL_SIZE && px+dx < s->width; dx++)
                        rb_pixel(s, px+dx, py+dy, c);
            }
        }
    }

    /* UI */
    char info[80];
    snprintf(info, sizeof(info), "Gen:%d  Alive:%d/%d  FPS:%d  %s",
             generation, alive_count, gw*gh, ctx->fps,
             paused ? "[PAUSED]" : "");
    rb_text(s, 4, 4, info, RB_RGB(200, 200, 200));
    rb_text(s, 4, 14, "space=randomize  p=pause  q=quit",
            RB_RGB(120, 120, 120));
}

int main(void) {
    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);

    gw = ctx.surface.width / CELL_SIZE;
    gh = ctx.surface.height / CELL_SIZE;
    grid_a = (uint8_t *)calloc(gw * gh, 1);
    grid_b = (uint8_t *)calloc(gw * gh, 1);

    srand((unsigned)42);
    randomize();

    rb_run(&ctx, (rb_callbacks_t){ .tick = tick, .draw = draw });

    free(grid_a);
    free(grid_b);
    rb_destroy(&ctx);
    return 0;
}
