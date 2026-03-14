/*
 * player.c — Music player: pipe spawn + audio ring + FFT viz + mouse seek bar
 *
 * Usage: ./player song.mp3
 * Quit:  q or ESC
 *
 * Pipes ffmpeg decode → ring buffer → aplay. Taps the ring for FFT
 * visualization. Mouse click on seek bar restarts at new position.
 *
 * Build: gcc -O2 -o player demos/player.c -lm -lpthread
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define FFT_SIZE 1024

static rb_pipe_t   *decode_pipe;
static rb_ring_t    audio_ring;
static rb_ring_t    viz_ring;
static volatile bool splitter_running;
static pthread_t    splitter_thread;

static float fft_smooth[FFT_SIZE / 2];
static float elapsed_sec;
static float duration_sec;
static bool  paused;
static const char *filename;

/* Splitter: read from decode_pipe->ring, write to both audio_ring and viz_ring */
static void *splitter_func(void *arg) {
    (void)arg;
    uint8_t tmp[4096];
    while (splitter_running) {
        if (!decode_pipe) break;
        size_t got = rb_ring_read(&decode_pipe->ring, tmp, sizeof(tmp));
        if (got == 0) {
            if (!decode_pipe->running) break;
            usleep(500);
            continue;
        }
        size_t off = 0;
        while (off < got && splitter_running) {
            size_t w = rb_ring_write(&audio_ring, tmp + off, got - off);
            if (w == 0) { usleep(100); continue; }
            off += w;
        }
        rb_ring_write(&viz_ring, tmp, got);
        elapsed_sec += (float)got / (48000.0f * 2 * sizeof(int16_t));
    }
    return NULL;
}

static float get_duration(const char *path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "ffprobe -v quiet -show_entries format=duration -of csv=p=0 '%s' 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;
    float dur = 0;
    if (fscanf(fp, "%f", &dur) != 1) dur = 0;
    pclose(fp);
    return dur;
}

static void start_decode(const char *path, float seek_sec) {
    char cmd[512];
    if (seek_sec > 0)
        snprintf(cmd, sizeof(cmd),
            "ffmpeg -ss %.1f -i '%s' -f s16le -ar 48000 -ac 2 pipe:1 2>/dev/null", seek_sec, path);
    else
        snprintf(cmd, sizeof(cmd),
            "ffmpeg -i '%s' -f s16le -ar 48000 -ac 2 pipe:1 2>/dev/null", path);
    decode_pipe = rb_pipe_spawn(cmd, 1 << 18);
    elapsed_sec = seek_sec;
}

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;
    int W = s->width, H = s->height;
    rb_clear(s, RB_RGB(15, 15, 25));

    /* Scale factor: 1.0 at 160px wide, grows with screen */
    float sc = (float)W / 160.0f;
    if (sc < 1.0f) sc = 1.0f;
    int text_gap = (int)(10 * sc);  /* vertical gap between text lines */
    int margin = (int)(6 * sc);

    /* Read samples from viz ring for FFT */
    float samples[FFT_SIZE];
    size_t avail = rb_ring_available(&viz_ring);
    if (avail > FFT_SIZE * sizeof(int16_t)) {
        size_t skip = avail - FFT_SIZE * sizeof(int16_t);
        int16_t discard[256];
        while (skip > 0) {
            size_t chunk = skip < sizeof(discard) ? skip : sizeof(discard);
            rb_ring_read(&viz_ring, discard, chunk);
            skip -= chunk;
        }
    }
    int16_t raw[FFT_SIZE];
    size_t got = rb_ring_read(&viz_ring, raw, FFT_SIZE * sizeof(int16_t));
    int n_samples = (int)(got / sizeof(int16_t));
    for (int i = 0; i < n_samples; i++) samples[i] = raw[i] / 32768.0f;
    for (int i = n_samples; i < FFT_SIZE; i++) samples[i] = 0.0f;

    float re[FFT_SIZE], im[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++) {
        float w = 0.5f * (1.0f - cosf(6.2831853f * i / (FFT_SIZE - 1)));
        re[i] = samples[i] * w;
        im[i] = 0.0f;
    }
    rb_fft(re, im, FFT_SIZE);

    for (int i = 0; i < FFT_SIZE / 2; i++) {
        float mag = sqrtf(re[i] * re[i] + im[i] * im[i]) / (FFT_SIZE / 2);
        fft_smooth[i] += (mag - fft_smooth[i]) * 0.3f;
    }

    /* FFT bars — scale bar width with screen */
    int bar_top = text_gap * 3;
    int bar_bot_margin = (int)(30 * sc);
    int bar_area_h = H - bar_top - bar_bot_margin;
    int bar_w_each = (int)(3 * sc); if (bar_w_each < 2) bar_w_each = 2;
    int bar_gap = (int)(1 * sc); if (bar_gap < 1) bar_gap = 1;
    int num_bars = W / (bar_w_each + bar_gap);
    if (num_bars > FFT_SIZE / 2) num_bars = FFT_SIZE / 2;

    for (int i = 0; i < num_bars; i++) {
        int bin = (int)(powf((float)i / num_bars, 2.0f) * (FFT_SIZE / 2 - 1));
        if (bin >= FFT_SIZE / 2) bin = FFT_SIZE / 2 - 1;
        float mag = fft_smooth[bin] * 8.0f;
        if (mag > 1.0f) mag = 1.0f;
        int bh = (int)(mag * bar_area_h);
        if (bh < 1) continue;
        int bx = i * (bar_w_each + bar_gap);
        int by = bar_top + bar_area_h - bh;
        rb_rect_fill(s, bx, by, bar_w_each, bh, rb_hue((float)i / num_bars * 0.7f));
    }

    /* Seek bar — scaled */
    int seek_h = (int)(10 * sc); if (seek_h < 6) seek_h = 6;
    int seek_y = H - seek_h - margin;
    int seek_x = margin * 2;
    int seek_w = W - margin * 4;
    rb_rect_fill(s, seek_x, seek_y, seek_w, seek_h, RB_RGB(40, 40, 50));
    if (duration_sec > 0) {
        float pct = elapsed_sec / duration_sec;
        if (pct > 1.0f) pct = 1.0f;
        int fill_w = (int)(pct * seek_w);
        rb_rect_fill(s, seek_x, seek_y, fill_w, seek_h, RB_RGB(80, 180, 255));
        int head_w = (int)(3 * sc); if (head_w < 2) head_w = 2;
        rb_rect_fill(s, seek_x + fill_w - head_w/2, seek_y - 2,
                     head_w, seek_h + 4, RB_RGB(255, 255, 255));
    }
    rb_rect(s, seek_x, seek_y, seek_w, seek_h, RB_RGB(80, 80, 100));

    /* Mouse click on seek bar */
    if (rb_mouse_pressed(&ctx->input, RB_MOUSE_LEFT) && duration_sec > 0) {
        int mx = ctx->input.mouse_x;
        int my = ctx->input.mouse_y;
        if (rb_point_in_rect(mx, my, seek_x, seek_y - 6, seek_w, seek_h + 12)) {
            float seek_pct = (float)(mx - seek_x) / seek_w;
            float seek_to = seek_pct * duration_sec;
            splitter_running = false;
            pthread_join(splitter_thread, NULL);
            rb_pipe_stop(decode_pipe);
            decode_pipe = NULL;
            uint8_t drain[1024];
            while (rb_ring_read(&audio_ring, drain, sizeof(drain)) > 0);
            while (rb_ring_read(&viz_ring, drain, sizeof(drain)) > 0);
            start_decode(filename, seek_to);
            splitter_running = true;
            pthread_create(&splitter_thread, NULL, splitter_func, NULL);
        }
    }

    if (rb_key_pressed(&ctx->input, RB_KEY_SPACE))
        paused = !paused;

    /* Title + info */
    rb_text(s, margin, margin, "PLAYER", RB_RGB(255, 255, 255));
    char info[128];
    snprintf(info, sizeof(info), "%.0f / %.0f sec  %s  FPS:%d  q=quit  SPACE=pause",
             elapsed_sec, duration_sec, paused ? "[PAUSED]" : "", ctx->fps);
    rb_text(s, margin, margin + text_gap, info, RB_RGB(140, 140, 160));
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <audio-file>\n", argv[0]);
        return 1;
    }
    filename = argv[1];
    duration_sec = get_duration(filename);
    memset(fft_smooth, 0, sizeof(fft_smooth));

    audio_ring = rb_ring_create(1 << 18);
    viz_ring = rb_ring_create(FFT_SIZE * sizeof(int16_t) * 4);

    start_decode(filename, 0);
    splitter_running = true;
    pthread_create(&splitter_thread, NULL, splitter_func, NULL);

    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);
    rb_audio_start_ring(&ctx, &audio_ring);
    rb_run(&ctx, (rb_callbacks_t){ .draw = draw });

    splitter_running = false;
    pthread_join(splitter_thread, NULL);
    rb_pipe_stop(decode_pipe);
    rb_destroy(&ctx);
    rb_ring_destroy(&audio_ring);
    rb_ring_destroy(&viz_ring);
    return 0;
}
