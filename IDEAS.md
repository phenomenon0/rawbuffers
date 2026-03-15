# RawBuf Video Effects Library — Pick Your Poison

## Context

rawbuf (1420 lines) already has: surface double-buffer, drawing primitives, pipe spawn (any process → ring buffer), audio playback, FFT, mouse input, collision helpers. The pattern for ALL of these is:

```
ffmpeg (or webcam/screen/stream) → rb_pipe_spawn → ring buffer → your code manipulates pixels → rb_present
```

With "a little ML" = pipe frames to a Python/ONNX/tflite subprocess, read results back. With "a lot of imagination" = combine these building blocks. Here's the menu.

---

## A. PURE PIXEL EFFECTS (no ML, just math on raw pixels)

### A1. Chroma Key (Green Screen)
Replace any pixel in a color range with another video or image. Two pipes: foreground video + background video. Per-pixel: if `G > 150 && R < 100 && B < 100` → take background pixel instead.
- **Complexity:** ~30 lines
- **Uses:** rb_pipe_spawn × 2, rb_blit, per-pixel color test

### A2. Glitch / Datamosh
Corrupt frame data deliberately — shift rows randomly, swap RGB channels, duplicate scanlines, inject noise blocks. Time-varying intensity.
- **Modes:** scanline shift, RGB channel swap, block duplication, static noise injection, pixel sort
- **Complexity:** ~50 lines
- **Uses:** direct pixel buffer manipulation, rand()

### A3. VHS / Retro CRT
Combine: scanline darkening (every other row dimmed), chromatic aberration (shift R channel left, B channel right), tracking noise (random horizontal offset bands), color bleeding, reduced color palette.
- **Complexity:** ~60 lines
- **Uses:** per-pixel color math, rb_scroll for tracking jitter

### A4. Thermal / Infrared Camera
Convert to grayscale luminance, then map through a heat colormap (black → blue → red → yellow → white). High contrast areas glow.
- **Complexity:** ~20 lines
- **Uses:** rb_lerp_color for gradient mapping

### A5. Night Vision
Boost brightness, tint everything green, add scanline overlay and random noise speckle. Vignette (darken edges).
- **Complexity:** ~25 lines
- **Uses:** per-pixel color transform, rb_circle_fill for vignette mask

### A6. Edge Detection (Sobel Filter)
3×3 convolution kernel on luminance → edge magnitude → draw white-on-black outlines of everything in the video. Neon variant: colorize edges by angle.
- **Complexity:** ~35 lines
- **Uses:** rb_peek to read neighbors, rb_pixel to write result

### A7. Pixelate / Mosaic
Divide frame into NxN blocks, average each block's color, fill block with that color. Mouse controls block size.
- **Complexity:** ~20 lines
- **Uses:** rb_peek, rb_rect_fill

### A8. Mirror / Kaleidoscope
Mirror left↔right, top↔bottom, or 4-way/6-way kaleidoscope by copying sectors with coordinate transforms.
- **Complexity:** ~25 lines
- **Uses:** rb_peek + rb_pixel with mirrored coordinates

### A9. Color Channel Isolation / Split
Show only R, G, or B channel. Or split screen into 3 panels showing each channel. Or offset channels for trippy chromatic aberration.
- **Complexity:** ~15 lines
- **Uses:** RB_R/G/B macros, rb_pixel

### A10. Motion Trail / Echo
Keep previous N frames in a circular buffer, blend them with decreasing opacity. Moving objects leave ghostly trails.
- **Complexity:** ~30 lines (need frame history array)
- **Uses:** rb_lerp_color for blending, circular frame buffer

### A11. ASCII Art Video
Map each block's luminance to a character from ` .:-=+*#%@`. Render with rb_text over a black background. Colored ASCII = use the block's average color.
- **Complexity:** ~25 lines
- **Uses:** rb_peek for luminance, rb_text for characters

### A12. Old Film / Silent Movie
Desaturate to sepia (weighted grayscale → tint), add random dust/scratch particles (white dots and vertical lines), intermittent brightness flicker, frame vignette.
- **Complexity:** ~40 lines
- **Uses:** per-pixel color math, rand() for particles

### A13. Bloom / Glow
Find bright pixels (luminance > threshold), blur them outward (simple box blur), add back to original. Makes lights bleed realistically.
- **Complexity:** ~40 lines
- **Uses:** two-pass pixel scan, rb_lerp_color for additive blend

### A14. Picture-in-Picture
Two video pipes rendered at different positions/sizes. Main video full-screen, secondary in a corner with border. Click to swap.
- **Complexity:** ~20 lines
- **Uses:** rb_pipe_spawn × 2, rb_blit, rb_rect for border, rb_mouse_pressed

### A15. Time Freeze / Bullet Time
Capture a frame, hold it frozen while audio continues. Resume on keypress. Variant: slow-motion by reading ring at half speed.
- **Complexity:** ~15 lines
- **Uses:** stop reading from ring, keep last frame

---

## B. AUDIO-REACTIVE VIDEO (FFT-driven visuals on video)

### B1. Audio-Reactive Color Grading
FFT bass energy controls overall tint (more bass = more red), mids control saturation, highs control brightness. Video pulses with the music.
- **Complexity:** ~30 lines
- **Uses:** rb_fft on audio ring, per-pixel color shift

### B2. Beat-Synced Glitch
Detect beats (sudden FFT energy spike), trigger glitch effects (A2) on beat hits. Intensity proportional to beat strength.
- **Complexity:** ~40 lines
- **Uses:** rb_fft for beat detection, glitch effect from A2

### B3. Waveform Overlay on Video
Draw the audio waveform as a translucent oscilloscope line over the video. Or embed FFT bars along the bottom edge.
- **Complexity:** ~25 lines
- **Uses:** rb_line for waveform, rb_fft for spectrum, rb_blit for video base

### B4. Audio-Reactive Zoom / Pulse
Scale the video from center based on bass energy — frame zooms in/out with the beat. Just sample from offset coordinates.
- **Complexity:** ~25 lines
- **Uses:** coordinate remapping per pixel based on FFT magnitude

### B5. Frequency-Band Edge Glow
Run Sobel (A6) on video, but colorize edges by which frequency band is loudest. Bass = red edges, mids = green, highs = blue. Video literally glows to the music.
- **Complexity:** ~45 lines
- **Uses:** Sobel from A6, rb_fft, rb_hue for frequency-to-color

---

## C. ML-POWERED EFFECTS (pipe frames to external model)

The ML pattern:
```
video frame → write to pipe/socket → Python/ONNX process → read result → overlay on frame
```
Rawbuf doesn't need ML code — it just pipes raw pixels to a subprocess and reads back annotations/masks/transformed frames.

### C1. Background Removal / Virtual Background
ML model (MediaPipe/U²-Net) outputs a binary mask per frame. rawbuf reads mask, replaces background pixels with another video/image/color.
- **ML model:** mediapipe selfie segmentation, rembg, or U²-Net
- **Pipeline:** frame → Python stdin → model → mask stdout → rawbuf composites
- **Complexity:** ~40 lines C + ~20 lines Python wrapper

### C2. Object Detection Bounding Boxes
YOLO/MobileNet-SSD detects objects, outputs `[class, x, y, w, h, confidence]` per detection. rawbuf draws labeled rectangles over video.
- **ML model:** YOLOv8-nano, MobileNet-SSD (ONNX)
- **Pipeline:** frame → model → JSON/CSV coordinates → rawbuf draws rb_rect + rb_text labels
- **Complexity:** ~35 lines C + model script

### C3. Pose Estimation Skeleton
MediaPipe/MoveNet outputs 17+ keypoints (nose, shoulders, elbows, wrists, hips, knees, ankles). rawbuf draws skeleton lines and joint circles over video.
- **ML model:** MediaPipe Pose, MoveNet
- **Pipeline:** frame → model → keypoint coordinates → rb_line + rb_circle_fill
- **Complexity:** ~45 lines C

### C4. Face Detection + Filters
Detect face bounding box + landmarks (eyes, nose, mouth). Draw filters: sunglasses sprite on eyes, mustache on mouth, pixelate face region, blur face for privacy.
- **ML model:** MediaPipe Face Mesh, dlib, or BlazeFace
- **Uses:** rb_sprite_draw for overlays, rb_rect_fill for pixelation (A7 on face region only)

### C5. Style Transfer
Feed frame through a neural style transfer model (fast style transfer). Output is the stylized frame — Van Gogh video, comic book, watercolor, etc.
- **ML model:** fast-neural-style (PyTorch), arbitrary style transfer
- **Pipeline:** raw frame → model → stylized frame → rb_blit directly
- **Note:** Heavier, maybe 5-10 FPS depending on model/GPU

### C6. Depth Estimation + Fog Effect
Monocular depth model (MiDaS/DPT) outputs depth map. Use depth to add fog: far pixels fade to gray/white, near pixels stay sharp. Or parallax scrolling.
- **ML model:** MiDaS, Depth Anything
- **Pipeline:** frame → model → depth map → rb_lerp_color(pixel, fog_color, depth)

### C7. Motion Detection / Security Camera
Frame differencing (current - previous) → threshold → highlight regions with movement. ML variant: only alert on person/vehicle detection (C2).
- **Simple version:** pure pixel diff, ~20 lines, no ML needed
- **Smart version:** add YOLO (C2) to classify what moved

### C8. Real-Time Colorization
Feed B&W video through a colorization model. Output is colorized frame. Works on old footage, security cameras, etc.
- **ML model:** DeOldify, InstColorization
- **Pipeline:** grayscale frame → model → colorized frame → rb_blit

### C9. Super Resolution Upscale
Run a small super-resolution model to upscale low-res source. Pipe low-res frames to model, get high-res back, downsample to terminal resolution but with sharper detail.
- **ML model:** Real-ESRGAN, FSRCNN
- **Use case:** Enhance old/low-quality video

### C10. Hand Gesture Recognition
MediaPipe Hands detects hand landmarks. Map gestures to controls: open palm = pause, fist = rewind, swipe = next track. Gesture-controlled video player.
- **ML model:** MediaPipe Hands
- **Pipeline:** frame → model → gesture class → rawbuf input event

---

## D. CREATIVE TOOLS & APPLICATIONS

### D1. Live VJ Tool
Load multiple video sources + audio input. Mouse-switchable effects (A1-A15). FFT drives effect intensity. Keyboard maps to effect presets. Performance tool for live shows.
- **Combines:** everything — pipes, FFT, mouse, effects chain

### D2. Video Annotator / Telestrator
Play video, pause, draw on frozen frame with mouse (freehand lines, rectangles, circles, text). Resume playback with annotations persisted as overlay.
- **Uses:** rb_mouse_down for drawing, frame capture for pause, overlay buffer

### D3. Rotoscope / Frame Painter
Step through video frame-by-frame (arrow keys). Paint masks or colors on each frame with mouse. Export painted frames. Manual animation on top of video.
- **Uses:** frame-by-frame pipe control (stop/start ffmpeg), mouse painting

### D4. Video Diff Tool
Two video sources side-by-side or overlaid. Highlight pixel differences in red. Useful for comparing encodes, finding changes between takes.
- **Uses:** rb_pipe_spawn × 2, per-pixel diff, rb_rect_inv or color highlight

### D5. Multi-Camera Monitor
4 (or more) video sources tiled on screen. Click to expand one to fullscreen. Each from a different file, webcam, or network stream.
- **Uses:** rb_pipe_spawn × N, rb_blit at tiled positions, rb_mouse_pressed for selection

### D6. Terminal Cinema
Full-featured video player: video + audio sync, seek bar, volume control, pause/play, playlist. The `player.c` demo extended with video frames.
- **Uses:** two pipes (video + audio), rb_audio_start_ring, mouse seek bar

### D7. Live Subtitle Overlay
Pipe audio to Whisper (speech-to-text), render transcribed text at bottom of video in real-time. Auto-captioning.
- **ML model:** whisper.cpp or faster-whisper
- **Uses:** audio ring → whisper pipe → text back → rb_text overlay

---

## E. MULTI-SOURCE COMPOSITING

### E1. Webcam + Screen Capture
Webcam in corner over screen capture. Streamer/presenter layout. Mouse to reposition webcam overlay.
- **Pipes:** `ffmpeg -f v4l2 -i /dev/video0 ...` + `ffmpeg -f x11grab -i :0 ...`

### E2. Video + Live Drawing + Audio Viz
Three layers: base video, freehand mouse drawing on top, FFT bars along bottom edge. All real-time.

### E3. Chroma Key + Depth
Green screen removal (A1) combined with depth estimation (C6) for proper layering — foreground person stays in front of composited background objects.

---

## What to pick?

| If you want... | Start with |
|----------------|-----------|
| Quick win, looks cool | A2 (glitch), A4 (thermal), A3 (VHS) |
| Music-driven visuals | B1 (color grade), B2 (beat glitch), B5 (freq glow) |
| Practical tool | D2 (annotator), D6 (cinema), D5 (multi-cam) |
| ML flex | C2 (object detect), C3 (pose skeleton), C1 (bg removal) |
| Everything at once | D1 (VJ tool) — it's the boss level |

## Implementation approach

Each effect is a **standalone demo file** (~50-100 lines) in `demos/`. They all follow the same skeleton:

```c
#define RAWBUF_IMPLEMENTATION
#include "../rawbuf.h"

static rb_pipe_t *vid;
static uint32_t frame[W * H];

void draw(rb_ctx_t *ctx) {
    // 1. Read frame from ring
    if (rb_ring_available(&vid->ring) >= sizeof(frame))
        rb_ring_read(&vid->ring, frame, sizeof(frame));
    // 2. Blit base frame
    rb_blit(&ctx->surface, 0, 0, frame, W, H, W);
    // 3. Apply effect (manipulate pixels)
    // 4. Draw UI overlay
}
```

Pick items from the menu → I'll implement them as demos.
