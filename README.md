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

- **Presets:** click the preset menu to see every preset by category. Use < > beside it to step through them, and SAVE to store your own. PANEL opens the drawer.
- **Screen, REVERB page:**
  - the early reflections and the low, mid and high decay of the program as it is set right now, rendered in the background from the engine itself, with live output bars;
  - the decay-time curve (click to add a point, drag it to move it, double-click to remove it);
  - every reverb value as a number.

  Drag the predelay line and the decay handle on the graph.
- **Screen, DELAY page:** the delay taps (drag them sideways for time, up and down for level) and every delay value.
- **Numbers on the screen:** right-click and drag up or down, or click and type a value.
- **Keys:** REVERB / DELAY (the page), HALL, RANDOM HALL, ROOM, CHAMBER, PLATE, AMBIENCE, INVERSE, DELAY, CHORUS, PITCH.
- **Faders:** one per parameter of the page, with names and values above them. A page with more than 16 parameters uses two rows.
- **Drawer** (a third of the plugin's width):
  - SYSTEM: machines, mute, SETUP, 480L hardware, input, output, transformers, iron and output level;
  - ANALOG;
  - EQ (analog models from ChannelStrip, plus colour);
  - COMPRESSOR.
- **SETUP:** opens beside the plugin. It holds colours, gradient, 3D (flat, soft, deep), text sizes for buttons, knobs, screen and faders, oversampling and precision.
- **Shape and Spread:** in the 480-style reverbs they shape the reverb's build-up, its swell, the way the 480L does. The constants are fitted to impulse responses of a real 480L across 28 factory programs at once.

The 480L factory programs use the values from its manual. The Classic Cart and Surround/HD Cart programs are Flexiverb's own settings, because the manuals give none. "Matched" presets are fitted to impulse responses of a real 480L and carry a little tone shaping. "480L II" presets are the earlier Flexiverb's IR-matched presets, unchanged, level-matched to the "Matched" ones so you can compare them. The IRs themselves are not included.
With the 480L hardware switched on (CONTROL, Setup), the model follows the 480L schematics:

- 18-bit delay memory
- 16-bit gain-ranging A/D and 18-bit D/A
- 20 kHz 9-pole filters
- 7 Hz input coupling
- the deglitcher and its post-filters
