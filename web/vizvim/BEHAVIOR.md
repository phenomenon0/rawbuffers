# BEHAVIOR.md — Vizvim

> Interaction behavior specification. Pairs with DESIGN.md for complete frontend experiences.
> Archetype: **Physical**

---

## 1. Interaction Philosophy

Vizvim feels like a physical recording device — a camera with weight and precision.
Controls have inertia: buttons depress with spring resistance, the PiP window has
momentum when dragged, and screen transitions slide with the convincing physicality
of a well-machined mechanism. Nothing teleports — elements arrive from somewhere and
depart to somewhere. Recording is the central act, and the UI stays out of its way:
during capture, chrome retreats to a floating control bar that feels like holding a
remote. Quick interactions snap instantly (20ms), while structural transitions unfold
with cinematic deliberation (280-400ms). The signature is controlled weight — every
element moves as if it has mass, but never sluggishly.

---

## 2. Timing & Easing System

| Role | Duration | Easing | Use cases |
|------|----------|--------|-----------|
| Micro | 20ms | `cubic-bezier(0.4, 0, 0.6, 1)` | Hover snap, focus color, icon fill |
| Standard | 280ms | `cubic-bezier(0.4, 0, 0.6, 1)` or `spring(0.55, 0.825)` | Mode card select, dropdown, toggle, device picker |
| Emphasis | 400ms | `spring(0.55, 0.825)` | Screen transition, countdown numerals, modal entrance |
| Exit | 220ms | `cubic-bezier(0.4, 0, 0.6, 1)` | Dismiss, close, fade out |
| Stagger | 40ms/item | `calc(0.2s + itemIndex * 40ms)`, capped at 400ms total | Mode cards, device list, settings items |

### Brand-Specific Easing Curves

| Name | CSS | Spring equivalent | Use |
|------|-----|-------------------|-----|
| Vizvim Standard | `cubic-bezier(0.4, 0, 0.6, 1)` | `spring(response: 0.55, damping: 0.825)` | Nearly every transition |
| Vizvim Soft | `cubic-bezier(0.25, 0.1, 0.3, 1)` | `spring(response: 0.55, damping: 0.9)` | Subtle shifts, background changes |
| Vizvim Snap | `cubic-bezier(0.4, 0.01, 0.165, 0.99)` | — | Quick state changes, hover feedback |
| Vizvim Decel | `cubic-bezier(0.11, 0.86, 0.15, 1)` | — | PiP drag settle, momentum finish |
| Interactive Spring | — | `spring(response: 0.15, damping: 0.86)` | Button press, PiP drag follow, gesture tracking |

### CSS Implementation

```css
:root {
  --v-ease: cubic-bezier(0.4, 0, 0.6, 1);
  --v-ease-soft: cubic-bezier(0.25, 0.1, 0.3, 1);
  --v-ease-snap: cubic-bezier(0.4, 0.01, 0.165, 0.99);
  --v-ease-decel: cubic-bezier(0.11, 0.86, 0.15, 1);
}
```

Spring animations in vanilla JS via `Web Animations API`:
```javascript
element.animate(keyframes, {
  duration: 400,
  easing: 'cubic-bezier(0.4, 0, 0.6, 1)',
  fill: 'forwards'
});
```

---

## 3. State Machine Patterns

### Record Button (the hero control)
```
idle
  → hover: glow intensifies (box-shadow scale 1→1.15), 20ms snap
  → active/pressed: scale(0.92), spring-interactive (0.15s response)
  → countdown: button morphs to countdown display, 400ms spring
  → recording: pulsing glow animation (1.5s ease-in-out infinite), icon → square (stop)
  → paused: glow freezes, icon → play triangle, muted opacity
  → stopped: spring-out to preview screen (400ms emphasis)
```

### Mode Card (source selection)
```
idle
  → hover: border brightens to --v-border-hover, subtle lift (translateY -2px), 20ms→280ms
  → active/pressed: scale(0.97), spring-interactive
  → selected: border → --v-accent (2px), bg → --v-accent-subtle, icon tints accent, 280ms spring
  → disabled: opacity 0.4, no pointer events (e.g., screen modes on mobile)
```

### Secondary Button / CTA
```
idle
  → hover: bg shift to --v-bg-4, 20ms snap
  → active/pressed: scale(0.97), spring-interactive (0.15s)
  → loading: spinner replaces label (opacity crossfade 200ms), disable pointer
  → success: checkmark icon draws via SVG path, spring(0.55, 0.825), revert 2s
  → error: horizontal shake (3 cycles, 0.4s), revert 3s
```

### Device Picker (camera/mic select)
```
closed → opening: dropdown slides down, 280ms spring, stagger items at 40ms each
  → open: focus trapped, arrow key navigation
  → selecting: selected item highlights with accent bg, 20ms snap
  → closing: reverse slide, 220ms exit
```

### Recording State Machine (application-level)
```
IDLE
  → CONFIGURING: source panel slides in from bottom, 400ms emphasis spring
  → COUNTDOWN: panel slides out, canvas scales from 0.95→1.0 (400ms spring),
               numerals (3, 2, 1) spring-in with scale(0→1) + fade, 800ms each
  → RECORDING: floating bar springs up from bottom (280ms), canvas is full viewport,
               recording indicator pulses
  → PAUSED: floating bar icon morphs (pause→play), pulse freezes, subtle dim overlay
  → RECORDING: (resume) dim lifts, pulse resumes, icon morphs back
  → STOPPED: canvas "shutter close" (scale 1→0.95 + darken, 300ms),
             preview screen slides up (400ms spring)
  → PREVIEW: video player + controls, trim handles, action buttons
  → IDLE: preview slides down (220ms exit), source panel slides back in
```

---

## 4. Loading & Perceived Performance

| Strategy | Implementation | When used |
|----------|---------------|-----------|
| Skeleton | Rounded rectangle shapes matching device picker layout, shimmer left→right at 1.5s | Device enumeration (camera/mic list) |
| Progressive loading | Camera preview renders immediately; device list and settings populate after | Source selection screen |
| Empty state | "No camera detected" with icon + explanation + "Check permissions" CTA | Missing media devices |
| Fake progress | Whisper model download: real progress bar via fetchWithProgress | Model download |
| Optimistic | Recording starts immediately on countdown end — no "preparing" state | Record action |

### Skeleton Shimmer

```css
@keyframes v-shimmer {
  0% { background-position: -200px 0; }
  100% { background-position: 200px 0; }
}
.v-skeleton {
  background: linear-gradient(90deg,
    var(--v-bg-2) 0%, var(--v-bg-3) 50%, var(--v-bg-2) 100%);
  background-size: 400px 100%;
  animation: v-shimmer 1.5s ease-in-out infinite;
  border-radius: var(--v-radius-md);
}
```

---

## 5. Feedback Patterns

### Toast / Notification
| Property | Value |
|----------|-------|
| Position | Bottom center, 80px above floating bar (avoids overlap) |
| Auto-dismiss | 4s (info/success), persistent (errors) |
| Max visible | 1 (stack replaces) |
| Animation | Spring-in from bottom (translateY 20px→0, 280ms spring) |
| Types | success (green left border), error (red left border), info (blue left border) |

### Validation
| Trigger | Immediate on selection (no forms with submit in recording flow) |
| Error display | Inline below control, red text with spring entrance |
| Error animation | Spring-in (scale 0.95→1, opacity 0→1, 280ms) |
| Success indicator | Green checkmark next to device name after permission granted |

### Confirmation (destructive actions)
| Style | Bottom sheet with explicit confirm/discard |
| Trigger | Discard recording, delete clip |
| Animation | Sheet slides up from bottom, 400ms spring, backdrop fades to --v-scrim |

### Success Celebration
| Style | Checkmark path-draw animation via SVG stroke-dashoffset |
| Duration | Draw takes 400ms (spring), persists as saved state |
| When | Download complete, recording saved |

### Haptic Pairing (mobile via Capacitor)
| Event | Haptic |
|-------|--------|
| Record start | Medium impact |
| Record stop | Light tap |
| Countdown tick | Light tap × 3 |
| Snapshot | Medium impact |
| Error | Heavy notification |
| Download complete | Success notification |

---

## 6. Overlay & Modal Behavior

| Property | Value |
|----------|-------|
| Backdrop color | `var(--v-scrim)` = `rgba(0, 0, 0, 0.7)` |
| Backdrop blur | `blur(8px)` (lighter than nav — don't obscure recording) |
| Entry animation | Scale from 0.95→1.0 + opacity fade, spring(0.55, 0.825) |
| Entry timing | 400ms Vizvim Standard |
| Exit animation | Scale 1.0→0.97 + fade out |
| Exit timing | 220ms Vizvim Standard |
| Dismiss methods | Escape, click backdrop, explicit close button |
| Focus trap | Yes |
| Scroll lock | Yes |
| Return focus | Yes — to trigger element |

### Bottom Sheet (mobile + confirmation)
- Slides up from bottom edge, spring(0.55, 0.825)
- Detent at 50% viewport height (simple confirmations)
- Swipe down to dismiss (velocity threshold: 300px/s)
- Grab handle: 36px × 4px rounded bar at top center

### No Command Palette
Vizvim is a focused recording tool — not enough features to warrant a command palette.
Keyboard shortcuts are discoverable via tooltip hints on desktop.

---

## 7. Scroll & Navigation

### Scroll Behavior
| Property | Value |
|----------|-------|
| Type | Native scroll within source selection settings panel |
| Scroll-triggered animations | None — content is short enough to fit without scroll on most viewports |
| Infinite scroll | No — finite device list, finite settings |
| Back-to-top | No |

### Navigation Transitions
| Property | Value |
|----------|-------|
| Screen switching | Slide + fade: source panel → recording canvas → preview |
| Source → Recording | Source slides down + fades (220ms exit), canvas scales up 0.95→1.0 (400ms spring) |
| Recording → Preview | Canvas dims + scales to 0.95 (300ms), preview slides up (400ms spring) |
| Preview → Source | Preview slides down (220ms exit), source slides up (400ms spring) |

### No Sticky Header
During recording, there is NO header — just the floating control bar.
Source selection has a minimal top bar with app name + settings icon.

---

## 8. Micro-interactions Catalog

| Element | Interaction | Specification |
|---------|------------|---------------|
| Button press | Scale on :active | `scale(0.97)`, spring-interactive (0.15s response) |
| Record button press | Scale on :active | `scale(0.92)`, spring-interactive — more dramatic for hero action |
| Toggle/switch | Slide + color | Track slides with 280ms Vizvim Standard, accent bg on enable |
| Mode card select | Border + tint | Border to accent + subtle bg tint, 280ms spring |
| Device picker focus | Ring animation | `box-shadow: 0 0 0 2px var(--v-accent)`, 200ms standard |
| PiP drag | Direct manipulation | No transition during drag (spring-interactive follows pointer), spring settle on release |
| PiP resize | Corner drag | Live resize during drag, spring settle to nearest preset size on release |
| Countdown numeral | Spring-in | scale(0)→scale(1), spring(0.55, 0.825), 600ms per numeral |
| Recording pulse | Glow cycle | box-shadow oscillates, 1.5s ease-in-out infinite |
| Timer tick | None | No animation on timer — digits update without transition (clarity over flourish) |
| VU meter bars | Height change | Smooth 60fps via requestAnimationFrame, no CSS transition (real-time audio) |
| Trim handle drag | Direct manipulation | No transition during drag, spring settle on release |
| Download button | Success morph | Label → spinner → checkmark, spring transitions between |
| Snapshot | Flash | Canvas flashes white (opacity 0→0.3→0, 200ms), spring scale pop on thumbnail |
| Tooltip | Fade in | 400ms delay, then 200ms opacity fade, positioned above element |
| Card hover | Subtle lift | translateY(-2px) + shadow increase, 280ms standard |

---

## 9. API Integration Patterns

Vizvim has no backend API — it's fully client-side. These patterns apply to
browser APIs (MediaDevices, MediaRecorder, FileSystem Access, Whisper WASM).

| Pattern | Behavior |
|---------|----------|
| Device enumeration | Progressive: show skeleton, populate as `enumerateDevices()` resolves, stagger-animate entries |
| Permission request | Inline prompt with explanation before triggering browser permission dialog |
| MediaRecorder errors | Toast with error message + retry suggestion, auto-dismiss after 6s |
| Whisper model download | Real progress bar (via Content-Length), cached after first download |
| File save | Optimistic: show success immediately after `saveBlob()` call, error toast if FS Access fails |
| Offline behavior | Core recording works offline (no network needed). Whisper needs model cached. PWA service worker. |
| Codec detection | `MediaRecorder.isTypeSupported()` on init, hide unsupported export options |

---

## 10. Agent Behavior Prompt Guide

### Quick Reference: Key Behavior Tokens

| Token | Value |
|-------|-------|
| Micro timing | 20ms snap |
| Standard timing | 280ms |
| Emphasis timing | 400ms |
| Primary easing | `cubic-bezier(0.4, 0, 0.6, 1)` / `spring(0.55, 0.825)` |
| Spring interactive | `spring(0.15, 0.86)` |
| Loading style | Skeleton shimmer for lists, progressive for camera |
| Toast position | Bottom center, above floating bar |
| Button press | `scale(0.97)`, spring-interactive |
| Record press | `scale(0.92)`, spring-interactive |
| Overlay backdrop | `rgba(0, 0, 0, 0.7)` + `blur(8px)` |
| Elevation model | Subtle shadows + backdrop-filter blur |
| Screen transitions | Slide + scale with spring(0.55, 0.825) |
| Recording indicator | Pulsing glow, 1.5s ease-in-out infinite |

### Example Prompt: Source Selection Screen

> "Create a vanilla JS/CSS source selection screen with Vizvim Physical behavior:
> mode cards in a responsive grid (auto-fill, 140px min), each card has 32px icon +
> label. Cards hover with translateY(-2px) + shadow increase in 280ms
> cubic-bezier(0.4,0,0.6,1). Selected card gets 2px accent border + subtle bg tint.
> Device pickers below selected mode — skeleton shimmer (1.5s) while enumerating,
> then stagger-animate items at 40ms each. Record button at bottom: coral (#e8614d),
> pill shape, scale(0.92) on press with spring(0.15, 0.86). Dark theme: bg #0a0a0b,
> fg #f0eded, Inter font, 14px base, 10px radius on cards."

### Example Prompt: Recording Screen

> "Create the recording screen with Vizvim Physical behavior: full-viewport canvas
> with floating control bar at bottom center. Control bar: frosted glass
> (rgba(17,17,19,0.85) + blur(16px)), 52px height, 20px radius, shadow-md. Contains:
> pause button (ghost), timer display (JetBrains Mono 20px), stop button (square icon,
> accent color), VU meter (4px height, green→yellow→red gradient). Recording indicator:
> pulsing red glow on stop button (box-shadow oscillation, 1.5s infinite). PiP webcam
> overlay: draggable with spring(0.15, 0.86) follow, spring(0.55, 0.825) settle on
> release. Circular clip with 2px white border."

### Example Prompt: Preview Screen

> "Create the preview screen with Vizvim Physical behavior: centered video player
> (max 80vw), trim bar below with draggable start/end handles (spring settle on
> release). Action buttons below: Download (primary, accent), Discard (ghost, danger
> on hover), Record Again (secondary). Download button morphs: label → spinner →
> checkmark (spring transitions). Discard shows bottom sheet confirmation (slides up
> 400ms spring, backdrop rgba(0,0,0,0.7) + blur(8px))."

---

## 11. Effects Interaction Patterns

### Effect Card Toggle
```
idle
  → hover: border brightens, subtle bg shift (20ms snap)
  → active/pressed: scale(0.95), spring-interactive
  → enabled: border → accent, bg → accent-subtle, icon tints accent
    → effect applies to live preview immediately (WASM processes next frame)
  → disabled: reverse animation, effect removed from chain
```

### Effect Chain Reorder (drag)
```
idle (horizontal chip strip)
  → drag start: chip lifts (scale 1.05, shadow-md), other chips create gap
  → dragging: chip follows pointer (spring-interactive, 0.15s response)
  → drop: chip settles into new position (spring 0.55, 0.825)
  → effect order updates → WASM applies effects in new order → preview updates
```

### Effect Parameter Panel
```
collapsed
  → tap active effect chip: panel expands below (height 0 → auto, 280ms spring)
  → slider drag: direct manipulation, no transition (real-time parameter update)
  → slider release: value commits, WASM updates immediately
  → color picker: swatch opens picker overlay (280ms spring entrance)
  → tap different chip: current panel collapses (220ms exit), new panel expands (280ms spring)
```

### Preset System
```
no preset
  → save: name input slides in (280ms spring), confirm saves to localStorage
    → success: checkmark draws (SVG path, 400ms spring), toast "Preset saved"
  → load: dropdown opens (280ms spring), preset list with stagger (40ms/item)
    → select: all effects restore in stagger sequence (40ms/item), preview updates
  → export: browser save dialog, .vizvim JSON file
  → import: file picker → parse → effects restore → toast "Preset loaded"
```

### Live Preview with Effects
```
camera frame arrives (30-60fps)
  → JS draws frame to WASM shared buffer (HEAPU8 view)
  → for each effect in chain: call editor_apply_effect(id)
  → apply global adjustments (brightness/contrast/saturation/hue)
  → read buffer back → drawImage to canvas
  → canvas shows processed frame in real-time
  → no perceptible lag — WASM effects run in <2ms per frame at 1080p
```

### Category Tab Switch
```
idle tab
  → tap: active indicator slides to new tab (280ms spring), underline morphs width+position
  → effect grid crossfades (150ms opacity, immediate content swap)
  → "All" tab: shows every effect; category tabs: filtered subset
```

### Effect Layering Visual Feedback
When multiple effects are active, the chain strip shows them left-to-right in
application order. Each chip has a subtle number badge (1, 2, 3...) indicating
processing order. The preview updates after the full chain runs — users see the
combined result, not intermediate steps.

---

### Reduced Motion Adaptation

When `prefers-reduced-motion: reduce`:
- Replace spring/slide screen transitions with opacity crossfade only
- Replace countdown scale-in with opacity fade-in
- Remove PiP drag momentum (snap to position)
- Keep: recording pulse (conveys state), VU meter (real-time data), timer updates
- Keep: opacity fades, color shifts, small transforms (< 8px)
- Replace card hover lift with border highlight only
