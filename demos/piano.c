/*
 * piano.c — Keyboard piano with visual feedback
 *
 * Two keyboard rows mapped to notes. Polyphonic additive sine synthesis.
 * Audio callback sums active oscillators with simple attack/release envelope.
 *
 * Build: gcc -O2 -o piano demos/piano.c -lm -lpthread
 * Run:   ./piano
 * Quit:  q or ESC
 */

#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"
#include <math.h>
#include <string.h>

#define MAX_VOICES 8
#define SAMPLE_RATE 48000

/* Note frequencies (equal temperament, A4 = 440 Hz) */
static float note_freq(int midi) {
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

/* Key → MIDI mapping */
/* Bottom row: Z X C V B N M , = C4 D4 E4 F4 G4 A4 B4 C5 */
/* Home row:   A S D F G H J K L = C5 D5 E5 F5 G5 A5 B5 C6 C#6 */

typedef struct {
    int   key;        /* ASCII key code */
    int   midi;       /* MIDI note number */
    const char *name; /* note name for display */
} key_mapping_t;

static const key_mapping_t keymap[] = {
    { 'z', 60, "C4"  }, { 'x', 62, "D4"  }, { 'c', 64, "E4"  }, { 'v', 65, "F4"  },
    { 'b', 67, "G4"  }, { 'n', 69, "A4"  }, { 'm', 71, "B4"  }, { ',', 72, "C5"  },
    { 'a', 72, "C5"  }, { 's', 74, "D5"  }, { 'd', 76, "E5"  }, { 'f', 77, "F5"  },
    { 'g', 79, "G5"  }, { 'h', 81, "A5"  }, { 'j', 83, "B5"  }, { 'k', 84, "C6"  },
    { 'l', 85, "C#6" },
};
#define NUM_KEYS (int)(sizeof(keymap) / sizeof(keymap[0]))

/* Voice state */
typedef struct {
    float freq;
    float phase;
    float env;       /* envelope level 0..1 */
    bool  active;
    int   key;       /* which key triggered this voice */
} voice_t;

static voice_t voices[MAX_VOICES];
static volatile bool keys_active[256]; /* shared: render thread sets, audio reads */

static void audio_callback(float *samples, int nframes, int channels, void *user) {
    (void)user;

    for (int i = 0; i < nframes; i++) {
        float mix = 0.0f;

        for (int v = 0; v < MAX_VOICES; v++) {
            voice_t *vo = &voices[v];
            if (!vo->active && vo->env <= 0.001f) continue;

            /* Simple envelope: attack 5ms, release 30ms */
            if (vo->active) {
                vo->env += (1.0f - vo->env) * (1.0f - expf(-1.0f / (0.005f * SAMPLE_RATE)));
            } else {
                vo->env *= expf(-1.0f / (0.03f * SAMPLE_RATE));
                if (vo->env < 0.001f) { vo->env = 0; continue; }
            }

            vo->phase += vo->freq / SAMPLE_RATE;
            if (vo->phase > 1.0f) vo->phase -= 1.0f;

            /* Sine + slight 2nd harmonic for warmth */
            float s = sinf(vo->phase * 6.2831853f) * 0.8f
                    + sinf(vo->phase * 12.566370f) * 0.15f;
            mix += s * vo->env;
        }

        mix *= 0.15f; /* master volume */
        samples[i * channels]     = mix;
        samples[i * channels + 1] = mix;
    }
}

/* Allocate or reuse a voice for a key */
static void note_on(int key, float freq) {
    /* Already playing? */
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].active && voices[i].key == key) return;

    /* Find free voice (or steal quietest) */
    int best = 0;
    float best_env = 2.0f;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active && voices[i].env < 0.001f) { best = i; break; }
        if (voices[i].env < best_env) { best_env = voices[i].env; best = i; }
    }

    voices[best].freq = freq;
    voices[best].phase = 0;
    voices[best].active = true;
    voices[best].key = key;
}

static void note_off(int key) {
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].key == key) voices[i].active = false;
}

static void draw(rb_ctx_t *ctx) {
    rb_surface_t *s = &ctx->surface;
    rb_clear(s, RB_RGB(20, 20, 30));

    rb_text(s, 4, 4, "KEYBOARD PIANO", RB_RGB(255, 255, 255));
    rb_text(s, 4, 16, "Bottom: ZXCVBNM,  Home: ASDFGHJKL", RB_RGB(140, 140, 160));

    /* Update voices based on held keys */
    for (int i = 0; i < NUM_KEYS; i++) {
        bool down = rb_key_down(&ctx->input, keymap[i].key);
        if (down)
            note_on(keymap[i].key, note_freq(keymap[i].midi));
        else
            note_off(keymap[i].key);
    }

    /* Draw piano keys */
    int key_w = (s->width - 40) / NUM_KEYS;
    if (key_w < 10) key_w = 10;
    int key_h = s->height / 3;
    int start_x = (s->width - key_w * NUM_KEYS) / 2;
    int start_y = s->height / 2 - key_h / 2;

    for (int i = 0; i < NUM_KEYS; i++) {
        int kx = start_x + i * key_w;
        bool pressed = rb_key_down(&ctx->input, keymap[i].key);

        /* Key color: white when up, colored when pressed */
        uint32_t color;
        if (pressed) {
            float hue = (float)i / NUM_KEYS;
            color = rb_hue(hue);
        } else {
            /* Bottom row = ivory, home row = light blue */
            color = (i < 8) ? RB_RGB(240, 235, 220) : RB_RGB(200, 220, 240);
        }

        rb_rect_fill(s, kx + 1, start_y, key_w - 2, key_h, color);
        rb_rect(s, kx + 1, start_y, key_w - 2, key_h, RB_RGB(80, 80, 80));

        /* Key label */
        char label[4];
        snprintf(label, sizeof(label), "%c", keymap[i].key);
        uint32_t text_color = pressed ? RB_RGB(255, 255, 255) : RB_RGB(60, 60, 60);
        rb_text(s, kx + key_w / 2 - 3, start_y + key_h - 12, label, text_color);

        /* Note name above key */
        rb_text(s, kx + 2, start_y - 10, keymap[i].name, RB_RGB(160, 160, 180));
    }

    /* FPS */
    char fps[32];
    snprintf(fps, sizeof(fps), "FPS:%d  q=quit", ctx->fps);
    rb_text(s, 4, s->height - 12, fps, RB_RGB(120, 120, 120));
}

int main(void) {
    memset(voices, 0, sizeof(voices));

    rb_ctx_t ctx = rb_init(0, 0, RB_FB_TERM);
    rb_audio_start(&ctx, audio_callback, NULL);
    rb_run(&ctx, (rb_callbacks_t){ .draw = draw });
    rb_destroy(&ctx);
    return 0;
}
