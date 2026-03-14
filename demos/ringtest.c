/*
 * ringtest.c — Visual ring buffer throughput meter
 *
 * Two threads: producer fills ring at sine-modulated speed,
 * consumer drains at fixed speed. Terminal renders fill level + MB/s.
 *
 * Build: gcc -O2 -o ringtest demos/ringtest.c -lm -lpthread
 * Run:   ./ringtest
 * Quit:  q or ESC
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>
#include <pthread.h>

#define RING_SIZE (1 << 16)  /* 64 KB */
#define CHUNK     512

static rb_ring_t ring;
static volatile bool prod_running = true;
static volatile size_t total_written = 0;
static volatile size_t total_read = 0;

static void *producer_thread(void *arg) {
    (void)arg;
    uint8_t buf[CHUNK];
    memset(buf, 0xAB, CHUNK);
    double start = rb_time();

    while (prod_running) {
        /* Sine-modulated write speed: 0.2x to 1.0x */
        double t = rb_time() - start;
        double speed = 0.6 + 0.4 * sin(t * 1.5);
        int write_sz = (int)(CHUNK * speed);
        if (write_sz < 1) write_sz = 1;

        size_t n = rb_ring_write(&ring, buf, (size_t)write_sz);
        total_written += n;
        usleep(200);  /* ~5000 writes/sec max */
    }
    return NULL;
}

static void *consumer_thread(void *arg) {
    (void)arg;
    uint8_t buf[CHUNK];

    while (prod_running) {
        size_t n = rb_ring_read(&ring, buf, CHUNK / 2);
        total_read += n;
        usleep(300);  /* slightly slower than producer at peak */
    }
    return NULL;
}

static double last_sample_time;
static size_t last_sample_written;
static double throughput_mbps = 0.0;

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;
    rb_clear(s, RB_RGB(16, 16, 24));

    /* Throughput calculation (smoothed over ~0.5s) */
    double now = ctx->time;
    if (now - last_sample_time > 0.5) {
        size_t bytes = total_written - last_sample_written;
        throughput_mbps = (double)bytes / (now - last_sample_time) / (1024.0 * 1024.0);
        last_sample_written = total_written;
        last_sample_time = now;
    }

    /* Fill level bar */
    size_t avail = rb_ring_available(&ring);
    float fill = (float)avail / (float)ring.capacity;

    int bar_x = 20;
    int bar_y = s->height / 2 - 8;
    int bar_w = s->width - 40;
    int bar_h = 16;

    /* Background */
    rb_rect_fill(s, bar_x, bar_y, bar_w, bar_h, RB_RGB(40, 40, 50));
    /* Fill */
    int fill_w = (int)(fill * bar_w);
    uint32_t fill_color = fill < 0.7f ? rb_hue(0.35f - fill * 0.3f) : RB_RGB(255, 80, 60);
    rb_rect_fill(s, bar_x, bar_y, fill_w, bar_h, fill_color);
    /* Border */
    rb_rect(s, bar_x, bar_y, bar_w, bar_h, RB_RGB(200, 200, 200));

    /* Labels */
    char line[80];
    snprintf(line, sizeof(line), "Ring Buffer: %zu / %zu bytes  (%.0f%% full)",
             avail, ring.capacity, fill * 100.0f);
    rb_text(s, bar_x, bar_y - 14, line, RB_RGB(220, 220, 255));

    snprintf(line, sizeof(line), "Throughput: %.2f MB/s", throughput_mbps);
    rb_text(s, bar_x, bar_y + bar_h + 6, line, RB_RGB(180, 255, 180));

    size_t wpos = atomic_load(&ring.write_pos);
    size_t rpos = atomic_load(&ring.read_pos);
    snprintf(line, sizeof(line), "write_pos: %zu  read_pos: %zu  mmap: %s",
             wpos, rpos, ring.mmap_backed ? "yes" : "no");
    rb_text(s, bar_x, bar_y + bar_h + 18, line, RB_RGB(160, 160, 180));

    rb_text(s, bar_x, 8, "RING BUFFER THROUGHPUT METER", RB_RGB(255, 255, 255));
    rb_text(s, bar_x, 20, "Producer: sine-modulated speed   Consumer: fixed speed",
            RB_RGB(140, 140, 160));

    /* FPS */
    snprintf(line, sizeof(line), "FPS:%d  q=quit", ctx->fps);
    rb_text(s, 4, s->height - 12, line, RB_RGB(120, 120, 120));
}

int main(void) {
    ring = rb_ring_create(RING_SIZE);

    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer_thread, NULL);
    pthread_create(&cons, NULL, consumer_thread, NULL);

    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);
    last_sample_time = rb_time();
    last_sample_written = 0;

    rb_run(&ctx, (rb_callbacks_t){ .draw = draw });

    prod_running = false;
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    rb_destroy(&ctx);
    rb_ring_destroy(&ring);
    return 0;
}
