# Amethyst

Keyboard-, MIDI- and remote-driven VJ software for **MilkDrop presets** on Windows, built on
**libprojectM 4.2** (the C++ MilkDrop engine), SDL2, OpenGL 3.3 and Dear ImGui.

## Download

Get **`Amethyst-win64.zip`** from the [Releases](https://github.com/westeriann/amethyst/releases) page,
unzip it anywhere and run **`Amethyst.exe`** — the folder is self-contained (Windows 10/11, OpenGL 3.3 GPU).

**Presets:** the download includes two sample presets. Drop any folder of `.milk` presets onto the window
(or Ctrl+Shift+O). For a big library, grab projectM's curated
[Cream of the Crop pack](https://github.com/projectM-visualizer/presets-cream-of-the-crop) (~9,800 presets)
and the [texture pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack), and put them in
`Documents\Amethyst\Presets` and `Documents\Amethyst\Textures` — Amethyst adds that folder automatically.

The old browser-based prototype lives in [`browser-version/`](browser-version/) (Butterchurn/WebGL).

## Features

**Playback**
* Two decks (A/B) with a crossfader and 7 mix modes: crossfade, add, screen, multiply, difference,
  lighten, luma key. `Tab` switches which deck you're driving; `\` auto-crossfades.
* Auto-pilot (in order or shuffled, optional A/B auto-mixing), launch quantize to beat / bar / 4 bars.
* Tempo: automatic BPM + phase detection from the audio, tap tempo (`T`), or **Ableton Link**.
* 10 cue slots with live thumbnails.

**Look**
* Post effects: kaleidoscope, mirror, feedback trails, RGB split, pixelate, invert, beat-synced strobe,
  zoom / rotate / swirl, hue / saturation / contrast / brightness (Effects panel, `W`, F2–F8).
* Overlays: a looping **video layer** (mp4/mov/wmv... with blend modes), a **logo/image**, and **text**
  (static or scrolling ticker). Drop an image or video onto the window to use it.

**Output**
* Any number of output windows, each on any display, showing the final mix or a single deck, with
  **corner-pin projection mapping** and a test grid (Outputs & mapping panel).
* **Spout** sender "Amethyst" for OBS, Resolume, TouchDesigner (F12).
* **MP4 recording** (H.264, 24/30/60 fps) to `Videos\Amethyst` (F9).

**Scenes & show**
* 16 **scenes**: a whole look (both deck presets, crossfader, mix mode, effects, overlays). Recall with
  Shift+F1…F8, store with Ctrl+Shift+F1…F8 (or Shift+click in the Scenes panel, Ctrl+F1). Scenes **morph**
  over a set number of beats.
* **Show timeline**: a list of scenes, each held for N bars, locked to the beat clock (Ctrl+T plays/stops).
* **Transitions** between decks: wipe, iris, zoom-through, glitch blocks, dissolve, luma fade, slide, timed in
  beats (Mixer panel; Shift+J cycles).

**Modulation** (Y)
* Route bass / mids / treble / level / beat pulse or tempo-synced LFOs (sine, saw, square, random) to zoom,
  rotate, swirl, hue, saturation, contrast, brightness, RGB split, trails, pixelate, kaleidoscope, crossfader or
  overlay opacity. One-click starters: kick -> zoom pulse, hats -> RGB split, slow hue drift.

**DJ link** (Control > DJ link)
* Connects to **Denon Engine OS** gear over the network (StageLinQ): follows the on-air deck's BPM and beat,
  and on track changes can switch to the next / a random preset or the next scene, and show "Artist - Title".
  Serato, rekordbox and Traktor: use **Ableton Link** as the tempo source instead.

**Live inputs**
* The video layer can show a video file, a **camera / capture card**, or a **Spout input** from another app.

**Lighting** (Outputs panel)
* **Art-Net** DMX output: each fixture takes the colour of a slice of the output (RGB, RGBW, Dim+RGB,
  Dim+RGB+Strobe), so lights follow the visuals.

**Control**
* **Fully custom keyboard**: `Ctrl+K` opens a visual keyboard. Drag an action onto a key, click a key to
  pick its action, or double-click an action and press any key combo. Shift / Ctrl / Alt layers;
  drag between keys to move a shortcut, right-click to clear. The help screen (`H`) is generated from
  your current layout, so it always matches.
* **MIDI learn** for every action (Control > MIDI): pads on buttons, knobs/faders on sliders.
  **LED feedback** lights mapped pads (toggles on/off, filled cues, active scene). Ready-made **templates** for
  Launchpad Mini MK3 / X / Pro MK3 (programmer mode) and Akai APC40 mkII (grid, scene buttons, crossfader, knobs).
* **OSC** (`/amethyst/<action> [value]`, UDP 9000) and a **phone web remote** (port 8080), both opt-in.

**Library**
* Live, audio-reactive preview thumbnails (own GPU thread, never stalls the output).
* **Auto-analysis** of every preset in the background (dominant colour, brightness, motion). It pauses itself
  whenever the show needs the GPU, and results are cached. Pools by colour, Calm / Intense, Dark / Bright, and
  **More like this** (right-click a preset, or Ctrl+M). **Hide broken** presets.
* Favorites (`Ctrl+D`), star ratings (`Ctrl+1…5`), tags, setlists with drag-to-reorder (`Ctrl+L`
  adds to the active one), pools (All / Favorites / rating / tag / setlist), folder categories, search.
* Live **preset editor** (`E`): edit the .milk code, apply instantly (`Ctrl+Enter`), save as a new preset.

**Interface**
* Every panel hides individually, and `U` hides everything. Panels move and resize; the layout is remembered.

## Default keys (all re-assignable with Ctrl+K)

| Presets | | Mixer & tempo | | Performance & FX | | View & files | |
|---|---|---|---|---|---|---|---|
| Space / → | next | Tab | focus deck A/B | B | blackout | U | hide all UI |
| ← | previous | Ctrl+← / → | crossfader | X | flash | P | browser |
| R | random | \\ , Ctrl+Space | auto crossfade | hold Z | freeze | N / C / D / W | HUD / cues / mixer / FX |
| Shift + above | hard cut | J | mix mode | - / = | speed (Backspace 1×) | F10 | menu bar |
| [ ] | blend time | A | auto-pilot | ↑ ↓ | sensitivity | E | preset editor |
| L | lock | , . | interval | Q | render quality | V | live previews |
| 1…0 | fire cue | S | shuffle | F2–F8 | Kaleido, Mirror, Trails, RGB, Pixel, Invert, Strobe | / , Ctrl+F | search |
| Shift+1…0 | store cue | K | launch quantize | F9 | record MP4 | F / F11 | fullscreen |
| Alt+1…0 | clear cue | T | tap tempo | F12 | Spout | O | output window |
| Ctrl+D | favorite | Shift+T | Auto / Tap / Link | | | I / M | audio input / file |
| Ctrl+1…5 | rate | | | | | H / F1 | help |
| Ctrl+L | add to setlist | | | | | Ctrl+K | keyboard layout |

Settings, key map, MIDI maps, scenes, analysis cache and layout live in `%APPDATA%\Amethyst\`
(`settings.ini`, `library.ini`, `scenes.ini`, `analysis.ini`, `layout.ini`).

## Build

Needs MSYS2 at `C:\msys64` with UCRT64 packages: `gcc cmake ninja SDL2 glew miniaudio`. Then `.\build.ps1`.
Vendored in `third_party/`: prebuilt libprojectM 4.2 (see `BUILD-INFO.txt`), Dear ImGui, Spout2 SDK,
Ableton Link (+ asio), stb_image.

`Amethyst.exe --selftest <dir> [--add-folder <presets>]` runs a scripted session in a settings sandbox
and writes PNG captures and a log to `<dir>`. It includes a built-in fake Engine OS device (StageLinQ),
an Art-Net capture on localhost and a Spout loopback.

**Not yet tested against real hardware:** StageLinQ (built from the open-source protocol docs and verified
only against the fake device), the Launchpad / APC40 templates and their LED colours, and MIDI LED feedback.

## License

Amethyst is free software, licensed under the **GNU General Public License v3.0 or later** — see [LICENSE](LICENSE).

Bundled third-party code keeps its own license (texts ship in `licenses\` of the release): libprojectM
LGPL-2.1 (dynamically linked DLL), Ableton Link GPL-2.0-or-later, asio Boost Software License, Spout2 BSD-2,
Dear ImGui MIT, stb_image public domain / MIT, miniaudio public domain / MIT-0.
