# Pixels Are Just Buffers You Can Hear

*The same buffer math powers screens and speakers. Fire propagation IS echo. Double-buffer swap IS the FFT butterfly. A palette LUT IS a waveshaper.*

---

## A Buffer Is a Buffer Is a Buffer

Look at these two declarations:

```c
uint32_t screen[320 * 200];    // 64,000 pixels at 60 Hz
float    waveform[48000];      // 48,000 samples at 48 kHz
```

Same primitive. Same layout. One contiguous array of numbers, indexed by position. The only difference is what you plug the output into. Route `screen` to a display and you see color. Route `waveform` to a DAC and you hear sound. The buffer doesn't know what it is — it's just numbers in memory, waiting for a clock to give them meaning.

rawbuf.h treats both the same way. The surface is a `uint32_t` array you write pixels into. The audio callback fills a `float` array you write samples into. The ring buffer shuttles bytes between them. Everything is a buffer, and the operations you perform on buffers — read neighbors, average, apply a lookup table, swap two pointers — are the same whether you're rendering fire or synthesizing sound.

---

## Fire Propagation IS Echo

Here's the fire effect from rawbuf's demos, stripped to its core:

```c
// FIRE: read 4 neighbors below, average, decay
for (int y = 0; y < H - 1; y++) {
    for (int x = 0; x < W; x++) {
        int avg = (
            src[(y+1)*W + (x-1)] +
            src[(y+1)*W + x] +
            src[(y+1)*W + (x+1)] +
            src[(y+2)*W + x]
        ) >> 2;
        int decay = rand() % cooling;
        dst[y * W + x] = MAX(0, avg - decay);
    }
}
```

Now here's a simple audio echo:

```c
// ECHO: read delayed sample, blend, decay
for (int i = 0; i < n; i++) {
    float delayed = delay_buf[delay_pos];
    out[i] = in[i] + delayed * feedback;
    delay_buf[delay_pos] = out[i];
    delay_pos = (delay_pos + 1) % delay_len;
}
```

The shape is identical:

| Operation | Fire | Echo |
|-----------|------|------|
| **Read neighbors** | 4 pixels below (spatial) | 1 sample behind (temporal) |
| **Average/blend** | `>> 2` (mean of 4) | `in + delayed * feedback` |
| **Apply decay** | `- rand() % cooling` | `* feedback` (< 1.0) |
| **Write result** | `dst[y*W + x]` | `out[i]` |

Fire reads spatial neighbors and propagates upward. Echo reads temporal neighbors and propagates forward. Both are convolution kernels running over a flat buffer. The fire "reverberates" heat across a 2D grid the same way echo reverberates sound across a 1D timeline. Change the kernel shape and decay rate, and the same algorithm produces either effect.

---

## Double-Buffer Swap IS the FFT

rawbuf's `rb_present()` swaps front and back buffers — you draw to `back`, then `front` and `back` trade places:

```c
// rawbuf: swap display buffers
memcpy(front, back, size);
// (conceptually: tmp = front; front = back; back = tmp)
```

The fire effect does the same thing:

```c
// fire: swap propagation buffers
uint8_t *tmp = fire_a;
fire_a = fire_b;
fire_b = tmp;
```

The FFT butterfly does the same thing at a deeper level. It takes time-domain data and swaps it into frequency-domain data. The "two buffers" are two representations of the same signal — one indexed by time, one indexed by frequency — with a mathematical transform between them.

All three follow the pattern: **two buffers, a transform, a pointer swap**. The display transform is rendering. The fire transform is propagation. The FFT transform is the Fourier kernel. But the mechanism — maintain two copies, apply a function from one to the other, swap — is universal.

This is why double-buffering exists everywhere. It's not a graphics trick. It's the fundamental pattern for any system that needs to read a stable state while computing the next one. Audio synthesis does it (fill buffer N while buffer N-1 plays). Physics simulations do it (compute next state from current state). The FFT does it (in-place butterflies swap between even and odd indexed sub-arrays).

---

## Palette LUT IS a Waveshaper

The fire demo maps heat values to colors through a lookup table:

```c
// Fire: heat value → color
uint32_t color = palette[fire_value];
```

A distortion effect maps audio amplitudes through a transfer curve:

```c
// Distortion: amplitude → shaped amplitude
float output = curve[(int)(input * 127 + 128)];
```

Both are the same operation: `output = LUT[input]`. No math, no computation — just a table lookup. The palette maps 0-255 heat into black→red→orange→yellow→white. The waveshaper maps -1.0..1.0 amplitude into a nonlinear transfer curve that adds harmonics.

In the playground's Audio demo, the distortion visualizer literally draws the transfer curve as a 2D plot — it's the same shape as a palette gradient rotated 45 degrees. Turn up the "Dist" slider and watch the curve compress. That compression is exactly what happens when a fire pixel hits the top of the palette and saturates to white.

The insight: any monotonic function applied element-wise to a buffer is a "palette." Temperature to color, amplitude to distortion, velocity to particle size, health to UI color — they're all the same `output = table[input]` lookup.

---

## Ring Buffer: The Universal Connector

rawbuf.h includes `rb_ring_t` — a lock-free SPSC ring buffer with the mmap double-map trick for zero-copy wrap-around:

```c
// Ring buffer: same memory mapped twice, adjacent
// Read/write across the boundary "wraps" transparently
void *p1 = mmap(addr,     size, ..., fd, 0);
void *p2 = mmap(addr+size, size, ..., fd, 0);
// Now buf[size + 5] == buf[5], no modulo needed
```

In audio, the ring buffer IS the delay line. The echo effect stores past samples in a circular buffer and reads them back at an offset. That offset is the delay time. The buffer wraps around, and old samples are overwritten by new ones — natural memory recycling.

In rawbuf's own audio output, the pipe to `aplay` is a kernel-managed ring buffer. `write()` blocks when the pipe buffer is full — that IS backpressure. The kernel's pipe buffer and rawbuf's `rb_ring_t` are the same data structure at different privilege levels.

In GPU rendering, the command ring buffer feeds draw calls from CPU to GPU. The CPU writes commands, the GPU reads them, and the head/tail pointers chase each other around the ring.

The ring buffer appears everywhere because it solves the universal producer-consumer problem: one thing generates data at its own rate, another thing consumes it at a different rate, and the ring absorbs the difference. Audio synthesis produces samples at 48 kHz. The display consumes frames at 60 Hz. The ring buffer sits between them, and the `audio_visual.c` demo uses exactly this pattern — the audio thread writes FFT magnitudes into `rb_ring_t`, the render thread reads them out and modulates the plasma.

---

## Try It: Audio-Reactive Plasma

Open the [playground](../playground.html) and go to the **Audio** tab. Play some notes (A through K keys). Now switch to the **Plasma** tab without stopping — the audio context stays alive and the plasma starts pulsing:

- **Bass** (low frequency bins) boosts wave amplitude — the plasma undulates harder
- **Mid** frequencies add a radial pulse wave emanating from the center
- **Treble** shifts brightness — high harmonics make the whole field glow

A green **FFT LINKED** badge appears in the plasma controls when audio is feeding the visualization. This is the thesis made interactive: the same smoothed FFT data that draws the frequency bars in the Audio tab is now driving the visual output of a completely different demo. One ring buffer, two outputs.

The full C implementation is in `demos/audio_visual.c` — it generates a tone with `rb_audio_start()`, computes a 16-bin DFT in the audio callback, writes the magnitudes through `rb_ring_t`, and the render thread reads them to modulate a plasma shader. Same buffer. Same math. Different clock.
