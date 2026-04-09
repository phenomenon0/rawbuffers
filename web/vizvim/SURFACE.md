# SURFACE.md — Vizvim

> Control surface design specification. Defines what to expose, hide, decompose.
> Philosophy: **Opinionated** — make the decision for the user, expose only what's essential.

---

## 1. Design Philosophy

Vizvim is a creative capture tool. The core loop is: pick a source, stack effects,
experiment with raw parameters, record. Effects are not secondary — they're the
reason creators choose Vizvim over a basic screen recorder. The interface exposes
raw, combinable settings that reward experimentation. But it does so progressively:
the first thing you see is a camera preview and an effects grid. You can start
recording in 2 taps, or spend 20 minutes building a complex layered look and saving
it as a preset. Configuration should feel like adjusting a physical instrument —
dials and sliders that respond immediately, with the preview showing results in
real-time. Quality settings (resolution, bitrate, codec) are tucked away because
they're not creative decisions. Effect parameters are front and center because they are.

### Core Tension

| Axis | Position |
|------|----------|
| Simplicity ←→ Power | Center-right — effects are powerful and exposed, but recording stays simple |
| Opinionated ←→ Configurable | Configurable for creative parameters, opinionated for technical settings |
| Flat ←→ Deep | Flat — 2 screens max depth (source+effects → recording → preview) |
| Visual ←→ Keyboard | Visual-first (real-time preview is the feedback loop), keyboard shortcuts for power users |

---

## 2. Disclosure Tiers

### Tier 0 — Always Visible (core creative capture, 80%+ of users)

| Feature | Control type | Placement | Why here |
|---------|-------------|-----------|----------|
| Mode selection | Card pills (4 options) | Source panel, top | First decision — what are you capturing? |
| Camera preview | Live canvas (with effects!) | Source panel, center | See effects in real-time before recording |
| **Effect category tabs** | Horizontal tab bar | Effects panel, top | Browse the creative toolkit |
| **Effect cards** | Grid of tappable cards | Effects panel, scrollable grid | The core creative action — toggle effects on/off |
| **Active effect chain** | Horizontal chip strip | Effects panel, above grid | See what's stacked, reorder, remove |
| Record button | Large pill button | Action bar, bottom center | Hero action — always reachable |
| Stop button | Icon button | Floating bar, right | Instantly accessible during recording |
| Pause button | Icon button | Floating bar, left | Common need during recording |
| Recording timer | Mono text | Floating bar, center | Elapsed time awareness |
| VU meter | Thin bar | Floating bar, integrated | Audio confirmation |
| Preview playback | Video player | Preview screen, center | Review your capture |
| Download button | Primary button | Preview screen, below video | Save the output |
| Record Again | Secondary button | Preview screen, below video | Quick re-take |

### Tier 1 — One Interaction Away (20-50% of users)

| Feature | Control type | Access method | Why here |
|---------|-------------|--------------|----------|
| **Effect parameters** | Sliders, color pickers, toggles | Tap active effect chip → panel expands | Raw controls reward experimentation but aren't needed for every effect |
| **Global adjustments** | 4 sliders + 2 toggles | Collapsible section in effects panel | Brightness/contrast/saturation/hue are secondary to effect selection |
| **Preset save** | Icon button + name input | Preset bar below chain | Not every session creates a preset |
| **Preset load** | Dropdown | Preset bar below chain | Quick access to saved looks |
| Camera picker | Dropdown select | Below mode pills | Multi-camera users need it |
| Microphone picker | Dropdown select | Below camera picker | Relevant after mode selection |
| PiP position | Corner buttons (TL/TR/BL/BR) | Screen+Cam mode only | Compositing-specific |
| PiP shape | Segmented (circle/rectangle) | Next to PiP position | Secondary compositing control |
| Snapshot button | Icon button | Floating bar or effects panel | Useful but not every recording |
| Trim handles | Draggable markers | Preview screen timeline | Post-recording refinement |
| Discard recording | Ghost button | Preview screen, below video | Deliberate action |

### Tier 2 — Settings Drawer (5-20% of users)

| Feature | Control type | Location | Why here |
|---------|-------------|----------|----------|
| Resolution | Segmented (720p/1080p/4K) | Settings drawer, "Quality" | 1080p default covers 85%+ |
| FPS | Segmented (24/30/60) | Settings drawer, "Quality" | 30fps default covers 90%+ |
| Bitrate | Segmented (standard/high/max) | Settings drawer, "Quality" | Abstracted — "standard" = 5Mbps |
| System audio toggle | Toggle | Settings drawer, "Audio" | Desktop Chrome only — hide on unsupported |
| Mic gain | Slider | Settings drawer, "Audio" | Most trust auto-gain |
| Mic monitoring | Toggle | Settings drawer, "Audio" | Niche |
| Countdown duration | Segmented (0/3/5s) | Settings drawer, "Recording" | 3s default works for most |
| Whisper captions toggle | Toggle | Settings drawer, "Recording" | Requires model download; opt-in |
| Export format | Segmented (WebM/MP4) | Preview screen, advanced | WebM default works everywhere |
| Keyboard shortcuts | Read-only list | Settings drawer, bottom | Discoverable not configurable |
| **Preset export/import** | Buttons | Settings drawer or preset ⋯ menu | Power user preset management |

### Tier 3 — Not Exposed in GUI

| Capability | Access method | Why not in GUI |
|-----------|--------------|----------------|
| Video codec (VP8/VP9/H264) | Automatic (best supported) | Implementation detail — we pick the best codec |
| Audio codec (Opus/AAC) | Automatic | Users don't choose audio codecs |
| Audio bitrate | Hardcoded 256kbps | Good enough for voice, no reason to expose |
| Canvas captureStream FPS | Matches selected FPS | Internal synchronization detail |
| Whisper model variant | Hardcoded tiny-q5_1 | One model, one quality level — simpler |
| MediaRecorder timeslice | Hardcoded 1000ms | Implementation detail |
| Chunk buffer strategy | Internal | Memory management, not a user concern |

### Not Exposed — Decided for the User

| Capability | Chosen default | Rationale |
|-----------|---------------|-----------|
| Recording format | WebM container | Universal browser support, good compression |
| Audio sample rate | 48kHz (browser default) | Standard quality, no benefit from exposing |
| PiP size | Small (15% of canvas) | Tested default; resize via drag if needed |
| Caption font size | 24px, white with black outline | Readable on all backgrounds |
| Caption position | Bottom center | Standard convention |
| Auto-save on stop | No (user chooses to save) | Prevents accidental storage fill |
| Max recording duration | None (unlimited) | Trust the user |
| File naming | `vizvim-{timestamp}.webm` | Consistent, sortable, no config needed |

---

## 3. Screen Decomposition

### Screen Map

```
Vizvim
├── Source + Effects (default/home screen)
│   ├── Source Panel (left/top)
│   │   ├── Mode pills (Screen+Cam, Screen, Camera, Audio)
│   │   ├── Live camera preview (WITH effects applied)
│   │   ├── Device pickers (camera, mic) — conditional on mode
│   │   └── Settings drawer (gear icon → bottom sheet)
│   ├── Effects Panel (right/bottom)
│   │   ├── Active effect chain (draggable chips)
│   │   ├── Category tabs (All, Color, Retro, Glitch, Audio, ML, Stylize)
│   │   ├── Effect cards grid (tap to toggle)
│   │   ├── Parameter panel (expanded per active effect)
│   │   ├── Global adjustments (collapsible: brightness/contrast/saturation/hue)
│   │   └── Preset bar (load/save/export)
│   └── Action bar: Record button
├── Recording (full viewport)
│   ├── Canvas (source + effects composited live)
│   ├── Floating control bar (pause, timer, VU, stop, snapshot)
│   ├── PiP overlay (draggable, Screen+Cam mode)
│   └── Caption overlay (if Whisper enabled)
└── Preview
    ├── Video player (effects baked in)
    ├── Trim timeline
    ├── Caption review (if applicable)
    └── Action buttons (Download, Record Again, Discard)
```

No settings "page" — settings live in a bottom sheet/drawer accessible from source screen.
Effects panel is the creative workspace — always visible alongside the preview.

### Navigation Model

| Property | Value |
|----------|-------|
| Primary navigation | Linear flow: Source → Recording → Preview → Source |
| Max top-level destinations | 3 (the three screens — no tabs, no sidebar) |
| Max depth from root | 1 (source screen is root; recording and preview are 1 deep) |
| Command palette | No — not enough features to warrant one |
| Search | No — nothing to search |

### Screen Budget Rules

| Rule | Constraint |
|------|-----------|
| Max items per screen (no scroll) | Source: 8-10 (mode cards + pickers + preview + record). Recording: 5 (canvas + bar controls). Preview: 6 (player + trim + 3 buttons). |
| Max form fields per group | Settings drawer: 3-4 per section (Quality, Audio, Recording) |
| Max tabs per tabbar | N/A — no tab bar |
| Max settings depth | 1 level (flat drawer, no sub-pages) |
| Total settings count | 9 (resolution, fps, bitrate, system audio, gain, monitoring, countdown, shortcuts, format) |

---

## 4. Control Selection

### Control Decision Tree (Vizvim-specific)

| Data shape | Control type | Vizvim-specific notes |
|-----------|-------------|-------------------|
| Binary (immediate) | Toggle switch | System audio, mic monitoring, Whisper — all take effect immediately |
| 2-3 options | Segmented control | Resolution (720/1080/4K), FPS (24/30/60), countdown (0/3/5), format (WebM/MP4) |
| 4 options (compare) | Card grid | Mode selection — visual cards, not radio buttons (the identity of the app) |
| Continuous range (approx) | Slider | Mic gain (with numeric readout) |
| 4+ options (no compare) | Dropdown | Camera picker, mic picker (dynamic device list) |
| PiP position | 4 corner buttons | Visual grid showing corners — not a dropdown |
| PiP shape | Segmented (circle/rect) | 2 options, visual comparison matters |

### Control Rules

| Rule | Specification |
|------|--------------|
| Toggles take effect | Immediately. No save button. |
| Destructive actions require | Bottom sheet confirmation: "Discard recording?" with red discard + cancel |
| Validation timing | Immediate — if a device is unavailable, show inline error right away |
| Empty state for device lists | "No cameras found" with explanation + "Check permissions" CTA |
| Overflow for long lists | N/A — device lists are short |

---

## 5. Default Values Strategy

### Default Selection Rules

| Rule | Specification |
|------|--------------|
| Safety bias | Recording starts paused (countdown gives time to prepare). Discard requires confirmation. |
| Majority matching | If >80% would choose the same, hardcode it (audio bitrate, codec, model variant) |
| Progressive revelation | First visit: show mode cards + Record button only. Settings drawer discoverable via gear icon. |
| Recovery | Every action reversible: re-record, re-trim, re-download |

### Key Defaults

| Setting | Default value | % who change | Rationale |
|---------|--------------|-------------|-----------|
| Mode | Camera Only | ~40% | Safest default — works on all platforms, no permissions beyond camera |
| Resolution | 1080p | ~15% | Standard quality, good for sharing |
| FPS | 30 | ~10% | Smooth enough for all use cases |
| Bitrate | Standard (5Mbps) | ~5% | Good quality-to-size ratio |
| Countdown | 3 seconds | ~20% | Gives time to prepare |
| System audio | Off | ~30% | Not available on all platforms; opt-in is safer |
| Mic gain | 1.0 (unity) | ~15% | Trust browser auto-gain |
| Whisper captions | Off | ~20% | Requires model download; opt-in |
| PiP position | Bottom-right | ~25% | Convention from video calls |
| PiP shape | Circle | ~15% | Friendlier, more modern feel |
| Export format | WebM | ~10% | Universal support |

### Candidates for Elimination

| Setting | Current default | Change rate | Recommendation |
|---------|----------------|-------------|---------------|
| Audio bitrate | 256kbps | <2% | Hardcode — nobody adjusts this |
| Audio codec | Opus | <1% | Hardcode — best available |
| Video codec | VP8/VP9 auto | <1% | Hardcode — pick best supported |
| Caption style | White + black outline | <5% | Hardcode — universally readable |

---

## 6. Progressive Disclosure Strategy

### Disclosure Layers

| Layer | What's visible | How it's discovered | Target user |
|-------|---------------|---------------------|-------------|
| Layer 0 — Quick capture | Mode pills + effects grid + Record button | Default state on launch | Everyone — record with or without effects in 2 taps |
| Layer 1 — Customize look | Effect parameters, adjustments, presets, device pickers | Tap active effect chip, tap Adjustments, tap mode pill | Creators building a look |
| Layer 2 — Tune quality | Resolution, FPS, bitrate, audio, Whisper | Gear icon → settings drawer | Technical users, podcasters |
| Layer 3 — Export/share presets | .vizvim JSON export/import | Preset ⋯ menu | Power users sharing looks |

### Disclosure Triggers

| Trigger | Reveals | Example |
|---------|---------|---------|
| App launch | Camera preview + effects grid + Record button | User can record immediately or browse effects |
| Tap an effect card | Effect toggles on, chip appears in chain, preview updates live | Tap "VHS" → instant VHS filter on camera |
| Tap active effect chip | Parameter panel expands below chain | Tap "VHS" chip → aberration slider, noise slider appear |
| Select a mode pill | Device pickers appear | Tap "Screen + Cam" → camera dropdown, mic dropdown, PiP controls |
| Tap gear icon | Settings drawer slides up | Quality, audio, recording, and Whisper settings |
| Toggle Whisper on | Model download prompt (first time) | "Download speech model (32MB)?" with progress bar |
| Stop recording | Preview screen with trim + actions | Full post-recording workflow |
| Tap preset ⋯ | Export/import/delete menu | Power user preset management |

### "Advanced" Section Rules

| Rule | Specification |
|------|--------------|
| When to create an "Advanced" section | Not needed — settings drawer IS the advanced section. Flat, not nested. |
| Collapsed by default | The entire drawer is collapsed (closed). Individual sections within are NOT collapsed. |
| Label | Gear icon (⚙) — no text label needed, universally understood |
| Position | Bottom sheet / drawer, slides up from bottom edge |

---

## 7. Constraint Communication

### Constraint Types

| Type | Communication method | Example |
|------|---------------------|---------|
| Hard limit (platform) | Hide unavailable modes | Mobile: "Screen + Cam" and "Screen Only" cards hidden (getDisplayMedia unavailable) |
| Permission (browser) | Inline prompt before trigger | "Vizvim needs camera access" → then browser permission dialog |
| Permission denied | Inline error + Settings link | "Camera blocked. Open browser settings to allow." |
| Feature dependency | Disabled with tooltip | Whisper toggle disabled until model downloaded. Trim disabled if recording < 2s. |
| Format support | Hide unsupported | MP4 export hidden if WebCodecs unavailable |

### Constraint Display Rules

| Rule | Specification |
|------|--------------|
| Hide vs disable | **Hide** features the platform can't support (screen recording on mobile). **Disable** features blocked by a prerequisite (with inline explanation). |
| Error prevention | Use constrained controls: mode cards only show available modes. Export shows only supported formats. Prevent the error, don't catch it. |
| Upgrade prompts | N/A — Vizvim is free. All features available on supported platforms. |

---

## 8. Workflow Decomposition

### Workflow Map

| Workflow | Steps | Pattern | Rationale |
|----------|-------|---------|-----------|
| Quick capture | 2 | Record → Stop → Download | Zero config — default camera + no effects |
| Creative capture | 4 | Browse effects → Stack + tweak → Record → Download | The core creative workflow |
| Preset capture | 3 | Load preset → Record → Download | Reuse a saved look |
| Build a look | 5 | Stack effects → Tweak parameters → Adjust globals → Save preset → Record | Deep creative session |
| Re-take | 2 | Preview → Discard (confirm) → Record Again | Fast iteration loop |
| Caption recording | 3 | Enable Whisper → Record → Download + SRT | Accessibility / subtitle workflow |
| Snapshot | 1 | During recording → tap snapshot → auto-saved | Interrupt-free capture |
| Share a look | 3 | Build look → Save preset → Export .vizvim → Share file | Creative collaboration |

### No Wizard
Vizvim does not use a wizard pattern. The flow is spatial (source+effects → record → preview),
not a multi-step form. Effects are always visible alongside the preview — you experiment
and record in the same space, not in separate steps.

---

## 9. Information Density

| Property | Value |
|----------|-------|
| Density mode | Comfortable — generous spacing, large touch targets (44px min) |
| Data table max columns | N/A — no data tables |
| Card grid columns | 2 (mobile), 4 (desktop) for mode cards |
| List item max info | Device name + type indicator (camera/mic icon) |
| Truncation strategy | Ellipsis at end for long device names |

### Detail View Pattern

| Approach | When used |
|----------|----------|
| Inline expand | Settings drawer sections (all flat, no sub-navigation) |
| Full page | Recording screen (immersive, full viewport) |
| Modal (sheet) | Discard confirmation, Whisper download prompt |

---

## 10. Agent Surface Prompt Guide

### Quick Reference: Key Surface Tokens

| Token | Value |
|-------|-------|
| Philosophy | Opinionated — good defaults, minimal settings |
| Max screen depth | 1 level (source → recording or preview) |
| Max nav items | N/A — linear flow, not tabbed |
| Settings approach | Flat drawer, 9 items, no sub-pages |
| Control default | Toggles immediate, no save button |
| Disclosure pattern | Mode selection reveals pickers; gear icon reveals settings drawer |
| Constraint style | Hide unavailable platform features, disable unmet prerequisites with tooltip |
| Density | Comfortable, 44px touch targets, generous spacing |
| Detail view | Full-viewport for recording, bottom sheet for confirmations |

### Example Prompt: Source Selection

> "Decompose Vizvim's source selection into an Opinionated-style screen:
> Tier 0: 4 mode cards (Screen+Cam, Screen, Camera, Audio) — always visible, 2-col
> grid on mobile, 4-col on desktop. Record button always at bottom. Tier 1: device
> pickers appear ONLY after selecting a mode that needs them. Camera preview inline.
> PiP controls only for Screen+Cam mode. Tier 2: gear icon opens flat settings drawer
> (9 items, no sub-pages). Hide screen modes on mobile (getDisplayMedia unavailable).
> Disable Whisper toggle until model cached. All toggles immediate, no save button."

### Example Prompt: Recording Screen

> "Build Vizvim's recording screen as full-viewport immersive:
> Canvas fills 100vw × 100vh. Floating control bar fixed at bottom center — only
> 5 elements visible: pause, timer, VU meter, snapshot, stop. No header, no sidebar,
> no settings during recording. PiP overlay is a draggable circle (Tier 1 — only in
> Screen+Cam mode). Caption overlay at bottom center (Tier 1 — only if Whisper
> enabled). Recording indicator is the glow on the stop button — no separate badge.
> Keyboard shortcuts discoverable via tooltip on desktop hover."
