# SmoothVT-7 — Vacuum Tube Stereo Compressor
Smooth Life · JUCE 8 · VST3 / AU / Standalone (AAX-ready)

## Build

Requirements: CMake 3.22+, a C++17 compiler (Xcode on macOS, Visual Studio 2022 on Windows). JUCE is downloaded automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release          # add -G Xcode on macOS if you prefer
cmake --build build --config Release --target SmoothVT7_All
```

The plugins end up in `build/SmoothVT7_artefacts/Release/` (VST3, AU on macOS, Standalone).
To use a local JUCE checkout instead of downloading it: `-DJUCE_DIR=/path/to/JUCE`.

**AAX:** install the AAX SDK, add `juce_set_aax_sdk_path(/path/to/aax-sdk)` before `juce_add_plugin` and add `AAX` to `FORMATS` in `CMakeLists.txt`. Pro Tools needs the build signed through PACE/iLok.

**Before selling:** JUCE 8 is dual-licensed (AGPLv3 or commercial). A closed-source commercial release needs a JUCE commercial licence (the Starter tier is free under a revenue cap). Plugin IDs are set to manufacturer `Smlf`, plugin `Svt7`, bundle `com.smoothlife.smoothvt7`.

## GitHub

Push this folder as the root of a repo. `.github/workflows/build.yml` builds on every push to `main` and on pull requests:

- **macOS:** VST3, AU and Standalone, Universal (Apple Silicon + Intel)
- **Windows:** VST3 and Standalone
- **Linux:** VST3 and Standalone

Each run also runs the DSP tests. Download the plugins from the run's **Artifacts** section on the Actions tab. They are unsigned: macOS will block them until they're signed and notarised with an Apple Developer ID (or, for your own testing, until you clear the quarantine flag with `xattr -dr com.apple.quarantine <plugin>`).

## Tests

```bash
cmake --build build --target dsp_test     && ctest --test-dir build -C Release
cmake --build build --target plugin_test  && ./build/plugin_test_artefacts/Release/plugin_test
```

`dsp_test` checks the compressor core: threshold, Harder/Softer, tube harmonics, make-up, Comp Out, Link vs Dual, Link HPF and program-dependent Auto release.
`plugin_test` runs the real processor at 1x/2x/4x/8x oversampling, in mono and stereo, checks latency reporting and state save/restore, and opens the editor.

## Controls

| Section | Control | Range |
|---|---|---|
| Each channel | Threshold | −40 … 0 dB |
| | Gain (make-up, drives the tube stage) | 0 … +20 dB |
| | Attack | 0.5, 1, 3, 10, 30, 80 ms |
| | Release | 50, 100, 200, 400, 800 ms, Auto |
| | Tube (Harder ↔ Softer) | moves ratio 8:1 → 2:1, knee 4 → 14 dB, tube drive and 2nd-harmonic bias together |
| | Meter switch | GR (rest at 0 VU, swings left with reduction) / VU (0 VU = −18 dBFS RMS output) |
| Centre | Stereo | Dual Mono · Link · Link HPF |
| | SC HPF | Off, 60, 90, 120, 150, 200 Hz — the sidechain high-pass used in Link HPF mode |
| Bottom | Mix | dry ↔ wet, latency-compensated parallel blend |
| | Low Cut / High Cut | Off/20–120 Hz · 8–18 kHz/Off, 12 dB/oct |
| | Filter Pre/Post | filters before the tube compressor or after it |
| | Oversample | 1x, 2x, 4x, 8x (polyphase IIR, latency reported to the host) |
| | Comp Out/In | Out removes the gain reduction but keeps tubes and transformers |
| | Output Level | −9 … +9 dB |

Double-click any control to reset it. The window resizes from 50 % to 150 % with a fixed aspect ratio.

## Signal path

```
in ─┬─ dry ───────────────────────────────────────────────────────┐
    └─ [filters if PRE] → oversample ↑ → input transformer          │
         → PWM-style gain cell (feed-forward, soft knee) ← detector │
         → make-up gain → class-A triode → output transformer        │
         → DC block → oversample ↓ → [filters if POST] → wet ── mix ─┴→ output level → out
```

Detector: RMS (2 ms) → soft-knee gain computer → attack/release ballistics. In Link modes both channels take the larger reduction; Link HPF filters the sidechain so bass doesn't pump the mix. Auto release combines a slow follower (holds sustained reduction) with a fast one (rides transients), so short peaks recover fast and dense passages recover gently.

## Files

- `Source/DSP.h` — compressor core, filters, tube and transformer models (no JUCE dependency)
- `Source/PluginProcessor.*` — parameters, oversampling, filters, mix, metering
- `Source/PluginEditor.*` — faceplate, knobs, chicken-head selectors, bat toggles, VU needles
- `Assets/faceplate_bg.png` — the panel artwork (2x), printed scales and labels only; moving parts are drawn live
- `design/render.html` — source of the panel artwork. Re-render with `node design/render_background.js design/render.html Assets/faceplate_bg.png` (needs Playwright), and keep control positions in sync with `layout` in `PluginEditor.cpp`.
