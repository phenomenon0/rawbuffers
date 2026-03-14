/*
 * visualizer.c — Piano + FFT frequency visualizer (capstone demo)
 *
 * Same piano key mapping as piano.c. Audio callback copies samples
 * into a ring buffer for the render thread. Render thread runs
 * radix-2 Cooley-Tukey FFT and draws rainbow frequency bars.
 *
 * Build: gcc -O2 -o visualizer demos/visualizer.c -lm -lpthread
 * Run:   ./visualizer
 * Quit:  q or ESC
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>
#include <string.h>

#define MAX_VOICES   8
#define SAMPLE_RATE  48000
#define FFT_SIZE     1024

/* ─── Note frequencies ─── */
static float note_freq(int midi) {
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

/* ─── Key mapping (duplicated from piano.c — intentionally no shared header) ─── */
typedef struct { int key; int midi; const char *name; } key_mapping_t;
static const key_mapping_t keymap[] = {
    { 'z', 60, "C4"  }, { 'x', 62, "D4"  }, { 'c', 64, "E4"  }, { 'v', 65, "F4"  },
    { 'b', 67, "G4"  }, { 'n', 69, "A4"  }, { 'm', 71, "B4"  }, { ',', 72, "C5"  },
    { 'a', 72, "C5"  }, { 's', 74, "D5"  }, { 'd', 76, "E5"  }, { 'f', 77, "F5"  },
    { 'g', 79, "G5"  }, { 'h', 81, "A5"  }, { 'j', 83, "B5"  }, { 'k', 84, "C6"  },
    { 'l', 85, "C#6" },
};
#define NUM_KEYS (int)(sizeof(keymap) / sizeof(keymap[0]))

/* ─── Voices ─── */
typedef struct {
    float freq, phase, env;
    bool  active;
    int   key;
} voice_t;

static voice_t voices[MAX_VOICES];

/* ─── Ring buffer for audio → render thread ─── */
static rb_ring_t viz_ring;

static void audio_callback(float *samples, int nframes, int channels, void *user) {
    (void)user;

    for (int i = 0; i < nframes; i++) {
        float mix = 0.0f;

        for (int v = 0; v < MAX_VOICES; v++) {
            voice_t *vo = &voices[v];
            if (!vo->active && vo->env <= 0.001f) continue;

            if (vo->active)
                vo->env += (1.0f - vo->env) * (1.0f - expf(-1.0f / (0.005f * SAMPLE_RATE)));
            else {
                vo->env *= expf(-1.0f / (0.03f * SAMPLE_RATE));
                if (vo->env < 0.001f) { vo->env = 0; continue; }
            }

            vo->phase += vo->freq / SAMPLE_RATE;
            if (vo->phase > 1.0f) vo->phase -= 1.0f;

            float s = sinf(vo->phase * 6.2831853f) * 0.8f
                    + sinf(vo->phase * 12.566370f) * 0.15f;
            mix += s * vo->env;
        }

        mix *= 0.15f;
        samples[i * channels]     = mix;
        samples[i * channels + 1] = mix;

        /* Feed mono mix into viz ring as float */
        rb_ring_write(&viz_ring, &mix, sizeof(float));
    }
}

static void note_on(int key, float freq) {
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].active && voices[i].key == key) return;
    int best = 0; float best_env = 2.0f;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active && voices[i].env < 0.001f) { best = i; break; }
        if (voices[i].env < best_env) { best_env = voices[i].env; best = i; }
    }
    voices[best] = (voice_t){ .freq = freq, .phase = 0, .env = 0, .active = true, .key = key };
}

static void note_off(int key) {
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].key == key) voices[i].active = false;
}

/* FFT now provided by rb_fft() in rawbuf.h */

/* ─── FFT magnitudes (smoothed) ─── */
static float fft_mag[FFT_SIZE / 2];
static float fft_smooth[FFT_SIZE / 2];

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;
    rb_clear(s, RB_RGB(12, 12, 20));

    /* Update voices */
    for (int i = 0; i < NUM_KEYS; i++) {
        if (rb_key_down(&ctx->input, keymap[i].key))
            note_on(keymap[i].key, note_freq(keymap[i].midi));
        else
            note_off(keymap[i].key);
    }

    /* Read samples from viz ring */
    float samples[FFT_SIZE];
    size_t avail = rb_ring_available(&viz_ring);
    /* Drain to latest, keeping only last FFT_SIZE samples */
    if (avail > FFT_SIZE * sizeof(float)) {
        size_t skip = avail - FFT_SIZE * sizeof(float);
        float discard[256];
        while (skip > 0) {
            size_t chunk = skip < sizeof(discard) ? skip : sizeof(discard);
            rb_ring_read(&viz_ring, discard, chunk);
            skip -= chunk;
        }
    }
    size_t got = rb_ring_read(&viz_ring, samples, FFT_SIZE * sizeof(float));
    int n_samples = (int)(got / sizeof(float));

    /* Zero-pad if we didn't get enough */
    for (int i = n_samples; i < FFT_SIZE; i++) samples[i] = 0.0f;

    /* Apply Hann window */
    float re[FFT_SIZE], im[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++) {
        float w = 0.5f * (1.0f - cosf(6.2831853f * i / (FFT_SIZE - 1)));
        re[i] = samples[i] * w;
        im[i] = 0.0f;
    }

    rb_fft(re, im, FFT_SIZE);

    /* Compute magnitudes */
    for (int i = 0; i < FFT_SIZE / 2; i++) {
        fft_mag[i] = sqrtf(re[i] * re[i] + im[i] * im[i]) / (FFT_SIZE / 2);
        /* Exponential smoothing */
        fft_smooth[i] += (fft_mag[i] - fft_smooth[i]) * 0.3f;
    }

    /* ─── Draw FFT bars (bottom half) ─── */
    float sc = (float)s->width / 160.0f;
    if (sc < 1.0f) sc = 1.0f;
    int bar_area_y = s->height / 2;
    int bar_area_h = s->height / 2 - (int)(14 * sc);
    int bar_w = (int)(3 * sc); if (bar_w < 2) bar_w = 2;
    int bar_gap = (int)(1 * sc); if (bar_gap < 1) bar_gap = 1;
    int num_bars = s->width / (bar_w + bar_gap);
    if (num_bars > FFT_SIZE / 2) num_bars = FFT_SIZE / 2;

    for (int i = 0; i < num_bars; i++) {
        /* Log-scale frequency mapping */
        int bin = (int)(powf((float)i / num_bars, 2.0f) * (FFT_SIZE / 2 - 1));
        if (bin >= FFT_SIZE / 2) bin = FFT_SIZE / 2 - 1;

        float mag = fft_smooth[bin] * 8.0f; /* scale up for visibility */
        if (mag > 1.0f) mag = 1.0f;

        int bh = (int)(mag * bar_area_h);
        if (bh < 1) continue;

        int bx = i * (bar_w + bar_gap);
        int by = bar_area_y + bar_area_h - bh;
        uint32_t color = rb_hue((float)i / num_bars);
        rb_rect_fill(s, bx, by, bar_w, bh, color);
    }

    /* ─── Draw piano keys (top portion) ─── */
    int margin = (int)(6 * sc);
    int key_w = (s->width - (int)(40 * sc)) / NUM_KEYS;
    if (key_w < 10) key_w = 10;
    int key_h = s->height / 5;
    int kx0 = (s->width - key_w * NUM_KEYS) / 2;
    int ky0 = (int)(30 * sc);

    for (int i = 0; i < NUM_KEYS; i++) {
        int kx = kx0 + i * key_w;
        bool pressed = rb_key_down(&ctx->input, keymap[i].key);
        uint32_t color = pressed ? rb_hue((float)i / NUM_KEYS)
                                 : (i < 8 ? RB_RGB(240, 235, 220) : RB_RGB(200, 220, 240));
        rb_rect_fill(s, kx + 1, ky0, key_w - 2, key_h, color);
        rb_rect(s, kx + 1, ky0, key_w - 2, key_h, RB_RGB(80, 80, 80));

        char label[4];
        snprintf(label, sizeof(label), "%c", keymap[i].key);
        rb_text(s, kx + key_w / 2 - 3, ky0 + key_h - 12, label,
                pressed ? RB_RGB(255, 255, 255) : RB_RGB(60, 60, 60));
    }

    rb_text(s, margin, margin, "PIANO + FFT VISUALIZER", RB_RGB(255, 255, 255));
    rb_text(s, margin, margin + (int)(12 * sc), "ZXCVBNM, / ASDFGHJKL", RB_RGB(140, 140, 160));

    char info[64];
    snprintf(info, sizeof(info), "FPS:%d  q=quit", ctx->fps);
    rb_text(s, margin, s->height - (int)(12 * sc), info, RB_RGB(120, 120, 120));
}

int main(void) {
    memset(voices, 0, sizeof(voices));
    memset(fft_smooth, 0, sizeof(fft_smooth));
    viz_ring = rb_ring_create(FFT_SIZE * sizeof(float) * 4);

    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);
    rb_audio_start(&ctx, audio_callback, NULL);
    rb_run(&ctx, (rb_callbacks_t){ .draw = draw });
    rb_destroy(&ctx);
    rb_ring_destroy(&viz_ring);
    return 0;
}
