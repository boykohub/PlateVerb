# BD Plugins

All plugins live on the `main` branch, one folder each. Each plugin has its own build on GitHub Actions
and only builds when its own files change (or the shared `Source/` code, which all of them use).

| Plugin | Folder | Download (Actions artifact) |
|---|---|---|
| PlateVerb | `Source/` | PlateVerb-VST3 |
| ChannelStrip | `ChannelStrip/Source/` | ChannelStrip-VST3 |
| 1081+ | `N1081/Source/` | 1081plus-VST3 |

`Source/Engine.h` (PlateVerb's engine and compressor) is shared: the 1081+ "Velvet" compressor and the
ChannelStrip compressor use it. To build one plugin by hand: Actions → "Build …" → Run workflow.

## Flexiverb

- **Presets:** use < and > to step through presets. Click the name to open the browser, which shows every preset by category (480L, Surround / HD Cart, Matched to the 480L impulse responses, 480L II, Your presets). **SAVE** stores the whole plugin as one of your presets.
- **Display:** shows the program(s), a status line and a graph. Right-click it to change the text size.
  - **Reverbs:** the graph shows decay time against frequency. Click to add a point, drag it to move it, double-click to remove it.
  - **Delay programs and the delay CONTROL pages:** the graph shows the delay taps. Drag a tap sideways to change its time, or up and down to change its level. The faders follow, and moving a fader moves the tap.
- **Keys:** MACHINE A / B, MACHINES (one, series, parallel, left/right), PAGE, CONTROL (setup, iron, utility delay), MUTE and SETUP. Keys light up while they are on or pressed.
- **Drawer** (tab on the right): ANALOG (the ChannelStrip preamp: drive, output, model, character, and whether it affects the reverb, the EQ or both), EQ (concentric knobs: frequency outside, gain inside) and COMPRESSOR.
- **SETUP:** colours and gradient, text sizes, oversampling (off, 2x, 4x, 6x, 8x for the analog, EQ and compressor chain) and precision (64-bit EQ). Plugins take 64-bit audio from the host.
- **Window:** drag the lower-right corner to scale it.

The 480L factory programs use the values from its manual. The Classic Cart and Surround/HD Cart programs are Flexiverb's own settings, because the manuals give none. "Matched" presets are fitted to impulse responses of a real 480L and carry a little tone shaping. "480L II" presets are the earlier Flexiverb's IR-matched presets, unchanged, level-matched to the "Matched" ones so you can compare them. The IRs themselves are not included.
With the 480L hardware switched on (CONTROL, Setup), the model follows the 480L schematics:

- 18-bit delay memory
- 16-bit gain-ranging A/D and 18-bit D/A
- 20 kHz 9-pole filters
- 7 Hz input coupling
- the deglitcher and its post-filters
