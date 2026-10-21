# samp_grass_restore

Brings the **grass and plants back in SA-MP**. A tiny `.asi` plugin, nothing else changes.

## Install

1. Download `GrassRestore.asi` from the [latest release](https://github.com/yessirks/samp_grass_restore/releases/latest).
2. Put it in your GTA San Andreas folder (next to `gta_sa.exe`). You need an ASI loader (it comes with CLEO, SilentPatch, Ultimate ASI Loader ...); if other `.asi` mods already work for you, you have one.
3. Start SA-MP. That's it.

To remove it, delete the file.

Optional: put `GrassRestore.ini` next to the `.asi` to change `Enabled` or turn on `Log=1` (writes `GrassRestore.log`, useful for bug reports).

## Why is there no grass in SA-MP?

`samp.dll` switches off the game's grass update (it overwrites the call to `CPlantMgr::Update` with `NOP`s), so the game never plants any grass even though the models and textures are all there. This plugin puts that one call back, and keeps checking in case SA-MP removes it again.

It is careful: it only touches that one call, only if it is exactly the `NOP`s SA-MP leaves, and only if the game is the stock **GTA:SA 1.0 US** `gta_sa.exe`. Anything else is left alone.

## Good to know

- Tested with SA-MP 0.3.7-R5 and the 1.0 US exe. Other versions are not tested.
- Grass costs some FPS (about 1 ms per frame in my test, with a heavy shader pack and a far grass distance). If a mod sets the grass draw distance, that is where to turn it down.
- The game grows and draws the grass itself, exactly like single-player, so grass mods (custom `plants.dat`, grass models ...) keep working.

## Build

Needs [Zig](https://ziglang.org/) (`winget install -e --id zig.zig`). Run `build.bat`: it builds `build\GrassRestore.asi` (32-bit) and copies it into the game folder next to this one if it finds `gta_sa.exe` there.

## License

MIT, see [LICENSE](LICENSE).
