# Video Editor Type Lang

This folder holds isolated text-style work for the editor side of `rawbuffers`.

Scope:
- no speech-to-text runtime
- no live speech-to-text
- no capture-time subtitle runtime
- yes to font, layout, color, and preset-shaped text styling

Entry point:
- `index.html` - standalone type lang for building future text overlay presets

Intent:
- keep `vizvim` focused on live capture
- keep the "font game" available for later editor integration
- preserve a simple config shape that can be mapped into `web/editor.html` later
