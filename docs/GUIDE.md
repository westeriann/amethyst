# Amethyst user guide

A practical guide to running visuals with Amethyst. Every action can be put on a key (Ctrl+K), a MIDI
control, OSC or the phone remote (all three under Control → MIDI, OSC & phone remote). The keys below are the defaults.

- [Getting started](#getting-started)
- [Presets and the browser](#presets-and-the-browser)
- [Decks, crossfader and transitions](#decks-crossfader-and-transitions)
- [Auto-pilot](#auto-pilot)
- [Auto FX](#auto-fx)
- [Effects and custom shaders](#effects-and-custom-shaders)
- [The visual preset editor](#the-visual-preset-editor)
- [Render an audio file to video](#render-an-audio-file-to-video)
- [Going live: outputs, recording, Spout](#going-live-outputs-recording-spout)
- [Show safety](#show-safety)
- [Tempo, DJ gear and remote control](#tempo-dj-gear-and-remote-control)
- [Troubleshooting](#troubleshooting)

## Getting started

1. Start Amethyst. It listens to whatever your PC is playing (system audio). To use a microphone or line-in,
   press **I**; to play a music file through Amethyst, press **M**.
2. Your presets are in `Documents\Amethyst\Presets`. Drop more `.milk` files or folders onto the window to add them.
3. Press **Space** for the next preset, **R** for a random one, **A** to let auto-pilot take over.
4. Press **U** to hide the interface, **F** for fullscreen, **H** to see every shortcut.

## Presets and the browser

- **Click** a preset to blend to it on the focused deck; **Shift+click** cuts straight to it; **Ctrl+click** loads it on deck B.
- **Search** with **/**. The two drop-downs at the top narrow the list:
  - **Pool**: All, Favourites, ratings, tags, setlists, colours, Bright/Dark, **Intense/Calm** (how hard a
    preset hits on kick drums), or "More like this".
  - **Folder**: one of your preset folders.
- **Ctrl+D** favourites, **Ctrl+1…5** rates, **Ctrl+L** adds to the active setlist.
- Right-click a preset for more: load to deck A/B, rate, add to a setlist, store as a cue, **Use as safe preset**.
- **Cues 1…0**: Shift+number stores the current preset, the number fires it.
- **Analysis**: Amethyst studies every preset in the background (colour, brightness, motion, bass punch). The
  progress bar in the browser shows what is left. It pauses while you perform; **Analyse now** runs it anyway.

## Decks, crossfader and transitions

There are two decks, **A** and **B**. **Tab** switches which one your keys control.

- **Ctrl+← / →** moves the crossfader, **\\** fades automatically to the other side.
- **J** cycles mix modes (Crossfade, Add, Screen, Multiply, Difference, Lighten, Luma key).
- **[ ]** set the blend time between presets on a deck; the Mixer (**D**) also chooses transition styles
  (wipe, iris, zoom, glitch, dissolve, luma, slide).
- **Scenes** save a whole look (both presets, mixer, effects, overlays): Ctrl+Shift+F1…F8 stores, Shift+F1…F8
  recalls and morphs over a number of beats. The **timeline** plays scenes for a set number of bars (Ctrl+T).

## Auto-pilot

Press **A** to turn it on. **Right-click the AUTO button** in the status bar for every setting:

| Setting | What it does |
|---|---|
| **Stay in folder** | Only pick presets from this folder and its subfolders. The pool, folder and search filters at the top of the browser narrow it further. |
| Shuffle | Random order, or the order of the list. |
| Mix decks A/B | Load the next preset on the hidden deck and crossfade, instead of blending on one deck. |
| Change every | The longest it stays on one preset in quiet, steady parts. |
| **Smart mode** | Follow the song (below). Off = switch on a timer, launched on the beat or bar. |
| New preset on the real drop after a fakeout | See fakeouts below. |
| Smart blend modes on drops | Drops slam both presets together for a bar before the old one melts away. |
| Build-up tease | How far the drop preset glows in during a build-up (0 = off). |
| Calmer while vocals | Softer effects, no phrase flashes, and no switch in the middle of a sung line. |

**Smart mode** listens to the music:

- It finds the **beat and bars**, and changes preset on **8-bar phrases**. The blend starts early so the new
  preset fully lands on the phrase's first downbeat.
- **Drops** get a hard cut (or a blend-mode smash) to a preset that hits hard on kicks, prepared ahead of time
  so it lands exactly on the drop.
- **Build-ups** hold the current preset while the drop preset slowly glows in underneath.
- **Breakdowns** get a calm preset with a long, soft blend.
- **Fakeouts** (a drop that cuts out after a bar or a few, then drops for real) are recognised: the real drop
  gets its own fresh preset.
- **Song start, end and change** are detected from silence, a lasting tempo change, or the track title from
  your DJ software. Between songs it fades down, and the first beat of the new song brings a new preset.

The status bar shows what it hears: `AUTO smart  Drop + vox  bar 3/8 +FX`.

## Auto FX

When auto-pilot runs, Auto FX drives the effects from the same song detection: a flash, zoom punch, RGB burst
and a one-bar strobe on drops; zoom, swirl and a speeding-up strobe through build-ups; a dark, colourless pause
in fakeout gaps; soft trails in breakdowns; a new effect look every phrase; fades between songs.

- **Strength** sets how hard it pushes. **Flashes & strobes** off removes every flash, strobe and colour flip.
- It adds on top of your own effects; it never turns your effects off.
- With **Use custom shaders** on, it also picks your shader effects on drops and some phrases.

## Effects and custom shaders

The Effects window (**W**) has kaleidoscope, mirror, trails, RGB split, pixelate, invert, strobe, zoom, rotate,
swirl, hue, saturation, contrast and brightness. **F2–F8** toggle the main ones. The **modulation** section
lets the music move any of them (bass, mids, treble, beat, LFOs, vocals, song energy).

**Custom shaders** are your own effects, written like ShaderToy:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec3 c = texture(iChannel0, uv).rgb;          // the picture so far
    c *= 1.0 + 0.5 * iBass * (1.0 - iBeatPhase);  // pump with the kick
    fragColor = vec4(c, 1.0);
}
```

Save `.glsl` files in `Documents\Amethyst\Shaders` (Effects → Custom shaders → **Folder**). Pick one in the
**Shader** list; **Amount** blends it with the picture. Saving the file reloads it straight away; if it has an
error, the error is shown and the last working version keeps running.

| Input | Meaning |
|---|---|
| `iChannel0` | The picture before your effect |
| `iChannel1` | Your effect's previous frame (for trails and feedback) |
| `iResolution`, `iTime`, `iFrame` | Size in pixels, seconds, frame number |
| `iBeat`, `iBeatPhase` | Beat count, and 0…1 position inside the current beat |
| `iBass`, `iMid`, `iTreble`, `iLevel` | Band levels, 0…1 |
| `iDrop` | 1 on a drop, fading out over a couple of beats |
| `iVocal` | 0…1, how much singing is detected |

Six examples come with Amethyst (in the `shaders` folder next to `Amethyst.exe`): ripple, chroma-quake,
kaleido-tunnel, vhs, bloom-glow, edge-neon. Copy one into your folder to start.

## The visual preset editor

Press **E** to edit the preset on the focused deck.

- **Make it dance**: tick recipes like *Pulse with the bass* or *Rainbow waveform* to make a preset react to music.
- **Motion, Trails & look, Waveform, Borders & grid, Shapes, Extra waves**: plain-English sliders, switches
  and colour pickers. Hover for an explanation, right-click to reset.
- Changes show live. **Undo/Redo** (Ctrl+Z / Ctrl+Y), **Revert**, **Save as** (Ctrl+S) to keep your version.
- **Code (advanced)** shows the raw `.milk` text.

## Render an audio file to video

**File → Render audio file to video…** turns a WAV, FLAC or MP3 into an MP4 with the music in it.

- Choose the size (720p to 4K) and 30 or 60 fps. If this PC's encoder can't do that, Amethyst steps down and tells you.
- **Let smart auto-mix + Auto FX run the show** renders a full show; untick it to render exactly your current
  setup (presets, effects, scenes).
- It renders offline: no frame is ever dropped, the beat grid is right from the first frame, and it is often
  faster than real time. Videos go to `Videos\Amethyst`.
- The video / camera / Spout layer plays in real time, so it is left out of file renders.

## Going live: outputs, recording, Spout

- **O** opens an output window on another screen (projector, LED wall). Drag its corners to fit a surface (corner pin).
- **F9** records the output to MP4 (`Videos\Amethyst`). **F12** sends it to OBS, Resolume or TouchDesigner via Spout.
- **Q** switches render quality if a computer struggles.
- Overlays (logo, text, video file, camera, Spout input) are in the Overlays window.
- Art-Net DMX output can colour lights from the picture (Output window, or the "Art-Net lighting" action).

## Show safety

- The **watchdog** watches the output while music plays. A preset that stays dark for 8 seconds is skipped; one that
  **freezes** is skipped and marked broken; one that drags the frame rate below about 14 fps is skipped (and
  marked broken the second time). If the safe preset does not fix a frozen picture, Amethyst restarts itself.
- **Safe preset now** (Performance actions, or the auto-pilot menu) is the panic button: it puts the safe preset on
  air and resets the effects. Choose your safe preset with right-click → **Use as safe preset**.
- If Amethyst ever crashes, it restarts by itself and marks the preset that was loading as broken.
  **Hide broken** in the browser hides broken presets.

## Tempo, DJ gear and remote control

- The tempo is detected automatically. **T** taps it, **Shift+T** switches Auto / Tap / Ableton Link.
- **Denon Engine OS** gear (StageLinQ) gives the exact BPM, beat and track title. Serato, rekordbox and Traktor
  can share their tempo through **Ableton Link**.
- **MIDI**: Control → MIDI, OSC & phone remote → learn any action; templates for Launchpad and APC40, with LED feedback.
- **Phone remote**: Control → MIDI, OSC & phone remote → Phone remote tab: turn it on (port 8080) and open the address it shows on your phone.
- **OSC**: `/amethyst/<action> [value]` on UDP port 9000.

## Troubleshooting

| Problem | Try |
|---|---|
| Nothing reacts to the music | Press **I** and pick the right audio device; check the level meter under Now playing. |
| The tempo is half or double | Tap it (**T**) a few times, or use Link / DJ gear. |
| It is slow | **Q** for lower quality; turn off **Live previews** (**V**); close other GPU-heavy apps. |
| A preset looks broken | Right-click it to check; **Hide broken** hides the ones the app has flagged. |
| Auto-pilot picks presets you don't want | Right-click AUTO → **Stay in folder**, or narrow the browser's pool/folder. |
| Flashes are too much | Right-click AUTO → untick **Flashes & strobes**. |

Settings and your library data live in `%APPDATA%\Amethyst`.
