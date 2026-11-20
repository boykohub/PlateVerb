# Boyko plugins

All plugins live on the `main` branch, one folder each. Each plugin has its own build on GitHub Actions
and only builds when its own files change (or the shared `Source/` code, which all of them use).

| Plugin | Folder | Download (Actions artifact) |
|---|---|---|
| PlateVerb | `Source/` | PlateVerb-VST3 |
| ChannelStrip | `ChannelStrip/Source/` | ChannelStrip-VST3 |
| 1081+ | `N1081/Source/` | 1081plus-VST3 |

`Source/Engine.h` (PlateVerb's engine and compressor) is shared: the 1081+ "Velvet" compressor and the
ChannelStrip compressor use it. To build one plugin by hand: Actions → "Build …" → Run workflow.
