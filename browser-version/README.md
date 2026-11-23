# Amethyst

Keyboard-driven VJ software that plays **MilkDrop presets** (`.milk`) live, reacting to your music.

## Start

Double-click **`Start Amethyst.bat`**. It starts a tiny local server and opens the app in a
Chrome/Edge app window. Keep the black console window open while you play.

Pick an audio source on the start screen: microphone/line-in, system or tab audio, an audio file, or visuals only.

## Load your presets

* **Drag & drop** `.milk` files — or a whole folder of them — anywhere on the window.
* Or press **Ctrl+O** (files) / **Ctrl+Shift+O** (folder).
* Imported presets are remembered between sessions. Conversion runs in the background;
  presets that can't be converted are skipped automatically (shown struck through in the browser).
* Drop texture images (`.jpg`/`.png`) that presets reference alongside them.
* Already-converted Butterchurn `.json` presets work too.
* 367 built-in presets are included.

## Keys

| Presets | | Auto-pilot | |
|---|---|---|---|
| `Space` / `→` | next preset | `A` | auto-pilot on/off |
| `←` | previous (history) | `,` `.` | interval −/+ |
| `R` | random | `K` | switch on the beat |
| `Shift` + above | hard cut | `S` | shuffle / in order |
| `[` `]` | blend time −/+ | | |
| `L` | lock preset | **Cues** | |
| `T` | show preset name | `1`…`0` | fire cue |
| | | `Shift`+`1`…`0` | store current preset |
| | | `Alt`+`1`…`0` | clear cue |

| Performance | | View & files | |
|---|---|---|---|
| `B` | blackout | `F` | fullscreen |
| `X` | white flash | `O` | output window (projector) |
| hold `Z` | freeze | `U` | hide/show interface |
| `-` `=` | speed −/+ (`Backspace` = 1×) | `P` / `Tab` | preset browser |
| `↑` `↓` | audio sensitivity | `/` | search (then `↑↓` `Enter`) |
| `Q` | render quality | `G` | pool: all / imported / built-in |
| | | `I` | audio input menu |
| | | `M` / `Shift+M` | play audio file / pause |
| | | `H` or `?` | all shortcuts |

Searching narrows the pool: type `/ flexi`, press `Esc`, and `Space` now steps through only matching presets
(press `Esc` again to clear).

## Projector setup

Press **O** to open the output window, drag it to the projector, press **F** in it for fullscreen.
Keep driving from the control window (or press keys in the output window — they work there too).
Keep the control window visible, not minimised.

## Notes

* Needs a browser with WebGL 2 (current Chrome or Edge).
* Rendering is done by Butterchurn, an open-source WebGL implementation of MilkDrop. A few presets use
  shader features it doesn't support; those fall back to MilkDrop's default shader instead of going black.
* `shader-fix.js` works around a bug in the in-browser `.milk` converter (unparenthesised
  expressions like `a*b + c` were mistranslated), so imported shaders render correctly.
* `VJ.stats()` in the browser console shows FPS and library info.
