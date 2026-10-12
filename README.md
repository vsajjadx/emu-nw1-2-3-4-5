# Emu Launcher

A Qt6/C++ front-end that brings eight emulator forks together in one place.

| Emulator | System | Submodule |
|---|---|---|
| shadPS4 | PlayStation 4 | `emulators/shadPS4` |
| Kyty | PlayStation 5 | `emulators/KytyPS5` |
| RPCS3 | PlayStation 3 | `emulators/rpcs3` |
| PCSX2 | PlayStation 2 | `emulators/pcsx2` |
| DuckStation | PlayStation 1 | `emulators/duckstation-ps1` |
| Dolphin | GameCube / Wii | `emulators/dolphin-wii` |
| Suyu | Nintendo Switch | `emulators/suyu-v0.0.4-nintendo` |
| Xenia | Xbox 360 | `emulators/xenia-xbox` |

The emulators are git submodules pointing at your forks, so this repo stays small.
Each one is downloaded only when you press **Build** in the launcher.

## Build the launcher

```sh
git clone https://github.com/<you>/emu-launcher.git   # no --recursive needed
cd emu-launcher
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/emu-launcher
```

Requires Qt 6 (Widgets), CMake 3.21+ and a C++20 compiler.

## Using it

1. Pick an emulator on the left.
2. **Build** fetches its submodule and compiles it into `build-emulators/<id>`.
   Each emulator has its own dependencies (see its own README); install them first.
   If you already have a built copy, use **Set executable...** instead.
3. **Set game folder...** scans that folder for files with the extensions the emulator supports.
4. Double-click a game, or use **Launch selected game**.

## Adding another emulator

Add the submodule (`git submodule add <url> emulators/<name>`) and a new entry in
`launcher/emulators.json` with its extensions, executable candidates and build steps.

## Licensing

The launcher code here is separate from the emulators. Each emulator keeps its own
license (mostly GPL), and is only linked as a submodule, never copied into this repo.
