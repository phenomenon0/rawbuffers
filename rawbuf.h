/*
 * rawbuf.h — The buffer hacker's toolkit for Linux
 *
 * Single-header library. Zero dependencies (except -lm, -lpthread).
 * Define RAWBUF_IMPLEMENTATION in exactly one .c file before including.
 *
 * What's here:
 *   - Terminal surface + double buffer + drawing primitives
 *   - Input with held-key tracking + Ctrl+key support
 *   - Lock-free SPSC ring buffer (mmap double-map trick)
 *   - Audio output via aplay pipe + synthesis thread
 *   - Blit, scroll, color-invert operations
 *
 * Usage:
 *   #define RAWBUF_IMPLEMENTATION
 *   #include "rawbuf.h"
 *
 *   void draw(rb_ctx_t *ctx) {
 *       rb_clear(&ctx->surface, RB_RGB(0,0,0));
 *       rb_pixel(&ctx->surface, 10, 10, RB_RGB(255,0,0));
 *   }
 *
 *   int main(void) {
 *       rb_ctx_t ctx = rb_init(160, 80, RB_FB_TERM);
 *       rb_run(&ctx, (rb_callbacks_t){ .draw = draw });
 *       rb_destroy(&ctx);
 *   }
 */

#ifndef RAWBUF_H
#define RAWBUF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sys/types.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 1: Types & API declarations
 * ═══════════════════════════════════════════════════════════════════════════ */

/* --- Colors --- */
#define RB_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) << 8)  |  (uint32_t)(b))
#define RB_RGB(r, g, b) RB_RGBA(r, g, b, 0xFF)

#define RB_R(c) (((c) >> 16) & 0xFF)
#define RB_G(c) (((c) >>  8) & 0xFF)
#define RB_B(c) (((c)      ) & 0xFF)
#define RB_A(c) (((c) >> 24) & 0xFF)

/* --- Flags --- */
#define RB_FB_TERM    0x02   /* terminal backend (default) */
#define RB_FB_RAW     0x01   /* /dev/fb0 backend (future) */
#define RB_INPUT_TERM 0x10   /* terminal raw mode input (default) */

/* --- Key codes (Linux input-event-codes subset + terminal specials) --- */
#define RB_KEY_ESC       27
#define RB_KEY_ENTER     10
#define RB_KEY_SPACE     32
#define RB_KEY_BACKSPACE 127
#define RB_KEY_UP        256
#define RB_KEY_DOWN      257
#define RB_KEY_LEFT      258
#define RB_KEY_RIGHT     259

/* Ctrl+key: terminal sends 0x01-0x1A for Ctrl+A through Ctrl+Z */
#define RB_KEY_CTRL(c) ((c) - 'a' + 1)

/* Frames a key stays "held" after the terminal last reported it */
#define RB_HOLD_FRAMES  6

/* Printable ASCII keys are their own code: 'a'=97, 'A'=65, '0'=48, etc */

/* --- Surface (framebuffer) --- */
typedef struct {
    uint32_t *front;       /* what's on screen now */
    uint32_t *back;        /* what you draw to */
    int       width;
    int       height;
    int       stride;      /* pixels per row (== width for now) */
    size_t    size;         /* total bytes per buffer */
    int       mode;         /* RB_FB_TERM or RB_FB_RAW */
    int       fd;           /* fb0 fd or -1 */
} rb_surface_t;

/* --- Mouse buttons --- */
#define RB_MOUSE_LEFT   0x01
#define RB_MOUSE_RIGHT  0x02
#define RB_MOUSE_MIDDLE 0x04

/* --- Input --- */
typedef struct {
    uint8_t  keys[512];       /* impulse: set on press, cleared next frame */
    uint8_t  prev_keys[512];  /* previous frame's impulse keys */
    uint8_t  held[512];       /* decay counter: nonzero = key is down */
    int      mouse_x, mouse_y;
    int      mouse_dx, mouse_dy;
    uint8_t  mouse_btn;       /* bitmask: bit 0=left, 1=right, 2=middle */
    uint8_t  prev_mouse_btn;  /* previous frame for edge detection */
    bool     quit_requested;
    int      mode;
} rb_input_t;

/* --- Ring Buffer (SPSC, lock-free) --- */
typedef struct {
    uint8_t       *buf;
    size_t         capacity;     /* power of 2 */
    _Atomic size_t write_pos;
    _Atomic size_t read_pos;
    bool           mmap_backed;  /* true = double-map trick, false = heap */
} rb_ring_t;

/* --- Audio --- */
typedef void (*rb_audio_fn)(float *samples, int nframes, int channels, void *user);
struct rb_audio;  /* opaque — defined in implementation */

/* --- Context --- */
typedef struct {
    rb_surface_t    surface;
    rb_input_t      input;
    double          time;       /* seconds since init */
    double          dt;         /* seconds since last frame */
    int             frame;      /* frame counter */
    int             fps;        /* measured FPS */
    bool            running;
    int             target_fps;
    struct rb_audio *audio;     /* NULL until rb_audio_start() */
} rb_ctx_t;

/* --- Callbacks --- */
typedef struct {
    void (*tick)(rb_ctx_t *ctx, float dt);  /* fixed timestep logic (optional) */
    void (*draw)(rb_ctx_t *ctx);            /* render each frame */
} rb_callbacks_t;

/* --- Core API --- */
rb_ctx_t  rb_init(int width, int height, int flags);
void      rb_destroy(rb_ctx_t *ctx);
void      rb_run(rb_ctx_t *ctx, rb_callbacks_t cb);
double    rb_time(void);

/* --- Surface --- */
static inline void rb_pixel(rb_surface_t *s, int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)s->width && (unsigned)y < (unsigned)s->height)
        s->back[y * s->stride + x] = c;
}

static inline uint32_t rb_peek(rb_surface_t *s, int x, int y) {
    if ((unsigned)x < (unsigned)s->width && (unsigned)y < (unsigned)s->height)
        return s->back[y * s->stride + x];
    return 0;
}

void rb_clear(rb_surface_t *s, uint32_t color);
void rb_present(rb_surface_t *s);

/* --- Drawing primitives --- */
void rb_line(rb_surface_t *s, int x0, int y0, int x1, int y1, uint32_t c);
void rb_rect(rb_surface_t *s, int x, int y, int w, int h, uint32_t c);
void rb_rect_fill(rb_surface_t *s, int x, int y, int w, int h, uint32_t c);
void rb_circle(rb_surface_t *s, int cx, int cy, int r, uint32_t c);
void rb_circle_fill(rb_surface_t *s, int cx, int cy, int r, uint32_t c);
void rb_text(rb_surface_t *s, int x, int y, const char *str, uint32_t c);

/* --- Blit & scroll --- */
void rb_blit(rb_surface_t *dst, int dx, int dy,
             const uint32_t *src, int sw, int sh, int src_stride);
void rb_blit_alpha(rb_surface_t *dst, int dx, int dy,
                   const uint32_t *src, int sw, int sh, int src_stride);
void rb_scroll(rb_surface_t *s, int dx, int dy, uint32_t fill);
void rb_rect_inv(rb_surface_t *s, int x, int y, int w, int h);

/* --- Color helpers --- */
uint32_t rb_hue(float h);                           /* h in [0,1] → rainbow */
uint32_t rb_lerp_color(uint32_t a, uint32_t b, float t);

/* --- Ring buffer --- */
rb_ring_t rb_ring_create(size_t capacity);
void      rb_ring_destroy(rb_ring_t *r);
size_t    rb_ring_write(rb_ring_t *r, const void *data, size_t len);
size_t    rb_ring_read(rb_ring_t *r, void *out, size_t len);
size_t    rb_ring_available(rb_ring_t *r);
size_t    rb_ring_space(rb_ring_t *r);

/* --- Audio --- */
void rb_audio_start(rb_ctx_t *ctx, rb_audio_fn callback, void *userdata);
void rb_audio_stop(rb_ctx_t *ctx);

/* --- Input helpers --- */
static inline bool rb_key_down(rb_input_t *inp, int key) {
    return key >= 0 && key < 512 && inp->held[key] > 0;
}
static inline bool rb_key_pressed(rb_input_t *inp, int key) {
    return key >= 0 && key < 512 && inp->keys[key] && !inp->prev_keys[key];
}

/* --- Mouse helpers --- */
static inline bool rb_mouse_down(rb_input_t *inp, uint8_t btn) {
    return (inp->mouse_btn & btn) != 0;
}
static inline bool rb_mouse_pressed(rb_input_t *inp, uint8_t btn) {
    return (inp->mouse_btn & btn) && !(inp->prev_mouse_btn & btn);
}
static inline bool rb_mouse_released(rb_input_t *inp, uint8_t btn) {
    return !(inp->mouse_btn & btn) && (inp->prev_mouse_btn & btn);
}

/* --- Collision helpers --- */
static inline bool rb_overlap_rect(int x1,int y1,int w1,int h1,
                                   int x2,int y2,int w2,int h2) {
    return x1 < x2+w2 && x1+w1 > x2 && y1 < y2+h2 && y1+h1 > y2;
}
static inline bool rb_overlap_circle(int x1,int y1,int r1,
                                     int x2,int y2,int r2) {
    int dx = x2-x1, dy = y2-y1, dr = r1+r2;
    return dx*dx + dy*dy <= dr*dr;
}
static inline bool rb_point_in_rect(int px,int py, int rx,int ry,int rw,int rh) {
    return px >= rx && px < rx+rw && py >= ry && py < ry+rh;
}

/* --- Pipe spawn (process stdout → ring buffer) --- */
typedef struct {
    rb_ring_t     ring;
    pid_t         child_pid;
    pthread_t     thread;
    int           pipe_fd;
    volatile bool running;
} rb_pipe_t;

rb_pipe_t *rb_pipe_spawn(const char *cmd, size_t ring_capacity);
void       rb_pipe_stop(rb_pipe_t *p);  /* stops, cleans up, and frees */

/* --- Audio ring playback (ring buffer → aplay) --- */
void rb_audio_start_ring(rb_ctx_t *ctx, rb_ring_t *source);

/* --- FFT utility --- */
void rb_fft(float *re, float *im, int n);

/* --- Sprite frame helper --- */
typedef struct {
    const uint32_t *pixels;
    int frame_w, frame_h;
    int sheet_stride;
    int cols;               /* frames per row in sheet */
} rb_sprite_t;

static inline void rb_sprite_draw(rb_surface_t *dst, int dx, int dy,
                                  const rb_sprite_t *spr, int frame) {
    int fx = (frame % spr->cols) * spr->frame_w;
    int fy = (frame / spr->cols) * spr->frame_h;
    rb_blit_alpha(dst, dx, dy,
                  spr->pixels + fy * spr->sheet_stride + fx,
                  spr->frame_w, spr->frame_h, spr->sheet_stride);
}

#endif /* RAWBUF_H */


/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 2: Implementation
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifdef RAWBUF_IMPLEMENTATION

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <math.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

/* ─── Timing ─── */

static struct timeval rb__start_time;

static void rb__init_time(void) {
    gettimeofday(&rb__start_time, NULL);
}

double rb_time(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - rb__start_time.tv_sec)
         + (now.tv_usec - rb__start_time.tv_usec) * 1e-6;
}

static void rb__sleep_us(int64_t us) {
    if (us > 0) usleep((useconds_t)us);
}

/* ─── Terminal State ─── */

static struct termios rb__orig_termios;
static bool rb__termios_saved = false;
static volatile sig_atomic_t rb__got_sigint = 0;
static volatile sig_atomic_t rb__got_sigwinch = 0;

static void rb__sigint_handler(int sig) {
    (void)sig;
    rb__got_sigint = 1;
}

static void rb__sigwinch_handler(int sig) {
    (void)sig;
    rb__got_sigwinch = 1;
}

static void rb__term_raw_enter(void) {
    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &rb__orig_termios);
    rb__termios_saved = true;

    struct termios raw = rb__orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    /* Hide cursor, switch to alt screen, enable SGR mouse tracking */
    write(STDOUT_FILENO, "\033[?1049h\033[?25l\033[?1003h\033[?1006h", 28);
}

static void rb__term_raw_exit(void) {
    /* Disable SGR mouse, show cursor, switch back from alt screen */
    write(STDOUT_FILENO, "\033[?1006l\033[?1003l\033[?25h\033[?1049l", 28);

    if (rb__termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &rb__orig_termios);
        rb__termios_saved = false;
    }
}

/* ─── Terminal Surface ─── */

/*
 * Terminal rendering uses Unicode half-block characters (▀ U+2580).
 * Each terminal cell represents TWO vertical pixels:
 *   - foreground color = top pixel
 *   - background color = bottom pixel
 *
 * This doubles the effective vertical resolution.
 * A 160×80 pixel surface fits in an 160×40 cell terminal.
 */

static void rb__surface_init_term(rb_surface_t *s, int w, int h) {
    /* Round height to even for half-block rendering */
    h = (h + 1) & ~1;

    s->width  = w;
    s->height = h;
    s->stride = w;
    s->size   = (size_t)w * h * sizeof(uint32_t);
    s->mode   = RB_FB_TERM;
    s->fd     = -1;
    s->front  = (uint32_t *)calloc(w * h, sizeof(uint32_t));
    s->back   = (uint32_t *)calloc(w * h, sizeof(uint32_t));

    if (!s->front || !s->back) {
        fprintf(stderr, "rawbuf: failed to allocate %zu bytes for surface\n", s->size * 2);
        exit(1);
    }

    /* Mark all front pixels as "dirty" so first present draws everything */
    memset(s->front, 0xFF, s->size);
}

static void rb__surface_destroy(rb_surface_t *s) {
    free(s->front);
    free(s->back);
    s->front = s->back = NULL;
}

/*
 * Present: diff back vs front, emit ANSI escape codes for changed cells.
 *
 * Each cell = 2 vertical pixels packed into one half-block character.
 * We use \033[38;2;R;G;Bm for foreground (top pixel)
 * and   \033[48;2;R;G;Bm for background (bottom pixel)
 * then print ▀ (upper half block).
 */
static void rb__surface_present_term(rb_surface_t *s) {
    /* Build output in a buffer to minimize write() syscalls */
    int cells_w = s->width;
    int cells_h = s->height / 2;

    /* Worst case per cell: ~40 bytes (color escapes + UTF-8 char) */
    /* Plus cursor positioning. Over-allocate to be safe. */
    size_t max_out = (size_t)cells_w * cells_h * 60 + 256;
    char *buf = (char *)malloc(max_out);
    if (!buf) return;
    int pos = 0;

    uint32_t last_fg = 0xFFFFFFFF;
    uint32_t last_bg = 0xFFFFFFFF;

    for (int cy = 0; cy < cells_h; cy++) {
        int py_top = cy * 2;
        int py_bot = cy * 2 + 1;

        /* Check if this row has any changes */
        bool row_dirty = false;
        for (int cx = 0; cx < cells_w; cx++) {
            int it = py_top * s->stride + cx;
            int ib = py_bot * s->stride + cx;
            if (s->back[it] != s->front[it] || s->back[ib] != s->front[ib]) {
                row_dirty = true;
                break;
            }
        }
        if (!row_dirty) continue;

        /* Move cursor to start of this row */
        pos += snprintf(buf + pos, max_out - pos, "\033[%d;1H", cy + 1);

        for (int cx = 0; cx < cells_w; cx++) {
            int it = py_top * s->stride + cx;
            int ib = py_bot * s->stride + cx;

            uint32_t fg = s->back[it]; /* top pixel = foreground */
            uint32_t bg = s->back[ib]; /* bottom pixel = background */

            if (fg == s->front[it] && bg == s->front[ib]) {
                /* Cell unchanged — move cursor right instead of redrawing */
                /* But batch consecutive skips */
                int skip = 1;
                while (cx + skip < cells_w) {
                    int jt = py_top * s->stride + cx + skip;
                    int jb = py_bot * s->stride + cx + skip;
                    if (s->back[jt] != s->front[jt] || s->back[jb] != s->front[jb])
                        break;
                    skip++;
                }
                if (skip > 0) {
                    pos += snprintf(buf + pos, max_out - pos, "\033[%dC", skip);
                    cx += skip - 1;
                }
                continue;
            }

            /* Emit color escapes only if changed from last emitted */
            if (fg != last_fg) {
                pos += snprintf(buf + pos, max_out - pos,
                    "\033[38;2;%d;%d;%dm", RB_R(fg), RB_G(fg), RB_B(fg));
                last_fg = fg;
            }
            if (bg != last_bg) {
                pos += snprintf(buf + pos, max_out - pos,
                    "\033[48;2;%d;%d;%dm", RB_R(bg), RB_G(bg), RB_B(bg));
                last_bg = bg;
            }

            /* Upper half block: ▀ (UTF-8: E2 96 80) */
            buf[pos++] = (char)0xE2;
            buf[pos++] = (char)0x96;
            buf[pos++] = (char)0x80;
        }
    }

    /* Reset colors at end */
    pos += snprintf(buf + pos, max_out - pos, "\033[0m");

    /* Single write syscall for the entire frame */
    write(STDOUT_FILENO, buf, pos);

    /* Swap: copy back → front */
    memcpy(s->front, s->back, s->size);

    free(buf);
}

/* ─── Input (terminal raw mode) ─── */

static void rb__input_init_term(rb_input_t *inp) {
    memset(inp, 0, sizeof(*inp));
    inp->mode = RB_INPUT_TERM;
}

/* Leftover bytes from previous read (for split escape sequences) */
static unsigned char rb__input_buf[256];
static int rb__input_buf_len = 0;

static void rb__input_poll_term(rb_input_t *inp) {
    inp->mouse_dx = 0;
    inp->mouse_dy = 0;

    /* Append new data to leftover buffer */
    int space = (int)sizeof(rb__input_buf) - rb__input_buf_len;
    if (space > 0) {
        int n = (int)read(STDIN_FILENO, rb__input_buf + rb__input_buf_len, (size_t)space);
        if (n > 0) rb__input_buf_len += n;
    }

    int i = 0;
    while (i < rb__input_buf_len) {
        if (rb__input_buf[i] == 0x03) { /* Ctrl+C */
            inp->quit_requested = true;
            i++;
        } else if (rb__input_buf[i] == 0x1B) { /* Escape sequence */
            if (i + 1 >= rb__input_buf_len) break; /* need more data */
            if (rb__input_buf[i+1] == '[') {
                if (i + 2 >= rb__input_buf_len) break; /* need more data */
                /* Check for SGR mouse: ESC[<btn;col;rowM/m */
                if (rb__input_buf[i+2] == '<') {
                    /* Find the terminator M or m */
                    int j = i + 3;
                    while (j < rb__input_buf_len &&
                           rb__input_buf[j] != 'M' && rb__input_buf[j] != 'm')
                        j++;
                    if (j >= rb__input_buf_len) break; /* incomplete, wait for more */

                    /* Parse params: btn;col;row */
                    int params[3] = {0, 0, 0};
                    int pi = 0;
                    for (int k = i + 3; k < j && pi < 3; k++) {
                        if (rb__input_buf[k] == ';') { pi++; }
                        else if (rb__input_buf[k] >= '0' && rb__input_buf[k] <= '9')
                            params[pi] = params[pi] * 10 + (rb__input_buf[k] - '0');
                    }

                    bool press = (rb__input_buf[j] == 'M');
                    int btn_code = params[0];
                    int col = params[1];
                    int row = params[2];

                    int old_mx = inp->mouse_x, old_my = inp->mouse_y;
                    inp->mouse_x = col - 1;
                    inp->mouse_y = (row - 1) * 2; /* half-block doubling */
                    inp->mouse_dx = inp->mouse_x - old_mx;
                    inp->mouse_dy = inp->mouse_y - old_my;

                    int base = btn_code & 0x03;
                    bool is_motion = (btn_code & 32) != 0;
                    bool is_scroll = (btn_code & 64) != 0;

                    if (is_scroll) {
                        /* Scroll — ignore for button state */
                    } else if (is_motion) {
                        /* Motion event: base encodes which button is held
                         * base 0=left, 1=middle, 2=right, 3=none */
                        uint8_t held = 0;
                        if (base == 0) held = RB_MOUSE_LEFT;
                        else if (base == 1) held = RB_MOUSE_MIDDLE;
                        else if (base == 2) held = RB_MOUSE_RIGHT;
                        /* Update: set held button, clear others only if
                         * a single button drag (most common case) */
                        if (base < 3) inp->mouse_btn = held;
                        /* base==3 means no button held during motion */
                        else inp->mouse_btn = 0;
                    } else if (press) {
                        if (base == 0) inp->mouse_btn |= RB_MOUSE_LEFT;
                        else if (base == 1) inp->mouse_btn |= RB_MOUSE_MIDDLE;
                        else if (base == 2) inp->mouse_btn |= RB_MOUSE_RIGHT;
                    } else {
                        if (base == 0) inp->mouse_btn &= ~RB_MOUSE_LEFT;
                        else if (base == 1) inp->mouse_btn &= ~RB_MOUSE_MIDDLE;
                        else if (base == 2) inp->mouse_btn &= ~RB_MOUSE_RIGHT;
                    }

                    i = j + 1;
                } else {
                    int key = 0;
                    switch (rb__input_buf[i+2]) {
                        case 'A': key = RB_KEY_UP;    break;
                        case 'B': key = RB_KEY_DOWN;  break;
                        case 'C': key = RB_KEY_RIGHT; break;
                        case 'D': key = RB_KEY_LEFT;  break;
                    }
                    if (key) {
                        inp->keys[key] = 1;
                        inp->held[key] = RB_HOLD_FRAMES;
                    }
                    i += 3;
                }
            } else {
                inp->keys[RB_KEY_ESC] = 1;
                inp->held[RB_KEY_ESC] = RB_HOLD_FRAMES;
                i++;
            }
        } else {
            /* Regular key (including Ctrl+A..Z as 0x01..0x1A) */
            inp->keys[rb__input_buf[i]] = 1;
            inp->held[rb__input_buf[i]] = RB_HOLD_FRAMES;
            i++;
        }
    }

    /* Keep unprocessed bytes for next call */
    if (i > 0 && i < rb__input_buf_len) {
        memmove(rb__input_buf, rb__input_buf + i, (size_t)(rb__input_buf_len - i));
        rb__input_buf_len -= i;
    } else {
        rb__input_buf_len = 0;
    }
}

/* Clear impulse keys and decay held state */
static void rb__input_clear_frame(rb_input_t *inp) {
    /* Save current impulse keys as previous (for edge detection) */
    memcpy(inp->prev_keys, inp->keys, sizeof(inp->keys));
    /* Clear impulse — they only last one frame */
    memset(inp->keys, 0, sizeof(inp->keys));
    /* Decay held keys */
    for (int i = 0; i < 512; i++)
        if (inp->held[i] > 0) inp->held[i]--;
    /* Save mouse button state for edge detection */
    inp->prev_mouse_btn = inp->mouse_btn;
}

/* ─── Drawing Primitives ─── */

void rb_clear(rb_surface_t *s, uint32_t color) {
    uint32_t *p = s->back;
    int total = s->width * s->height;
    for (int i = 0; i < total; i++)
        p[i] = color;
}

void rb_present(rb_surface_t *s) {
    if (s->mode == RB_FB_TERM) {
        rb__surface_present_term(s);
    }
    /* RB_FB_RAW: future */
}

void rb_line(rb_surface_t *s, int x0, int y0, int x1, int y1, uint32_t c) {
    /* Bresenham's line algorithm — pure integer, no floating point */
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        rb_pixel(s, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void rb_rect(rb_surface_t *s, int x, int y, int w, int h, uint32_t c) {
    rb_line(s, x, y, x + w - 1, y, c);
    rb_line(s, x, y + h - 1, x + w - 1, y + h - 1, c);
    rb_line(s, x, y, x, y + h - 1, c);
    rb_line(s, x + w - 1, y, x + w - 1, y + h - 1, c);
}

void rb_rect_fill(rb_surface_t *s, int x, int y, int w, int h, uint32_t c) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            rb_pixel(s, col, row, c);
}

void rb_circle(rb_surface_t *s, int cx, int cy, int r, uint32_t c) {
    /* Midpoint circle algorithm — pure integer */
    int x = r, y = 0;
    int err = 1 - r;

    while (x >= y) {
        rb_pixel(s, cx + x, cy + y, c);
        rb_pixel(s, cx - x, cy + y, c);
        rb_pixel(s, cx + x, cy - y, c);
        rb_pixel(s, cx - x, cy - y, c);
        rb_pixel(s, cx + y, cy + x, c);
        rb_pixel(s, cx - y, cy + x, c);
        rb_pixel(s, cx + y, cy - x, c);
        rb_pixel(s, cx - y, cy - x, c);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void rb_circle_fill(rb_surface_t *s, int cx, int cy, int r, uint32_t c) {
    for (int y = -r; y <= r; y++) {
        int half_w = (int)sqrtf((float)(r * r - y * y));
        for (int x = -half_w; x <= half_w; x++)
            rb_pixel(s, cx + x, cy + y, c);
    }
}

/* ─── 8×8 Bitmap Font ─── */

/*
 * Minimal embedded font covering ASCII 32-126.
 * Each character is 8 bytes (8 rows of 8 pixels, MSB-left).
 * Total: 95 chars × 8 bytes = 760 bytes. Fits anywhere.
 */
static const uint8_t rb__font[95][8] = {
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

void rb_text(rb_surface_t *s, int x, int y, const char *str, uint32_t c) {
    while (*str) {
        int ch = (unsigned char)*str;
        if (ch >= 32 && ch <= 126) {
            const uint8_t *glyph = rb__font[ch - 32];
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

/* ─── Color Helpers ─── */

uint32_t rb_hue(float h) {
    /* HSV with S=1, V=1. h in [0,1] wraps. */
    h = h - floorf(h);
    float r, g, b;
    float f = h * 6.0f;
    int i = (int)f;
    float frac = f - i;

    switch (i % 6) {
        case 0: r=1;     g=frac;  b=0;      break;
        case 1: r=1-frac;g=1;     b=0;      break;
        case 2: r=0;     g=1;     b=frac;   break;
        case 3: r=0;     g=1-frac;b=1;      break;
        case 4: r=frac;  g=0;     b=1;      break;
        case 5: r=1;     g=0;     b=1-frac; break;
        default:r=1;     g=0;     b=0;      break;
    }
    return RB_RGB((uint8_t)(r*255), (uint8_t)(g*255), (uint8_t)(b*255));
}

uint32_t rb_lerp_color(uint32_t a, uint32_t b, float t) {
    if (t <= 0) return a;
    if (t >= 1) return b;
    uint8_t r = (uint8_t)(RB_R(a) + (RB_R(b) - RB_R(a)) * t);
    uint8_t g = (uint8_t)(RB_G(a) + (RB_G(b) - RB_G(a)) * t);
    uint8_t bl= (uint8_t)(RB_B(a) + (RB_B(b) - RB_B(a)) * t);
    return RB_RGB(r, g, bl);
}

/* ─── Blit & Scroll ─── */

void rb_blit(rb_surface_t *dst, int dx, int dy,
             const uint32_t *src, int sw, int sh, int src_stride) {
    for (int y = 0; y < sh; y++) {
        int dstY = dy + y;
        if ((unsigned)dstY >= (unsigned)dst->height) continue;
        for (int x = 0; x < sw; x++) {
            int dstX = dx + x;
            if ((unsigned)dstX >= (unsigned)dst->width) continue;
            dst->back[dstY * dst->stride + dstX] = src[y * src_stride + x];
        }
    }
}

void rb_blit_alpha(rb_surface_t *dst, int dx, int dy,
                   const uint32_t *src, int sw, int sh, int src_stride) {
    for (int y = 0; y < sh; y++) {
        int dstY = dy + y;
        if ((unsigned)dstY >= (unsigned)dst->height) continue;
        for (int x = 0; x < sw; x++) {
            int dstX = dx + x;
            if ((unsigned)dstX >= (unsigned)dst->width) continue;
            uint32_t sc = src[y * src_stride + x];
            int a = RB_A(sc);
            if (a == 0) continue;
            if (a == 255) {
                dst->back[dstY * dst->stride + dstX] = sc;
                continue;
            }
            uint32_t dc = dst->back[dstY * dst->stride + dstX];
            int inv = 255 - a;
            dst->back[dstY * dst->stride + dstX] = RB_RGB(
                (uint8_t)((RB_R(sc) * a + RB_R(dc) * inv) / 255),
                (uint8_t)((RB_G(sc) * a + RB_G(dc) * inv) / 255),
                (uint8_t)((RB_B(sc) * a + RB_B(dc) * inv) / 255));
        }
    }
}

void rb_scroll(rb_surface_t *s, int dx, int dy, uint32_t fill) {
    uint32_t *buf = s->back;
    int w = s->width, h = s->height, stride = s->stride;

    /* Vertical shift */
    if (dy > 0 && dy < h) {
        for (int y = h - 1; y >= dy; y--)
            memmove(buf + y * stride, buf + (y - dy) * stride, w * sizeof(uint32_t));
        for (int y = 0; y < dy; y++)
            for (int x = 0; x < w; x++) buf[y * stride + x] = fill;
    } else if (dy < 0 && dy > -h) {
        int ady = -dy;
        for (int y = 0; y < h - ady; y++)
            memmove(buf + y * stride, buf + (y + ady) * stride, w * sizeof(uint32_t));
        for (int y = h - ady; y < h; y++)
            for (int x = 0; x < w; x++) buf[y * stride + x] = fill;
    }

    /* Horizontal shift */
    if (dx > 0 && dx < w) {
        for (int y = 0; y < h; y++) {
            memmove(buf + y * stride + dx, buf + y * stride, (w - dx) * sizeof(uint32_t));
            for (int x = 0; x < dx; x++) buf[y * stride + x] = fill;
        }
    } else if (dx < 0 && dx > -w) {
        int adx = -dx;
        for (int y = 0; y < h; y++) {
            memmove(buf + y * stride, buf + y * stride + adx, (w - adx) * sizeof(uint32_t));
            for (int x = w - adx; x < w; x++) buf[y * stride + x] = fill;
        }
    }
}

void rb_rect_inv(rb_surface_t *s, int x, int y, int w, int h) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            if ((unsigned)col < (unsigned)s->width && (unsigned)row < (unsigned)s->height) {
                uint32_t c = s->back[row * s->stride + col];
                s->back[row * s->stride + col] =
                    RB_RGB(255 - RB_R(c), 255 - RB_G(c), 255 - RB_B(c));
            }
}

/* ─── Ring Buffer ─── */

rb_ring_t rb_ring_create(size_t capacity) {
    rb_ring_t r;
    memset(&r, 0, sizeof(r));

    /* Round up to power of 2 */
    size_t sz = 1;
    while (sz < capacity) sz <<= 1;

    /* Page-align for mmap (minimum one page) */
    long page = sysconf(_SC_PAGESIZE);
    if ((long)sz < page) sz = (size_t)page;

    r.capacity = sz;
    atomic_init(&r.write_pos, 0);
    atomic_init(&r.read_pos, 0);

    /* Try mmap double-map trick: map the same fd at two adjacent addresses
     * so that reading/writing across the boundary wraps automatically */
#ifdef __NR_memfd_create
    int fd = (int)syscall(__NR_memfd_create, "rb_ring", 0);
    if (fd >= 0) {
        if (ftruncate(fd, (off_t)sz) == 0) {
            void *addr = mmap(NULL, sz * 2, PROT_NONE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (addr != MAP_FAILED) {
                void *p1 = mmap(addr, sz, PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_FIXED, fd, 0);
                void *p2 = mmap((char *)addr + sz, sz, PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_FIXED, fd, 0);
                if (p1 != MAP_FAILED && p2 != MAP_FAILED) {
                    r.buf = (uint8_t *)addr;
                    r.mmap_backed = true;
                    close(fd);
                    return r;
                }
                munmap(addr, sz * 2);
            }
        }
        close(fd);
    }
#endif

    /* Fallback: plain heap */
    r.buf = (uint8_t *)calloc(1, sz);
    r.mmap_backed = false;
    return r;
}

void rb_ring_destroy(rb_ring_t *r) {
    if (!r->buf) return;
    if (r->mmap_backed)
        munmap(r->buf, r->capacity * 2);
    else
        free(r->buf);
    r->buf = NULL;
}

size_t rb_ring_available(rb_ring_t *r) {
    return atomic_load(&r->write_pos) - atomic_load(&r->read_pos);
}

size_t rb_ring_space(rb_ring_t *r) {
    return r->capacity - rb_ring_available(r);
}

size_t rb_ring_write(rb_ring_t *r, const void *data, size_t len) {
    size_t space = rb_ring_space(r);
    if (len > space) len = space;
    if (len == 0) return 0;

    size_t wpos = atomic_load(&r->write_pos);
    size_t offset = wpos & (r->capacity - 1);

    if (r->mmap_backed) {
        /* Double-map handles wrap-around transparently */
        memcpy(r->buf + offset, data, len);
    } else {
        size_t first = r->capacity - offset;
        if (first > len) first = len;
        memcpy(r->buf + offset, data, first);
        if (len > first)
            memcpy(r->buf, (const uint8_t *)data + first, len - first);
    }

    atomic_store(&r->write_pos, wpos + len);
    return len;
}

size_t rb_ring_read(rb_ring_t *r, void *out, size_t len) {
    size_t avail = rb_ring_available(r);
    if (len > avail) len = avail;
    if (len == 0) return 0;

    size_t rpos = atomic_load(&r->read_pos);
    size_t offset = rpos & (r->capacity - 1);

    if (r->mmap_backed) {
        memcpy(out, r->buf + offset, len);
    } else {
        size_t first = r->capacity - offset;
        if (first > len) first = len;
        memcpy(out, r->buf + offset, first);
        if (len > first)
            memcpy((uint8_t *)out + first, r->buf, len - first);
    }

    atomic_store(&r->read_pos, rpos + len);
    return len;
}

/* ─── Audio (aplay pipe + synthesis thread) ─── */

/*
 * Audio output via pipe to aplay. The pipe buffer IS the ring buffer —
 * write() blocks when full, which IS backpressure. No ALSA ioctl needed.
 *
 * A pthread runs the user's synthesis callback, converts float→S16LE,
 * and writes to the pipe. The callback fills interleaved stereo floats
 * in [-1, 1] range.
 */

struct rb_audio {
    int           pipe_fd;     /* write end of pipe to aplay */
    pid_t         child_pid;   /* aplay process */
    pthread_t     thread;      /* synthesis thread */
    rb_audio_fn   callback;
    void         *userdata;
    rb_ring_t    *source_ring; /* if set, read S16LE from ring instead of callback */
    volatile bool running;
    int           sample_rate;
    int           channels;
    int           buf_frames;  /* frames per callback invocation */
};

static void *rb__audio_thread(void *arg) {
    struct rb_audio *a = (struct rb_audio *)arg;
    int nsamples = a->buf_frames * a->channels;
    size_t chunk_bytes = (size_t)nsamples * sizeof(int16_t);

    if (a->source_ring) {
        /* Ring buffer mode: read S16LE directly from ring → aplay pipe */
        int16_t *ibuf = (int16_t *)malloc(chunk_bytes);
        while (a->running) {
            size_t got = rb_ring_read(a->source_ring, ibuf, chunk_bytes);
            if (got == 0) { usleep(100); continue; }
            size_t written = 0;
            while (written < got && a->running) {
                ssize_t n = write(a->pipe_fd, (uint8_t *)ibuf + written, got - written);
                if (n <= 0) { a->running = false; break; }
                written += (size_t)n;
            }
        }
        free(ibuf);
    } else {
        /* Callback mode: synthesize float → convert to S16LE → aplay pipe */
        float *fbuf = (float *)malloc((size_t)nsamples * sizeof(float));
        int16_t *ibuf = (int16_t *)malloc(chunk_bytes);

        while (a->running) {
            memset(fbuf, 0, (size_t)nsamples * sizeof(float));
            a->callback(fbuf, a->buf_frames, a->channels, a->userdata);

            /* Float [-1,1] → S16LE */
            for (int i = 0; i < nsamples; i++) {
                float s = fbuf[i];
                if (s > 1.0f) s = 1.0f;
                if (s < -1.0f) s = -1.0f;
                ibuf[i] = (int16_t)(s * 32767.0f);
            }

            /* write() blocks when pipe buffer is full = natural backpressure */
            size_t written = 0;
            while (written < chunk_bytes && a->running) {
                ssize_t n = write(a->pipe_fd, (uint8_t *)ibuf + written, chunk_bytes - written);
                if (n <= 0) { a->running = false; break; }
                written += (size_t)n;
            }
        }

        free(fbuf);
        free(ibuf);
    }
    return NULL;
}

void rb_audio_start(rb_ctx_t *ctx, rb_audio_fn callback, void *userdata) {
    struct rb_audio *a = (struct rb_audio *)calloc(1, sizeof(*a));
    a->sample_rate = 48000;
    a->channels = 2;
    a->buf_frames = 1024;
    a->callback = callback;
    a->userdata = userdata;
    a->running = true;

    int pipefd[2];
    if (pipe(pipefd) != 0) { free(a); return; }

    pid_t pid = fork();
    if (pid == 0) {
        /* Child: become aplay, reading from stdin */
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        /* Redirect stderr to /dev/null to keep terminal clean */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execlp("aplay", "aplay",
               "-f", "S16_LE", "-r", "48000", "-c", "2",
               "-t", "raw", "-q", "-", NULL);
        _exit(1);
    }

    close(pipefd[0]);
    a->pipe_fd = pipefd[1];
    a->child_pid = pid;

    pthread_create(&a->thread, NULL, rb__audio_thread, a);
    ctx->audio = a;
}

void rb_audio_stop(rb_ctx_t *ctx) {
    struct rb_audio *a = ctx->audio;
    if (!a) return;

    a->running = false;
    pthread_join(a->thread, NULL);
    close(a->pipe_fd);
    waitpid(a->child_pid, NULL, 0);
    free(a);
    ctx->audio = NULL;
}

/* ─── Audio Ring Playback ─── */

void rb_audio_start_ring(rb_ctx_t *ctx, rb_ring_t *source) {
    struct rb_audio *a = (struct rb_audio *)calloc(1, sizeof(*a));
    a->sample_rate = 48000;
    a->channels = 2;
    a->buf_frames = 1024;
    a->callback = NULL;
    a->source_ring = source;
    a->running = true;

    int pipefd[2];
    if (pipe(pipefd) != 0) { free(a); return; }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execlp("aplay", "aplay",
               "-f", "S16_LE", "-r", "48000", "-c", "2",
               "-t", "raw", "-q", "-", NULL);
        _exit(1);
    }

    close(pipefd[0]);
    a->pipe_fd = pipefd[1];
    a->child_pid = pid;

    pthread_create(&a->thread, NULL, rb__audio_thread, a);
    ctx->audio = a;
}

/* ─── Pipe Spawn (process stdout → ring buffer) ─── */

static void *rb__pipe_reader_thread(void *arg) {
    rb_pipe_t *p = (rb_pipe_t *)arg;
    unsigned char tmp[4096];
    while (p->running) {
        ssize_t n = read(p->pipe_fd, tmp, sizeof(tmp));
        if (n <= 0) { p->running = false; break; }
        size_t off = 0;
        while (off < (size_t)n) {
            size_t wrote = rb_ring_write(&p->ring, tmp + off, (size_t)n - off);
            if (wrote == 0) { usleep(100); continue; }
            off += wrote;
        }
    }
    return NULL;
}

rb_pipe_t *rb_pipe_spawn(const char *cmd, size_t ring_capacity) {
    rb_pipe_t *p = (rb_pipe_t *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->ring = rb_ring_create(ring_capacity);

    int pipefd[2];
    if (pipe(pipefd) != 0) { free(p); return NULL; }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(1);
    }

    close(pipefd[1]);
    p->pipe_fd = pipefd[0];
    p->child_pid = pid;
    p->running = true;

    pthread_create(&p->thread, NULL, rb__pipe_reader_thread, p);
    return p;
}

void rb_pipe_stop(rb_pipe_t *p) {
    if (!p || !p->running) return;
    p->running = false;
    kill(p->child_pid, SIGTERM);
    pthread_join(p->thread, NULL);
    close(p->pipe_fd);
    waitpid(p->child_pid, NULL, 0);
    rb_ring_destroy(&p->ring);
    free(p);
}

/* ─── FFT (radix-2 Cooley-Tukey) ─── */

void rb_fft(float *re, float *im, int n) {
    /* Bit-reversal permutation */
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float tr = re[i]; re[i] = re[j]; re[j] = tr;
            float ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
    }
    /* Butterfly passes */
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -6.2831853f / len;
        float wre = cosf(ang), wim = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cur_re = 1.0f, cur_im = 0.0f;
            for (int j = 0; j < len / 2; j++) {
                float tre = re[i+j+len/2]*cur_re - im[i+j+len/2]*cur_im;
                float tim = re[i+j+len/2]*cur_im + im[i+j+len/2]*cur_re;
                re[i+j+len/2] = re[i+j] - tre;
                im[i+j+len/2] = im[i+j] - tim;
                re[i+j] += tre;
                im[i+j] += tim;
                float new_re = cur_re*wre - cur_im*wim;
                cur_im = cur_re*wim + cur_im*wre;
                cur_re = new_re;
            }
        }
    }
}

/* ─── Context Init / Destroy ─── */

rb_ctx_t rb_init(int width, int height, int flags) {
    rb_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    rb__init_time();

    /* If no backend specified, default to terminal */
    if (!(flags & (RB_FB_TERM | RB_FB_RAW)))
        flags |= RB_FB_TERM;

    /* Auto-detect terminal size if width/height are 0 */
    if ((flags & RB_FB_TERM) && (width <= 0 || height <= 0)) {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            width  = ws.ws_col;
            height = ws.ws_row * 2; /* ×2 for half-block doubling */
        } else {
            width  = 80;
            height = 48;
        }
    }

    if (flags & RB_FB_TERM) {
        rb__surface_init_term(&ctx.surface, width, height);
        rb__term_raw_enter();
    }

    rb__input_init_term(&ctx.input);

    ctx.running = true;
    ctx.target_fps = 60;
    ctx.time = rb_time();

    /* Install signal handlers */
    signal(SIGINT, rb__sigint_handler);
    signal(SIGWINCH, rb__sigwinch_handler);

    return ctx;
}

void rb_destroy(rb_ctx_t *ctx) {
    /* Stop audio if running */
    if (ctx->audio)
        rb_audio_stop(ctx);

    if (ctx->surface.mode == RB_FB_TERM)
        rb__term_raw_exit();
    rb__surface_destroy(&ctx->surface);
    signal(SIGINT, SIG_DFL);
    signal(SIGWINCH, SIG_DFL);
}

/* ─── Main Loop ─── */

void rb_run(rb_ctx_t *ctx, rb_callbacks_t cb) {
    double last_time = rb_time();
    double fps_timer = last_time;
    int fps_frames = 0;

    double frame_dt = 1.0 / ctx->target_fps;

    while (ctx->running) {
        double now = rb_time();
        ctx->dt = now - last_time;
        ctx->time = now;
        last_time = now;

        /* Input */
        rb__input_poll_term(&ctx->input);

        if (rb__got_sigint || ctx->input.quit_requested) {
            ctx->running = false;
            break;
        }

        /* Quit on ESC or 'q' */
        if (ctx->input.keys[RB_KEY_ESC] || ctx->input.keys['q']) {
            ctx->running = false;
            break;
        }

        /* Fixed timestep tick (if provided) */
        if (cb.tick) {
            cb.tick(ctx, (float)frame_dt);
        }

        /* Draw */
        if (cb.draw) {
            cb.draw(ctx);
        }

        /* Present (swap buffers) */
        rb_present(&ctx->surface);

        /* Clear impulse keys after frame is complete */
        rb__input_clear_frame(&ctx->input);

        /* FPS measurement */
        fps_frames++;
        if (now - fps_timer >= 1.0) {
            ctx->fps = fps_frames;
            fps_frames = 0;
            fps_timer = now;
        }

        ctx->frame++;

        /* Frame pacing */
        double elapsed = rb_time() - now;
        double sleep_s = frame_dt - elapsed;
        if (sleep_s > 0)
            rb__sleep_us((int64_t)(sleep_s * 1e6));
    }
}

#endif /* RAWBUF_IMPLEMENTATION */
