#include "Detector.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

namespace fs = std::filesystem;

const char *systemId(System s)
{
    switch (s) {
    case System::PS1: return "ps1";
    case System::PS2: return "ps2";
    case System::PS3: return "ps3";
    case System::PS4: return "ps4";
    case System::PS5: return "ps5";
    case System::GameCube: return "gamecube";
    case System::Wii: return "wii";
    case System::Switch: return "switch";
    case System::Xbox360: return "xbox360";
    }
    return "";
}

const char *systemName(System s)
{
    switch (s) {
    case System::PS1: return "PlayStation 1";
    case System::PS2: return "PlayStation 2";
    case System::PS3: return "PlayStation 3";
    case System::PS4: return "PlayStation 4";
    case System::PS5: return "PlayStation 5";
    case System::GameCube: return "GameCube";
    case System::Wii: return "Wii";
    case System::Switch: return "Nintendo Switch";
    case System::Xbox360: return "Xbox 360";
    }
    return "";
}

namespace {

std::string lower(std::string s)
{
    for (char &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string upper(std::string s)
{
    for (char &c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

uint32_t le32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t le16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

uint32_t be32(const uint8_t *p)
{
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
         | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

bool readAt(std::ifstream &f, uint64_t off, void *buf, size_t n)
{
    f.clear();
    f.seekg(static_cast<std::streamoff>(off), std::ios::beg);
    if (!f)
        return false;
    f.read(static_cast<char *>(buf), static_cast<std::streamsize>(n));
    return f.gcount() == static_cast<std::streamsize>(n);
}

std::string trimNul(const std::string &s)
{
    std::string out = s.substr(0, s.find('\0'));
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back())))
        out.pop_back();
    return out;
}

// ---------------------------------------------------------------- ISO 9660

// Handles plain .iso (2048-byte sectors) and raw .bin (2352-byte sectors, mode 1 or 2).
struct Image {
    std::ifstream f;
    uint32_t stride = 2048;
    uint32_t skip = 0;

    bool open(const std::string &path)
    {
        f.open(fs::path(path), std::ios::binary);
        if (!f)
            return false;
        static const uint32_t layouts[][2] = {{2048, 0}, {2352, 16}, {2352, 24}};
        for (const auto &l : layouts) {
            uint8_t b[6];
            if (readAt(f, 16ull * l[0] + l[1], b, 6) && b[0] == 1 && std::memcmp(b + 1, "CD001", 5) == 0) {
                stride = l[0];
                skip = l[1];
                return true;
            }
        }
        return false;
    }

    bool read(uint32_t lba, uint32_t bytes, std::vector<uint8_t> &out)
    {
        out.assign(bytes, 0);
        for (uint32_t done = 0; done < bytes; done += 2048) {
            const uint32_t n = std::min<uint32_t>(2048, bytes - done);
            const uint64_t off = static_cast<uint64_t>(lba + done / 2048) * stride + skip;
            if (!readAt(f, off, out.data() + done, n))
                return false;
        }
        return true;
    }
};

struct Entry {
    std::string name; // lower-case, ";1" version suffix removed
    uint32_t lba = 0;
    uint32_t size = 0;
    bool dir = false;
};

std::vector<Entry> listDir(Image &img, uint32_t lba, uint32_t size)
{
    std::vector<Entry> out;
    size = std::min<uint32_t>(size, 1u << 20);
    size = (size + 2047) / 2048 * 2048;
    std::vector<uint8_t> buf;
    if (size == 0 || !img.read(lba, size, buf))
        return out;

    size_t pos = 0;
    while (pos + 34 <= buf.size()) {
        const uint8_t len = buf[pos];
        if (len == 0) { // padding up to the next sector
            pos = (pos / 2048 + 1) * 2048;
            continue;
        }
        if (pos + len > buf.size())
            break;
        const uint8_t nlen = buf[pos + 32];
        if (33u + nlen <= len) {
            Entry e;
            e.lba = le32(&buf[pos + 2]);
            e.size = le32(&buf[pos + 10]);
            e.dir = (buf[pos + 25] & 2) != 0;
            e.name.assign(reinterpret_cast<const char *>(&buf[pos + 33]), nlen);
            const bool dotEntry = nlen == 1 && (e.name[0] == '\0' || e.name[0] == '\1');
            if (!dotEntry) {
                const auto semi = e.name.find(';');
                if (semi != std::string::npos)
                    e.name.resize(semi);
                e.name = lower(e.name);
                out.push_back(e);
            }
        }
        pos += len;
    }
    return out;
}

const Entry *findEntry(const std::vector<Entry> &entries, const char *lowerName)
{
    for (const auto &e : entries)
        if (e.name == lowerName)
            return &e;
    return nullptr;
}

bool detectDisc(const std::string &path, Detection &d)
{
    {
        std::ifstream f(fs::path(path), std::ios::binary);
        if (!f)
            return false;

        // GameCube / Wii disc header: magic words at 0x18 (Wii) and 0x1C (GameCube).
        uint8_t hdr[0x60];
        if (readAt(f, 0, hdr, sizeof hdr)) {
            const std::string title = trimNul(std::string(reinterpret_cast<char *>(hdr) + 0x20, 0x40));
            if (be32(hdr + 0x18) == 0x5D1C9EA3u) {
                d.candidates = {System::Wii};
                if (!title.empty()) d.title = title;
                return true;
            }
            if (be32(hdr + 0x1C) == 0xC2339F3Du) {
                d.candidates = {System::GameCube};
                if (!title.empty()) d.title = title;
                return true;
            }
        }

        // Xbox 360 XISO: volume descriptor magic 0x10000 into the game partition,
        // whose start depends on the disc generation (XGD1/2/3) or a trimmed image.
        static const uint64_t bases[] = {0ull, 0xFD90000ull, 0x2090000ull, 0x18300000ull};
        char magic[20];
        for (uint64_t base : bases) {
            if (readAt(f, base + 0x10000, magic, sizeof magic) && std::memcmp(magic, "MICROSOFT*XBOX*MEDIA", 20) == 0) {
                d.candidates = {System::Xbox360};
                return true;
            }
        }
    }

    Image img;
    if (!img.open(path))
        return false;
    std::vector<uint8_t> pvd;
    if (!img.read(16, 2048, pvd))
        return false;
    const auto root = listDir(img, le32(&pvd[158]), le32(&pvd[166]));

    if (const Entry *cnf = findEntry(root, "system.cnf")) {
        std::vector<uint8_t> data;
        if (img.read(cnf->lba, std::min<uint32_t>(cnf->size, 2048), data)) {
            const std::string text = upper(std::string(data.begin(), data.end()));
            if (text.find("BOOT2") != std::string::npos) {
                d.candidates = {System::PS2};
                return true;
            }
            if (text.find("BOOT") != std::string::npos) {
                d.candidates = {System::PS1};
                return true;
            }
        }
    }
    if (findEntry(root, "ps3_game") || findEntry(root, "ps3_disc.sfb")) {
        d.candidates = {System::PS3};
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- PlayStation SFO / SELF

std::map<std::string, std::string> parseSfo(const fs::path &path)
{
    std::map<std::string, std::string> out;
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return out;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (b.size() < 20 || std::memcmp(b.data(), "\0PSF", 4) != 0)
        return out;
    const uint32_t keyTable = le32(&b[8]);
    const uint32_t dataTable = le32(&b[12]);
    const uint32_t count = std::min<uint32_t>(le32(&b[16]), 512);
    for (uint32_t i = 0; i < count; ++i) {
        const size_t e = 20 + static_cast<size_t>(i) * 16;
        if (e + 16 > b.size())
            break;
        const uint32_t keyPos = keyTable + le16(&b[e]);
        const uint32_t len = le32(&b[e + 4]);
        const uint32_t dataPos = dataTable + le32(&b[e + 12]);
        if (keyPos >= b.size() || dataPos + len > b.size())
            continue;
        const std::string key(reinterpret_cast<const char *>(&b[keyPos]));
        out[key] = trimNul(std::string(reinterpret_cast<const char *>(&b[dataPos]), len));
    }
    return out;
}

bool selfSystem(const fs::path &path, System &out)
{
    std::ifstream f(path, std::ios::binary);
    uint8_t m[4];
    if (!f || !readAt(f, 0, m, 4))
        return false;
    if (m[0] == 0x4F && m[1] == 0x15 && m[2] == 0x3D && m[3] == 0x1D) { out = System::PS4; return true; }
    if (m[0] == 0x54 && m[1] == 0x14 && m[2] == 0xF5 && m[3] == 0xEE) { out = System::PS5; return true; }
    if (std::memcmp(m, "SCE\0", 4) == 0) { out = System::PS3; return true; }
    return false;
}

// Case-insensitive child lookup (dumps come in every capitalisation).
bool findChild(const fs::path &dir, const std::string &name, fs::path &found)
{
    std::error_code ec;
    for (const auto &e : fs::directory_iterator(dir, ec)) {
        if (lower(e.path().filename().string()) == name) {
            found = e.path();
            return true;
        }
    }
    return false;
}

bool detectFolder(const fs::path &dir, Detection &d)
{
    fs::path eboot, sce, ps3game, param, usrdir;

    // PS4 / PS5: <game>/eboot.bin + <game>/sce_sys/param.sfo
    if (findChild(dir, "eboot.bin", eboot) && findChild(dir, "sce_sys", sce)) {
        System sys = System::PS4;
        fs::path sfo;
        if (findChild(sce, "param.sfo", sfo)) {
            const auto f = parseSfo(sfo);
            const auto id = f.find("TITLE_ID");
            if (id != f.end() && id->second.rfind("PPSA", 0) == 0)
                sys = System::PS5;
            const auto t = f.find("TITLE");
            if (t != f.end() && !t->second.empty())
                d.title = t->second;
        } else {
            System s;
            if (selfSystem(eboot, s) && (s == System::PS4 || s == System::PS5))
                sys = s;
        }
        d.candidates = {sys};
        d.launchPath = eboot.string();
        return true;
    }

    // PS3: <game>/PS3_GAME/PARAM.SFO, or the PS3_GAME folder itself (PARAM.SFO + USRDIR)
    fs::path root = dir;
    bool ps3 = false;
    if (findChild(dir, "ps3_game", ps3game)) {
        root = dir;
        ps3 = true;
        findChild(ps3game, "param.sfo", param);
    } else if (findChild(dir, "param.sfo", param) && findChild(dir, "usrdir", usrdir)) {
        root = dir;
        ps3 = true;
    }
    if (ps3) {
        if (!param.empty()) {
            const auto f = parseSfo(param);
            const auto t = f.find("TITLE");
            if (t != f.end() && !t->second.empty())
                d.title = t->second;
        }
        d.candidates = {System::PS3};
        d.launchPath = root.string();
        return true;
    }
    return false;
}

// First FILE entry of a .cue sheet, resolved relative to the sheet.
std::string cueFirstFile(const fs::path &cue)
{
    std::ifstream f(cue);
    std::string line;
    while (std::getline(f, line)) {
        const auto p = upper(line).find("FILE");
        if (p == std::string::npos)
            continue;
        size_t i = p + 4;
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
            ++i;
        std::string name;
        if (i < line.size() && line[i] == '"') {
            const auto end = line.find('"', i + 1);
            name = line.substr(i + 1, end == std::string::npos ? std::string::npos : end - i - 1);
        } else {
            const auto end = line.find_first_of(" \t\r", i);
            name = line.substr(i, end == std::string::npos ? std::string::npos : end - i);
        }
        if (!name.empty())
            return (cue.parent_path() / name).string();
    }
    return {};
}

const char *const kExtensions[] = {
    ".nsp", ".xci", ".nro", ".nso", ".nca", ".xex", ".xcp", ".wbfs", ".wad", ".gcm", ".gcz", ".tgc",
    ".rvz", ".dol", ".cue", ".pbp", ".m3u", ".iso", ".bin", ".img", ".chd", ".cso", ".zso", ".gz",
    ".self", ".exe",
};

} // namespace

bool isKnownGameExtension(const std::string &path)
{
    const std::string ext = lower(fs::path(path).extension().string());
    for (const char *e : kExtensions)
        if (ext == e)
            return true;
    return false;
}

Detection detectGame(const std::string &pathStr)
{
    Detection d;
    const fs::path p(pathStr);
    d.title = p.stem().string();
    if (d.title.empty())
        d.title = p.filename().string();

    std::error_code ec;
    if (fs::is_directory(p, ec)) {
        d.title = p.filename().string();
        detectFolder(p, d);
        return d;
    }

    const std::string ext = lower(p.extension().string());
    const std::string name = lower(p.filename().string());

    if (ext == ".nsp" || ext == ".xci" || ext == ".nro" || ext == ".nso" || ext == ".nca") {
        d.candidates = {System::Switch};
    } else if (ext == ".xex" || ext == ".xcp") {
        d.candidates = {System::Xbox360};
    } else if (ext == ".wbfs" || ext == ".wad") {
        d.candidates = {System::Wii};
    } else if (ext == ".gcm" || ext == ".gcz" || ext == ".tgc") {
        d.candidates = {System::GameCube};
    } else if (ext == ".rvz" || ext == ".dol") {
        d.candidates = {System::GameCube, System::Wii}; // same emulator either way
    } else if (ext == ".pbp" || ext == ".m3u") {
        d.candidates = {System::PS1};
    } else if (ext == ".cue") {
        const std::string bin = cueFirstFile(p);
        std::error_code e2;
        if (!bin.empty() && fs::exists(bin, e2) && detectDisc(bin, d)) {
            // detected from the image behind the sheet
        } else {
            d.candidates = {System::PS1};
        }
    } else if (ext == ".chd") {
        d.candidates = {System::PS1, System::PS2}; // compressed: contents can't be sniffed
    } else if (ext == ".cso" || ext == ".zso" || ext == ".gz") {
        d.candidates = {System::PS2};
    } else if (ext == ".bin" && name == "eboot.bin") {
        if (!detectFolder(p.parent_path(), d)) {
            System s;
            if (selfSystem(p, s))
                d.candidates = {s};
        }
        d.launchPath = p.string();
        if (d.title == "eboot")
            d.title = p.parent_path().filename().string();
    } else if (ext == ".iso" || ext == ".bin" || ext == ".img") {
        detectDisc(pathStr, d);
    } else if (ext == ".self") {
        System s;
        if (selfSystem(p, s))
            d.candidates = {s};
    } else if (ext == ".exe") {
        std::ifstream f(p, std::ios::binary);
        char m[8];
        if (f && readAt(f, 0, m, 8) && std::memcmp(m, "PS-X EXE", 8) == 0)
            d.candidates = {System::PS1};
    }
    return d;
}
