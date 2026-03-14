/*
 * bounce.c — Bouncing ball with keyboard controls
 *
 * Arrow keys / WASD: push the ball
 * Space: spawn explosion of particles
 * r: randomize ball color
 *
 * Demonstrates: input handling, double buffering, animation, drawing primitives
 *
 * Build: gcc -O2 -o bounce demos/bounce.c -lm
 * Run:   ./bounce
 * Quit:  q or ESC
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>
#include <stdlib.h>

#define MAX_PARTICLES 256

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    uint32_t color;
} particle_t;

static struct {
    float x, y;
    float vx, vy;
    float radius;
    uint32_t color;
    particle_t particles[MAX_PARTICLES];
    int num_particles;
    float trail_hue;
} state;

static void spawn_particles(float x, float y, int count) {
    for (int i = 0; i < count && state.num_particles < MAX_PARTICLES; i++) {
        particle_t *p = &state.particles[state.num_particles++];
        float angle = (float)(rand() % 628) / 100.0f;
        float speed = 20.0f + (float)(rand() % 80);
        p->x = x;
        p->y = y;
        p->vx = cosf(angle) * speed;
        p->vy = sinf(angle) * speed;
        p->life = 0.5f + (float)(rand() % 100) / 100.0f;
        p->color = rb_hue((float)(rand() % 100) / 100.0f);
    }
}

static void tick(rb_ctx_t *ctx, float dt) {
    rb_surface_t *s = &ctx->surface;
    float push = 200.0f;

    /* Input → velocity */
    if (ctx->input.keys['w'] || ctx->input.keys[RB_KEY_UP])    state.vy -= push * dt;
    if (ctx->input.keys['s'] || ctx->input.keys[RB_KEY_DOWN])  state.vy += push * dt;
    if (ctx->input.keys['a'] || ctx->input.keys[RB_KEY_LEFT])  state.vx -= push * dt;
    if (ctx->input.keys['d'] || ctx->input.keys[RB_KEY_RIGHT]) state.vx += push * dt;
    if (ctx->input.keys[RB_KEY_SPACE]) spawn_particles(state.x, state.y, 20);
    if (ctx->input.keys['r']) state.color = rb_hue((float)(rand() % 100) / 100.0f);

    /* Physics */
    float gravity = 120.0f;
    float friction = 0.995f;
    float bounce_loss = 0.8f;

    state.vy += gravity * dt;
    state.vx *= friction;
    state.vy *= friction;
    state.x += state.vx * dt;
    state.y += state.vy * dt;

    /* Bounce off walls */
    if (state.x - state.radius < 0) {
        state.x = state.radius;
        state.vx = -state.vx * bounce_loss;
        spawn_particles(state.x, state.y, 5);
    }
    if (state.x + state.radius >= s->width) {
        state.x = s->width - state.radius - 1;
        state.vx = -state.vx * bounce_loss;
        spawn_particles(state.x, state.y, 5);
    }
    if (state.y - state.radius < 0) {
        state.y = state.radius;
        state.vy = -state.vy * bounce_loss;
        spawn_particles(state.x, state.y, 5);
    }
    if (state.y + state.radius >= s->height) {
        state.y = s->height - state.radius - 1;
        state.vy = -state.vy * bounce_loss;
        spawn_particles(state.x, state.y, 8);
    }

    /* Update particles */
    for (int i = 0; i < state.num_particles; ) {
        particle_t *p = &state.particles[i];
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vy += gravity * 0.5f * dt;
        p->life -= dt;
        if (p->life <= 0) {
            state.particles[i] = state.particles[--state.num_particles];
        } else {
            i++;
        }
    }

    state.trail_hue += dt * 0.1f;
}

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;

    /* Dark background with subtle gradient */
    for (int y = 0; y < s->height; y++) {
        uint8_t v = (uint8_t)(y * 20 / s->height);
        for (int x = 0; x < s->width; x++)
            rb_pixel(s, x, y, RB_RGB(v, v/2, v));
    }

    /* Particles */
    for (int i = 0; i < state.num_particles; i++) {
        particle_t *p = &state.particles[i];
        float alpha = p->life;
        if (alpha > 1.0f) alpha = 1.0f;
        uint32_t c = rb_lerp_color(RB_RGB(20,10,10), p->color, alpha);
        int size = (int)(alpha * 2.0f) + 1;
        rb_rect_fill(s, (int)p->x - size/2, (int)p->y - size/2, size, size, c);
    }

    /* Ball shadow */
    rb_circle_fill(s, (int)state.x + 2, (int)state.y + 2,
                   (int)state.radius, RB_RGB(10, 10, 10));

    /* Ball */
    rb_circle_fill(s, (int)state.x, (int)state.y,
                   (int)state.radius, state.color);

    /* Ball highlight */
    rb_circle_fill(s, (int)state.x - 2, (int)state.y - 3,
                   (int)(state.radius * 0.3f), RB_RGB(255, 255, 255));

    /* Velocity vector */
    rb_line(s, (int)state.x, (int)state.y,
            (int)(state.x + state.vx * 0.1f),
            (int)(state.y + state.vy * 0.1f),
            RB_RGB(255, 255, 0));

    /* UI */
    char info[64];
    snprintf(info, sizeof(info), "FPS:%d  vel:(%.0f,%.0f)  particles:%d",
             ctx->fps, state.vx, state.vy, state.num_particles);
    rb_text(s, 4, 4, info, RB_RGB(200, 200, 200));
    rb_text(s, 4, 14, "WASD/arrows=push  space=explode  r=color  q=quit",
            RB_RGB(140, 140, 140));

    /* Border */
    rb_rect(s, 0, 0, s->width, s->height, RB_RGB(60, 60, 80));
}

int main(void) {
    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);

    /* Initial state */
    state.x = ctx.surface.width / 2.0f;
    state.y = ctx.surface.height / 3.0f;
    state.vx = 60.0f;
    state.vy = 0.0f;
    state.radius = 6.0f;
    state.color = RB_RGB(80, 180, 255);

    rb_run(&ctx, (rb_callbacks_t){ .tick = tick, .draw = draw });
    rb_destroy(&ctx);
    return 0;
}
