# DESIGN.md — Vizvim

> Visual design specification. Defines the look of Vizvim — a standalone recording app
> built on the Physical (Apple-style) archetype.

---

## 1. Identity

**Name**: Vizvim
**Tagline**: Capture everything. Own your footage.
**Archetype**: Physical — spring-based, tactile, premium
**Audience**: Creators, educators, developers — anyone who records screen, camera, or both
**Tone**: Confident, minimal, warm. Not clinical. Not playful. Professional with soul.

---

## 2. Color System

### Palette: "Ember Dark"

A dark recording-appropriate palette with warm undertones. The accent is a deep coral/ember
that evokes the red recording indicator while feeling more refined than raw red.

#### Semantic Tokens

| Token | Value | Role |
|-------|-------|------|
| `--v-bg-0` | `#0a0a0b` | Base — deepest background |
| `--v-bg-1` | `#111113` | Surface — panels, cards |
| `--v-bg-2` | `#19191c` | Elevated — hover targets, containers |
| `--v-bg-3` | `#222226` | Interactive — buttons, inputs |
| `--v-bg-4` | `#2c2c31` | Active — pressed states |
| `--v-fg-0` | `#f0eded` | Primary text |
| `--v-fg-1` | `#b8b3b0` | Secondary text |
| `--v-fg-2` | `#78736f` | Tertiary / disabled text |
| `--v-accent` | `#e8614d` | Primary accent — coral/ember |
| `--v-accent-hover` | `#f07560` | Accent hover state |
| `--v-accent-dim` | `#c04a3a` | Accent pressed/active |
| `--v-accent-subtle` | `rgba(232, 97, 77, 0.12)` | Accent background tint |
| `--v-rec` | `#e8614d` | Recording indicator (same as accent — intentional) |
| `--v-rec-glow` | `rgba(232, 97, 77, 0.35)` | Recording pulse glow |
| `--v-success` | `#34d399` | Success states |
| `--v-warn` | `#fbbf24` | Warning states |
| `--v-error` | `#f87171` | Error states |
| `--v-info` | `#60a5fa` | Informational |
| `--v-border` | `rgba(255, 255, 255, 0.06)` | Default border |
| `--v-border-hover` | `rgba(255, 255, 255, 0.12)` | Hover border |
| `--v-glass` | `rgba(255, 255, 255, 0.03)` | Glass/frosted overlay |
| `--v-scrim` | `rgba(0, 0, 0, 0.7)` | Backdrop scrim |

#### Dark Mode

Vizvim is dark-only. Recording apps benefit from a dark UI — less light bleed into
webcam footage, less distraction during screen capture, and the recording indicator
(coral/ember) stands out against dark backgrounds.

---

## 3. Typography

### Font Stack

| Role | Family | Fallback | Weight | Use |
|------|--------|----------|--------|-----|
| Body | `'Inter'` | `-apple-system, system-ui, sans-serif` | 400, 500 | All body text, descriptions |
| Label | `'Inter'` | `-apple-system, system-ui, sans-serif` | 500, 600 | Buttons, labels, headings |
| Mono | `'JetBrains Mono'` | `'SF Mono', 'Fira Code', monospace` | 400 | Timer, duration, file sizes, technical |

### Type Scale

| Token | Size | Line Height | Weight | Use |
|-------|------|-------------|--------|-----|
| `--v-text-xs` | 11px | 1.45 | 400 | Captions, hints, status bar |
| `--v-text-sm` | 13px | 1.45 | 400 | Secondary labels, metadata |
| `--v-text-base` | 14px | 1.5 | 400 | Body text, descriptions |
| `--v-text-md` | 15px | 1.45 | 500 | Button text, primary labels |
| `--v-text-lg` | 18px | 1.35 | 600 | Section headings |
| `--v-text-xl` | 24px | 1.25 | 600 | Screen titles |
| `--v-text-2xl` | 36px | 1.15 | 700 | Countdown numerals |
| `--v-text-timer` | 20px | 1 | 400 | Recording timer (mono) |

### Font Loading

Inter and JetBrains Mono via Google Fonts with `display=swap`. System fallback renders
immediately; web fonts swap in without layout shift (sizes matched to system).

---

## 4. Spacing System

8px base grid.

| Token | Value | Use |
|-------|-------|-----|
| `--v-space-0` | 0 | Reset |
| `--v-space-1` | 4px | Tight internal padding, icon gaps |
| `--v-space-2` | 8px | Standard internal padding |
| `--v-space-3` | 12px | Component internal padding |
| `--v-space-4` | 16px | Section padding, card padding |
| `--v-space-5` | 24px | Group separation |
| `--v-space-6` | 32px | Section separation |
| `--v-space-7` | 48px | Screen-level spacing |
| `--v-space-8` | 64px | Major section breaks |

---

## 5. Border & Radius

| Token | Value | Use |
|-------|-------|-----|
| `--v-radius-sm` | 6px | Small elements (chips, tags) |
| `--v-radius-md` | 10px | Buttons, inputs, small cards |
| `--v-radius-lg` | 14px | Cards, panels, mode cards |
| `--v-radius-xl` | 20px | Modals, sheets, major containers |
| `--v-radius-full` | 9999px | Pills, circular buttons, PiP webcam |

Physical archetype uses **generous radii** — rounder than Precision (which uses 2-4px).
This gives the tactile, friendly feel that differentiates Vizvim from rawcut's engineering aesthetic.

---

## 6. Elevation & Shadow

Physical archetype uses subtle shadows + backdrop-filter blur (not luminance stepping).

| Token | Value | Use |
|-------|-------|-----|
| `--v-shadow-sm` | `0 1px 3px rgba(0,0,0,0.3), 0 1px 2px rgba(0,0,0,0.2)` | Cards, subtle lift |
| `--v-shadow-md` | `0 4px 12px rgba(0,0,0,0.35), 0 2px 4px rgba(0,0,0,0.2)` | Floating controls, dropdowns |
| `--v-shadow-lg` | `0 12px 40px rgba(0,0,0,0.4), 0 4px 12px rgba(0,0,0,0.25)` | Modals, sheets, overlays |
| `--v-shadow-glow` | `0 0 20px var(--v-rec-glow)` | Recording indicator glow |
| `--v-blur` | `blur(16px) saturate(150%)` | Frosted glass panels |

---

## 7. Component Tokens

### Buttons

| Variant | Background | Text | Border | Radius |
|---------|-----------|------|--------|--------|
| Primary | `var(--v-accent)` | `#ffffff` | none | `--v-radius-md` |
| Secondary | `var(--v-bg-3)` | `var(--v-fg-0)` | `1px solid var(--v-border)` | `--v-radius-md` |
| Ghost | `transparent` | `var(--v-fg-1)` | none | `--v-radius-md` |
| Danger | `var(--v-error)` | `#ffffff` | none | `--v-radius-md` |
| Record | `var(--v-rec)` | `#ffffff` | none | `--v-radius-full` |

Minimum touch target: 44px (Apple HIG).
Button padding: `10px 20px` (default), `8px 16px` (compact).

### Mode Cards

Source selection cards (Screen+Cam, Screen Only, Camera Only, Audio Only).

| Property | Value |
|----------|-------|
| Background | `var(--v-bg-1)` |
| Border | `1px solid var(--v-border)` |
| Radius | `var(--v-radius-lg)` |
| Padding | `var(--v-space-4)` |
| Selected border | `2px solid var(--v-accent)` |
| Selected bg | `var(--v-accent-subtle)` |
| Icon size | 32px |
| Min width | 140px |
| Gap between cards | `var(--v-space-3)` |

### Inputs / Selects

| Property | Value |
|----------|-------|
| Background | `var(--v-bg-2)` |
| Border | `1px solid var(--v-border)` |
| Radius | `--v-radius-md` |
| Height | 40px |
| Focus ring | `0 0 0 2px var(--v-accent)` |
| Text | `var(--v-fg-0)` |
| Placeholder | `var(--v-fg-2)` |

### Floating Control Bar (recording screen)

| Property | Value |
|----------|-------|
| Background | `rgba(17, 17, 19, 0.85)` with `var(--v-blur)` |
| Border | `1px solid var(--v-border)` |
| Radius | `var(--v-radius-xl)` |
| Shadow | `var(--v-shadow-md)` |
| Padding | `8px 16px` |
| Height | 52px |
| Position | Fixed bottom center, 24px from edge |

### VU Meter

| Property | Value |
|----------|-------|
| Bar color (low) | `var(--v-success)` |
| Bar color (mid) | `var(--v-warn)` |
| Bar color (high) | `var(--v-error)` |
| Background | `var(--v-bg-3)` |
| Height | 4px |
| Radius | `--v-radius-full` |

---

## 8. Iconography

System: Lucide icons (open source, consistent 24px grid, 1.5px stroke).

| Icon | Use |
|------|-----|
| `circle` (filled) | Record button |
| `square` (filled) | Stop button |
| `pause` | Pause button |
| `play` | Resume / preview play |
| `camera` | Camera source |
| `monitor` | Screen source |
| `mic` | Microphone |
| `volume-2` | System audio |
| `download` | Download recording |
| `scissors` | Trim |
| `subtitles` | Whisper captions |
| `settings` | Advanced settings |
| `x` | Close / discard |
| `check` | Confirm / success |
| `image` | Snapshot |

Icon size: 20px (controls), 32px (mode cards), 16px (inline/status).

### Effect Icons (per category)

| Icon | Category |
|------|----------|
| `palette` | Color effects (thermal, night vision, invert, channel split) |
| `film` | Retro effects (VHS, old film) |
| `zap` | Glitch effects (glitch, datamosh) |
| `music` | Audio-reactive (FFT grade, beat glitch, waveform) |
| `brain` | ML effects (bg removal, pose, face, depth) |
| `layers` | Compositing (PiP, chroma key, blend modes) |
| `sliders-horizontal` | Adjustments (brightness, contrast, saturation, hue) |
| `eye` | Stylize (edge detection, pixelate, tilt shift) |
| `bookmark` | Presets |
| `save` | Save preset |
| `upload` | Import preset |
| `download` | Export preset |
| `grip-vertical` | Drag handle (effect chain reorder) |

---

## 9. Effects UI Components

### Effect Category Tabs

Horizontal scrollable tab bar above the effects grid.

| Property | Value |
|----------|-------|
| Background | `var(--v-bg-1)` |
| Tab padding | `8px 16px` |
| Tab font | `var(--v-text-sm)`, weight 500 |
| Tab color (inactive) | `var(--v-fg-2)` |
| Tab color (active) | `var(--v-fg-0)` |
| Active indicator | 2px bottom border, `var(--v-accent)` |
| Icon size | 16px, inline before label |
| Gap between tabs | `var(--v-space-1)` |
| Overflow | Horizontal scroll, no scrollbar visible (CSS `scrollbar-width: none`) |

Categories: All, Color, Retro, Glitch, Audio, ML, Stylize, Compositing

### Effect Cards

Small, tappable cards in a grid. Each represents one effect.

| Property | Value |
|----------|-------|
| Background | `var(--v-bg-2)` |
| Border | `1px solid var(--v-border)` |
| Radius | `var(--v-radius-md)` |
| Padding | `var(--v-space-2) var(--v-space-3)` |
| Size | Flexible, min 100px wide |
| Icon | 20px, centered above label |
| Label | `var(--v-text-xs)`, `var(--v-fg-1)`, centered |
| Active border | `2px solid var(--v-accent)` |
| Active bg | `var(--v-accent-subtle)` |
| Active icon | tinted `var(--v-accent)` |
| Disabled | `opacity: 0.35`, no pointer events |
| Grid | `auto-fill, minmax(100px, 1fr)`, gap `var(--v-space-2)` |

### Active Effect Chain

A horizontal strip showing currently enabled effects in order. Draggable to reorder.

| Property | Value |
|----------|-------|
| Container bg | `var(--v-bg-0)` |
| Container border | `1px solid var(--v-border)` |
| Container radius | `var(--v-radius-md)` |
| Container padding | `var(--v-space-2)` |
| Container height | 44px |
| Chip bg | `var(--v-bg-3)` |
| Chip border | `1px solid var(--v-border-hover)` |
| Chip radius | `var(--v-radius-sm)` |
| Chip padding | `4px 10px` |
| Chip font | `var(--v-text-xs)`, `var(--v-fg-0)` |
| Chip icon | 14px, left of label |
| Chip remove (×) | 14px, right side, `var(--v-fg-2)` → `var(--v-error)` on hover |
| Drag handle | `grip-vertical` icon, 12px, `var(--v-fg-2)` |
| Empty state | "No effects active" in `var(--v-fg-2)`, centered |
| Overflow | Horizontal scroll |

### Effect Parameter Panel

Expandable panel below an active effect chip, showing raw controls.

| Property | Value |
|----------|-------|
| Background | `var(--v-bg-1)` |
| Border | `1px solid var(--v-border)` |
| Radius | `var(--v-radius-lg)` |
| Padding | `var(--v-space-4)` |
| Max height | 300px (scrollable) |
| Shadow | `var(--v-shadow-sm)` |

#### Parameter Controls

| Control | Use | Spec |
|---------|-----|------|
| Slider | Continuous values (gain, intensity, threshold) | Track: 4px, `var(--v-bg-4)`. Thumb: 16px circle, `var(--v-accent)`. Fill: `var(--v-accent)`. Value label: mono, right-aligned. |
| Color picker | RGB values (chroma key, fog, waveform) | 32px swatch circle + hex input. Swatch border: `var(--v-border)`. |
| Segmented control | Discrete options (palette, mode, position) | Same as general segmented. Active: `var(--v-accent)` bg. |
| Toggle | Binary (mono edge, flip H/V) | 40px × 22px track, 18px thumb. Active: `var(--v-accent)`. |
| Stepper | Small integers (block size, brush size) | −/+ buttons with numeric display between. |

### Preset Bar

Horizontal bar below the effect chain, for preset management.

| Property | Value |
|----------|-------|
| Background | transparent |
| Height | 36px |
| Layout | Row: [preset dropdown] [save] [⋯ more] |
| Preset dropdown | Ghost style, `var(--v-text-sm)`, shows current preset name or "No preset" |
| Save button | Ghost icon button (`save` icon, 20px) |
| More menu | "Export", "Import", "Delete" |

### Global Adjustments Panel

Collapsible section with 4 sliders + 2 toggles.

| Property | Value |
|----------|-------|
| Trigger | "Adjustments" label with chevron, collapsible |
| Sliders | Brightness, Contrast, Saturation (range -100 to 100), Hue (0-360) |
| Toggles | Flip H, Flip V |
| Reset button | Ghost "Reset" next to section title |

---

## 10. Layout Breakpoints

| Name | Width | Layout |
|------|-------|--------|
| Mobile | < 640px | Single column, stacked cards, full-width controls |
| Tablet | 640-1024px | Two-column source selection, wider preview |
| Desktop | > 1024px | Side panel for settings, spacious preview |

### Layout Grid

**Source + Effects screen (desktop)**:
```
┌─────────────────────────────────────────────────────────────┐
│  Vizvim                                          ⚙  │ top bar
├────────────────────────────┬────────────────────────────────┤
│                            │ [All] [Color] [Retro] [Glitch]│ category tabs
│   ┌──────────────────┐     │ ┌──────┐ ┌──────┐ ┌──────┐   │
│   │                  │     │ │Thermal│ │ Edge │ │NightV│   │ effect cards
│   │  Camera Preview  │     │ └──────┘ └──────┘ └──────┘   │
│   │  (with effects)  │     │ ┌──────┐ ┌──────┐ ┌──────┐   │
│   │                  │     │ │ VHS  │ │Glitch│ │Pixel │   │
│   └──────────────────┘     │ └──────┘ └──────┘ └──────┘   │
│                            │                                │
│  [Screen+Cam] [Screen]     │ Active: [VHS ×] [Glitch ×]   │ effect chain
│  [Camera ✓]  [Audio]      │ ▸ Adjustments                 │
│                            │ Preset: [None ▾] [💾] [⋯]    │
│  Camera: [Logitech ▾]     │                                │
│  Mic:    [Default ▾]      │                                │
├────────────────────────────┴────────────────────────────────┤
│            ⏺ Record                                        │ action bar
└─────────────────────────────────────────────────────────────┘
```

**Source + Effects screen (mobile)**:
```
┌───────────────────────┐
│  Vizvim            ⚙  │
├───────────────────────┤
│  ┌─────────────────┐  │
│  │ Camera Preview  │  │
│  │ (with effects)  │  │
│  └─────────────────┘  │
│  [Cam ✓] [Screen]     │ mode pills
│  Camera: [Logitech ▾] │
├───────────────────────┤
│  [VHS ×] [Glitch ×]  │ effect chain (swipeable)
│  [Color][Retro][Glitch]│ category tabs (scroll)
│  ┌────┐┌────┐┌────┐  │
│  │Thrm││Edge││NgtV│  │ compact cards
│  └────┘└────┘└────┘  │
│  ▸ Adjustments        │
├───────────────────────┤
│      ⏺ Record        │
└───────────────────────┘
```

- Recording screen: Full viewport canvas, floating controls
- Preview: Centered video with controls below

---

## 10. Motion Tokens (referenced by BEHAVIOR.md)

| Token | CSS | Spring | Use |
|-------|-----|--------|-----|
| `--v-micro` | `20ms` | — | Hover snap, focus, color |
| `--v-standard` | `280ms` | `spring(0.55, 0.825)` | Card select, menu, toggle |
| `--v-emphasis` | `400ms` | `spring(0.55, 0.825)` | Screen transition, modal |
| `--v-exit` | `220ms` | — | Close, dismiss |
| `--v-spring-interactive` | — | `spring(0.15, 0.86)` | PiP drag, button press |
| `--v-stagger` | `40ms/item` | — | List items, device cards |
