// Builds tiny synthetic disc images / game folders and checks what Detector makes of them.

#include "../launcher/Detector.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

static void put32(std::vector<uint8_t> &b, size_t off, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        b[off + i] = (v >> (8 * i)) & 0xFF;
}

static void write(const fs::path &p, const std::vector<uint8_t> &data)
{
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
}

static void writeText(const fs::path &p, const std::string &s)
{
    write(p, std::vector<uint8_t>(s.begin(), s.end()));
}

struct IsoFile {
    std::string name; // e.g. "SYSTEM.CNF;1"
    std::string content;
    bool dir = false;
};

// Minimal ISO 9660: PVD at sector 16, root directory at sector 18, files after that.
// stride/skip let us fake raw 2352-byte "BIN" sectors.
static std::vector<uint8_t> makeIso(const std::vector<IsoFile> &files, uint32_t stride = 2048, uint32_t skip = 0)
{
    const uint32_t sectors = 40;
    std::vector<uint8_t> img(static_cast<size_t>(sectors) * stride, 0);
    auto sector = [&](uint32_t n) { return static_cast<size_t>(n) * stride + skip; };

    // directory records
    std::vector<uint8_t> dir(2048, 0);
    size_t pos = 0;
    auto addRecord = [&](const std::string &name, uint32_t lba, uint32_t size, bool isDir) {
        const size_t nlen = name.size();
        size_t len = 33 + nlen;
        if (len % 2)
            ++len;
        dir[pos] = static_cast<uint8_t>(len);
        put32(dir, pos + 2, lba);
        put32(dir, pos + 10, size);
        dir[pos + 25] = isDir ? 2 : 0;
        dir[pos + 32] = static_cast<uint8_t>(nlen);
        std::memcpy(&dir[pos + 33], name.data(), nlen);
        pos += len;
    };
    addRecord(std::string(1, '\0'), 18, 2048, true);
    addRecord(std::string(1, '\1'), 18, 2048, true);
    uint32_t next = 19;
    for (const auto &f : files) {
        addRecord(f.name, next, static_cast<uint32_t>(f.content.size()), f.dir);
        if (!f.content.empty())
            std::memcpy(&img[sector(next)], f.content.data(), f.content.size());
        ++next;
    }
    std::memcpy(&img[sector(18)], dir.data(), dir.size());

    // primary volume descriptor
    img[sector(16)] = 1;
    std::memcpy(&img[sector(16) + 1], "CD001", 5);
    img[sector(16) + 6] = 1;
    img[sector(16) + 156] = 34;
    put32(img, sector(16) + 158, 18);
    put32(img, sector(16) + 166, 2048);
    return img;
}

static std::vector<uint8_t> makeSfo(const std::string &titleId, const std::string &title)
{
    // header (20) + 2 entries (32) + key table + data table
    const std::string keys = std::string("TITLE_ID") + '\0' + "TITLE" + '\0';
    const std::string d1 = titleId + '\0';
    const std::string d2 = title + '\0';
    const uint32_t keyTable = 20 + 32;
    const uint32_t dataTable = keyTable + static_cast<uint32_t>(keys.size());
    std::vector<uint8_t> b(dataTable + d1.size() + d2.size(), 0);
    std::memcpy(b.data(), "\0PSF", 4);
    put32(b, 8, keyTable);
    put32(b, 12, dataTable);
    put32(b, 16, 2);
    auto entry = [&](size_t i, uint16_t keyOff, uint32_t len, uint32_t dataOff) {
        const size_t e = 20 + i * 16;
        b[e] = keyOff & 0xFF;
        b[e + 1] = keyOff >> 8;
        b[e + 2] = 0x04;
        b[e + 3] = 0x02;
        put32(b, e + 4, len);
        put32(b, e + 8, len);
        put32(b, e + 12, dataOff);
    };
    entry(0, 0, static_cast<uint32_t>(d1.size()), 0);
    entry(1, 9, static_cast<uint32_t>(d2.size()), static_cast<uint32_t>(d1.size()));
    std::memcpy(&b[keyTable], keys.data(), keys.size());
    std::memcpy(&b[dataTable], d1.data(), d1.size());
    std::memcpy(&b[dataTable + d1.size()], d2.data(), d2.size());
    return b;
}

static std::string one(const Detection &d)
{
    return d.candidates.size() == 1 ? systemId(d.candidates[0]) : (d.candidates.empty() ? "none" : "many");
}

int main()
{
    const fs::path root = fs::temp_directory_path() / "emu_detector_test";
    fs::remove_all(root);
    fs::create_directories(root);

    // --- optical images --------------------------------------------------------
    write(root / "ps2.iso", makeIso({{"SYSTEM.CNF;1", "BOOT2 = cdrom0:\\SLUS_200.62;1\r\nVER = 1.00\r\n"}}));
    write(root / "ps1.iso", makeIso({{"SYSTEM.CNF;1", "BOOT = cdrom:\\SCUS_941.63;1\r\nTCB = 4\r\n"}}));
    write(root / "ps3.iso", makeIso({{"PS3_GAME", "", true}, {"PS3_DISC.SFB;1", "x"}}));
    write(root / "empty.iso", makeIso({{"README.TXT;1", "hello"}}));

    CHECK(one(detectGame((root / "ps2.iso").string())) == "ps2");
    CHECK(one(detectGame((root / "ps1.iso").string())) == "ps1");
    CHECK(one(detectGame((root / "ps3.iso").string())) == "ps3");
    CHECK(one(detectGame((root / "empty.iso").string())) == "none");

    // raw 2352-byte BIN + CUE (mode 2 form 1 -> 24 byte header, and mode 1 -> 16 byte header)
    write(root / "disc" / "game.bin", makeIso({{"SYSTEM.CNF;1", "BOOT = cdrom:\\SLUS_000.00;1\r\n"}}, 2352, 24));
    writeText(root / "disc" / "game.cue", "FILE \"game.bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n");
    CHECK(one(detectGame((root / "disc" / "game.cue").string())) == "ps1");
    CHECK(one(detectGame((root / "disc" / "game.bin").string())) == "ps1");

    write(root / "disc2" / "g2.bin", makeIso({{"SYSTEM.CNF;1", "BOOT2 = cdrom0:\\SLES_500.00;1\r\n"}}, 2352, 16));
    writeText(root / "disc2" / "g2.cue", "FILE g2.bin BINARY\n  TRACK 01 MODE1/2352\n");
    CHECK(one(detectGame((root / "disc2" / "g2.cue").string())) == "ps2");

    // cue pointing at a missing bin falls back to PS1
    writeText(root / "disc3.cue", "FILE \"nothing.bin\" BINARY\n");
    CHECK(one(detectGame((root / "disc3.cue").string())) == "ps1");

    // --- GameCube / Wii headers -------------------------------------------------
    {
        std::vector<uint8_t> gc(0x100, 0);
        gc[0x1C] = 0xC2; gc[0x1D] = 0x33; gc[0x1E] = 0x9F; gc[0x1F] = 0x3D;
        std::memcpy(&gc[0x20], "Test GameCube Game", 18);
        write(root / "gc.iso", gc);
        std::vector<uint8_t> wii(0x100, 0);
        wii[0x18] = 0x5D; wii[0x19] = 0x1C; wii[0x1A] = 0x9E; wii[0x1B] = 0xA3;
        std::memcpy(&wii[0x20], "Test Wii Game", 13);
        write(root / "wii.iso", wii);
    }
    const auto gc = detectGame((root / "gc.iso").string());
    CHECK(one(gc) == "gamecube");
    CHECK(gc.title == "Test GameCube Game");
    CHECK(one(detectGame((root / "wii.iso").string())) == "wii");

    // --- Xbox 360 ----------------------------------------------------------------
    {
        std::vector<uint8_t> x(0x10020, 0);
        std::memcpy(&x[0x10000], "MICROSOFT*XBOX*MEDIA", 20);
        write(root / "x360.iso", x);
    }
    CHECK(one(detectGame((root / "x360.iso").string())) == "xbox360");

    // --- by extension -------------------------------------------------------------
    for (auto [file, sys] : std::vector<std::pair<const char *, const char *>>{
             {"a.nsp", "switch"}, {"a.xci", "switch"}, {"a.xex", "xbox360"}, {"a.wbfs", "wii"},
             {"a.gcz", "gamecube"}, {"a.pbp", "ps1"}, {"a.cso", "ps2"}}) {
        writeText(root / file, "x");
        CHECK(one(detectGame((root / file).string())) == sys);
    }
    writeText(root / "a.rvz", "x");
    CHECK(detectGame((root / "a.rvz").string()).candidates.size() == 2); // GC + Wii, same emulator
    writeText(root / "a.chd", "x");
    CHECK(one(detectGame((root / "a.chd").string())) == "many"); // PS1 or PS2: genuinely ambiguous
    writeText(root / "a.txt", "x");
    CHECK(one(detectGame((root / "a.txt").string())) == "none");

    // PS-X EXE
    {
        std::vector<uint8_t> e(32, 0);
        std::memcpy(e.data(), "PS-X EXE", 8);
        write(root / "psx.exe", e);
        write(root / "pc.exe", std::vector<uint8_t>{'M', 'Z', 0, 0, 0, 0, 0, 0});
    }
    CHECK(one(detectGame((root / "psx.exe").string())) == "ps1");
    CHECK(one(detectGame((root / "pc.exe").string())) == "none");

    // --- PS4 / PS5 folders ---------------------------------------------------------
    write(root / "ps4game" / "eboot.bin", std::vector<uint8_t>{0x4F, 0x15, 0x3D, 0x1D, 0, 0, 0, 0});
    write(root / "ps4game" / "sce_sys" / "param.sfo", makeSfo("CUSA01234", "My PS4 Game"));
    const auto ps4 = detectGame((root / "ps4game").string());
    CHECK(one(ps4) == "ps4");
    CHECK(ps4.title == "My PS4 Game");
    CHECK(fs::path(ps4.launchPath).filename() == "eboot.bin");

    write(root / "ps5game" / "eboot.bin", std::vector<uint8_t>{0x54, 0x14, 0xF5, 0xEE, 0, 0, 0, 0});
    write(root / "ps5game" / "SCE_SYS" / "PARAM.SFO", makeSfo("PPSA01234_00", "My PS5 Game")); // odd casing
    // note: 'SCE_SYS' on a case-sensitive FS must still be found
    CHECK(one(detectGame((root / "ps5game").string())) == "ps5");

    // dropping the eboot.bin itself must give the same answer
    CHECK(one(detectGame((root / "ps4game" / "eboot.bin").string())) == "ps4");

    // eboot without sce_sys: decided by the SELF header
    write(root / "lonely" / "eboot.bin", std::vector<uint8_t>{0x54, 0x14, 0xF5, 0xEE, 0, 0, 0, 0});
    CHECK(one(detectGame((root / "lonely" / "eboot.bin").string())) == "ps5");

    // --- PS3 folders ----------------------------------------------------------------
    write(root / "ps3game" / "PS3_GAME" / "PARAM.SFO", makeSfo("BLUS30000", "My PS3 Game"));
    write(root / "ps3game" / "PS3_GAME" / "USRDIR" / "EBOOT.BIN", std::vector<uint8_t>{'S', 'C', 'E', 0});
    const auto ps3 = detectGame((root / "ps3game").string());
    CHECK(one(ps3) == "ps3");
    CHECK(ps3.title == "My PS3 Game");

    // random folder is not a game
    write(root / "random" / "notes.txt", std::vector<uint8_t>{'h', 'i'});
    CHECK(one(detectGame((root / "random").string())) == "none");

    CHECK(isKnownGameExtension("x.ISO"));
    CHECK(!isKnownGameExtension("x.jpg"));

    fs::remove_all(root);
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
