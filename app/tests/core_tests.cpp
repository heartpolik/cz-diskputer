// Host tests for the DiscPuter core. Non-zero exit on failure.
#include "image_info.h"
#include "library.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace dp;
namespace fs = std::filesystem;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

void eqs(const std::string& got, const std::string& want, const std::string& what) {
    check(got == want, what + ": got '" + got + "', want '" + want + "'");
}

// a fake image head: volume descriptors at sector 16.. as given ("P" primary, "B" El Torito,
// "T" terminator, "U" UDF NSR02 after the terminator), optional MBR / GPT at the start
std::vector<uint8_t> head(const std::string& layout, const char* label, bool mbr = false, bool gpt = false) {
    std::vector<uint8_t> h(kProbeBytes, 0);
    size_t s = 16;
    for (char c : layout) {
        uint8_t* d = h.data() + s * 2048;
        switch (c) {
            case 'P':
                d[0] = 1;
                std::memcpy(d + 1, "CD001", 5);
                std::memset(d + 40, ' ', 32);
                std::memcpy(d + 40, label, std::strlen(label));
                break;
            case 'B':
                d[0] = 0;
                std::memcpy(d + 1, "CD001", 5);
                std::memcpy(d + 7, "EL TORITO SPECIFICATION", 23);
                break;
            case 'T':
                d[0] = 255;
                std::memcpy(d + 1, "CD001", 5);
                break;
            case 'U':
                std::memcpy(d + 1, "NSR02", 5);
                break;
            case '-':
                std::memcpy(d + 1, "BEA01", 5);
                break;
        }
        ++s;
    }
    if (mbr) {
        h[510] = 0x55;
        h[511] = 0xAA;
        h[446 + 4] = 0x17;  // a partition type in entry 1
    }
    if (gpt) std::memcpy(h.data() + 512, "EFI PART", 8);
    return h;
}

void test_inspect() {
    auto h = head("PBT", "DEBIAN 13 AMD64 N");
    auto i = inspect(h.data(), h.size(), 700000000, "debian.iso");
    check(i.iso9660 && i.el_torito && !i.udf, "iso + el torito");
    eqs(i.label, "DEBIAN 13 AMD64 N", "label trimmed");
    check(i.recommended() == Mode::Cd, "iso -> CD");
    eqs(i.tags(), "BOOT", "boot tag");

    h = head("PBT", "UBUNTU", true, true);
    i = inspect(h.data(), h.size(), 5000000000ull, "ubuntu.iso");
    check(i.hybrid(), "hybrid");
    eqs(i.tags(), "BOOT HYBRID", "hybrid tags");

    h = head("PBT-U", "CCCOMA_X64FRE_EN-US");
    i = inspect(h.data(), h.size(), 5800000000ull, "Win11.ISO");
    check(i.iso9660 && i.udf && i.el_torito, "windows: iso + udf bridge");
    eqs(i.tags(), "BOOT", "udf bridge shows only boot");

    h = head("", "", true);
    i = inspect(h.data(), h.size(), 8000000000ull, "rpi.img");
    check(!i.iso9660 && i.mbr && i.recommended() == Mode::Disk, "disk image -> DISK");
    eqs(i.tags(), "MBR", "mbr tag");

    h = head("", "");
    i = inspect(h.data(), h.size(), 1474560, "floppy.img");
    eqs(i.tags(), "RAW", "raw tag");

    i = inspect(nullptr, 0, 700000000, "game.cue");
    check(i.cue && !i.supported(), "cue unsupported for now");
    eqs(i.tags(), "BIN/CUE", "cue tag");

    // truncated file: no crash, nothing found
    h = head("PBT", "X");
    i = inspect(h.data(), 100, 100, "tiny.iso");
    check(!i.iso9660, "short head");
}

void test_format() {
    eqs(format_size(4700000000ull), "4.7 GB", "GB");
    eqs(format_size(650000000), "650 MB", "MB");
    eqs(format_size(921600), "922 KB", "KB");
    eqs(format_size(10), "1 KB", "tiny");
    check(is_image_name("a.ISO") && is_image_name("b.img") && is_image_name("c.cue"), "image names");
    check(!is_image_name("notes.txt") && !is_image_name("._a.iso") && !is_image_name("iso"), "not images");
}

void test_scan(const char* real_iso) {
    const fs::path dir = fs::temp_directory_path() / "dp_scan_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    auto write = [&](const std::string& name, const std::vector<uint8_t>& data) {
        std::ofstream o(dir / name, std::ios::binary);
        o.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    };
    write("b.iso", head("PBT", "BETA"));
    write("A.iso", head("PT", "ALPHA"));
    write("readme.txt", {1, 2, 3});
    fs::create_directories(dir / "sub.iso");  // a folder named like an image is skipped

    std::string err;
    auto imgs = scan(dir, err);
    check(err.empty(), "scan ok");
    check(imgs.size() == 2, "two images");
    if (imgs.size() == 2) {
        eqs(imgs[0].name, "A.iso", "sorted case-insensitively");
        eqs(imgs[0].info.label, "ALPHA", "label from file");
        check(imgs[1].info.el_torito, "boot from file");
        check(find_path(imgs, imgs[1].path) == 1, "find path");
    }
    check(find_path(imgs, "/nope") == -1, "missing path");
    scan(dir / "missing", err);
    check(!err.empty(), "missing folder error");
    fs::remove_all(dir);

    if (real_iso) {
        std::ifstream in(real_iso, std::ios::binary);
        std::vector<uint8_t> h(kProbeBytes);
        in.read(reinterpret_cast<char*>(h.data()), static_cast<std::streamsize>(h.size()));
        const auto i = inspect(h.data(), static_cast<size_t>(in.gcount()), fs::file_size(real_iso), real_iso);
        check(i.iso9660, "real iso (hdiutil) is ISO 9660");
        std::printf("real iso: label '%s' tags '%s'\n", i.label.c_str(), i.tags().c_str());
    }
}

}  // namespace

int main(int argc, char** argv) {
    test_inspect();
    test_format();
    test_scan(argc > 1 ? argv[1] : nullptr);
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
