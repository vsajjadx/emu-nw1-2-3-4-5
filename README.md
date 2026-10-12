# Emu Launcher

One Windows package with eight emulators inside and a launcher in front of them.
Add your games and the launcher works out which emulator to use, no setup needed.

| Emulator | Systems | Source (submodule) |
|---|---|---|
| shadPS4 | PlayStation 4 | `emulators/shadPS4` |
| Kyty | PlayStation 5 | `emulators/KytyPS5` |
| RPCS3 | PlayStation 3 | `emulators/rpcs3` |
| PCSX2 | PlayStation 2 | `emulators/pcsx2` |
| DuckStation | PlayStation 1 | `emulators/duckstation-ps1` |
| Dolphin | GameCube, Wii | `emulators/dolphin-wii` |
| Suyu | Nintendo Switch | `emulators/suyu-v0.0.4-nintendo` |
| Xenia | Xbox 360 | `emulators/xenia-xbox` |

## Getting it

GitHub Actions builds everything on Windows (**Actions → Build everything (Windows)**,
runs on every push to `main`, or press *Run workflow*). The result is the artifact
**EmuLauncher-windows**:

```
EmuLauncher/
  emu-launcher.exe
  BUILD-REPORT.txt        <- which emulators built and which failed
  emulators/
    shadPS4/  kyty/  rpcs3/  pcsx2/  duckstation/  dolphin/  suyu/  xenia/
```

Unzip it anywhere and run `emu-launcher.exe`. Push a tag (`v1.0`, ...) and the same
zip is attached to a GitHub release.

Every emulator is built by its own job, so one failing build doesn't block the others;
a failed one is simply missing from the package and listed in `BUILD-REPORT.txt`.

## Using it

* **Add games...** / **Add folder...**, or just drag files and folders onto the window.
  A folder is searched recursively; a PS3/PS4/PS5 game folder counts as one game.
* The launcher looks *inside* the file to decide the system, not only at the extension:
  * PS1 / PS2 / PS3 discs from the ISO 9660 contents (`SYSTEM.CNF` with `BOOT` vs `BOOT2`, `PS3_GAME`),
    including `.bin/.cue` with raw sectors
  * GameCube / Wii from the disc header, Xbox 360 from the XISO header
  * PS4 / PS5 folders from `sce_sys/param.sfo` (`CUSA...` vs `PPSA...`) and the `eboot.bin` header
  * Switch / Xbox 360 / Wii formats from their extensions (`.nsp .xci .nro`, `.xex`, `.wbfs .rvz .wad`)
* Double-click (or Enter) launches the game with the right emulator.
* Right-click → **Open with** to override the choice. The choice is remembered.
* Compressed images (`.chd` could be PS1 or PS2) can't be inspected, so the launcher asks once.

The game list is stored in `%APPDATA%/EmuLauncher/emu-launcher/library.json`.
Your games are never copied, only referenced.

## Developing

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release     # detector unit tests (no Qt needed)
```

Without Qt 6 only the detector and its tests are built; with Qt 6 you also get `emu-launcher`.

Layout:

* `launcher/Detector.*` system detection, plain C++ (tested in `tests/detector_test.cpp`)
* `launcher/emulators.json` emulator list: folder, executable name, systems, command line
* `launcher/MainWindow.*`, `Library.*`, `EmulatorRegistry.*` the Qt UI
* `.github/workflows/build.yml` builds and packs everything

To add or change an emulator, edit `emulators.json` (and add its build job to the workflow).

## Licensing

The launcher is separate from the emulators. Each emulator keeps its own license
(mostly GPL) and is only referenced as a submodule, never copied into this repo.
Games and BIOS/firmware/keys are not included, you provide your own.
