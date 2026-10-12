#pragma once

// Game/system detection. Pure standard C++ (no Qt) so it can be unit-tested anywhere.

#include <string>
#include <vector>

enum class System { PS1, PS2, PS3, PS4, PS5, GameCube, Wii, Switch, Xbox360 };

const char *systemId(System s);   // "ps1", "ps2", "ps3", "ps4", "ps5", "gamecube", "wii", "switch", "xbox360"
const char *systemName(System s); // "PlayStation 1", ...

struct Detection {
    // Most likely system first. Empty = not recognised.
    // Several entries = the file really is ambiguous (e.g. a .chd could be PS1 or PS2).
    std::vector<System> candidates;
    std::string title;      // best-effort display title
    std::string launchPath; // what to hand to the emulator; empty = the path that was scanned
};

// `path` may be a file or a game folder (PS3/PS4/PS5 style dumps).
Detection detectGame(const std::string &path);

// True for file extensions worth looking at when scanning a folder.
bool isKnownGameExtension(const std::string &path);
