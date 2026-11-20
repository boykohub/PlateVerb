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

## Flexiverb (the LARC)

Flexiverb's window works like the 480L's LARC remote:

- **BANK**, then 1-0, picks a bank (press BANK again for banks 11-20, again for 21-26). Then 1-0 loads a program.
- **PROG**, then 1-0, loads a program from the current bank. **VAR**: 1 is the factory setting, 2-0 are your stored variations.
- **REG**: press a bank (1-0), then a number (1-0), to recall a register. **STO**, bank, number stores one. In VAR mode, STO stores a variation.
- **PAGE** steps through the six-slider pages. **MACH** switches between machines A and B. **MUTE** mutes the output.
- **CTRL** puts the setup on the sliders: configuration, 480L hardware, EQ, compressor and delay. Use PAGE or 1-8 to pick a page.
- Click the display for a program list. Right-click the panel to change the window size.

Banks 1-17 use the factory values from the 480L manual. Banks 18-21 (Classic Cart) and 22-26 (Surround/HD Cart) use
the cart program names, but the manuals print no values for them, so those settings are Flexiverb's own.
With the 480L hardware switched on (CTRL page 1, HW), the model follows the 480L schematics:

- 18-bit delay memory
- 16-bit gain-ranging A/D and 18-bit D/A
- 20 kHz 9-pole filters
- 7 Hz input coupling
- the deglitcher and its post-filters
