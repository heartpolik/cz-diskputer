#include "image_info.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace dp {
namespace {

constexpr size_t kSector = 2048;

std::string lower_ext(const std::string& name) {
    const auto dot = name.rfind('.');
    if (dot == std::string::npos) return "";
    std::string e = name.substr(dot + 1);
    for (auto& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}

bool has(const uint8_t* p, const char* s) { return std::memcmp(p, s, std::strlen(s)) == 0; }

std::string trimmed(const uint8_t* p, size_t n) {
    std::string s(reinterpret_cast<const char*>(p), n);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    return s;
}

}  // namespace

const char* mode_name(Mode m) { return m == Mode::Cd ? "CD" : "DISK"; }

std::string ImageInfo::tags() const {
    std::string t;
    auto add = [&t](const char* s) {
        if (!t.empty()) t += ' ';
        t += s;
    };
    if (cue) add("BIN/CUE");
    if (el_torito) add("BOOT");
    else if (iso9660 || udf) add("DATA");
    if (hybrid()) add("HYBRID");
    if (udf && !iso9660) add("UDF");
    if (!iso9660 && !udf && !cue) add(gpt ? "GPT" : mbr ? "MBR" : "RAW");
    return t;
}

ImageInfo inspect(const uint8_t* head, size_t n, uint64_t size, const std::string& filename) {
    ImageInfo info;
    info.size = size;
    const std::string ext = lower_ext(filename);
    if (ext == "cue" || ext == "bin") {
        info.cue = true;
        return info;
    }

    if (n >= 512 && head[510] == 0x55 && head[511] == 0xAA) {
        // a real partition table has at least one non-empty entry
        for (int i = 0; i < 4; ++i) {
            const uint8_t* e = head + 446 + i * 16;
            if (e[4] != 0) info.mbr = true;
        }
    }
    if (n >= 520 && has(head + 512, "EFI PART")) info.gpt = true;

    // volume descriptor set: sectors 16.. until the terminator (type 255)
    size_t s = 16;
    for (; (s + 1) * kSector <= n; ++s) {
        const uint8_t* d = head + s * kSector;
        if (!has(d + 1, "CD001")) break;
        const uint8_t type = d[0];
        if (type == 1 && !info.iso9660) {
            info.iso9660 = true;
            info.label = trimmed(d + 40, 32);
        } else if (type == 0 && has(d + 7, "EL TORITO SPECIFICATION")) {
            info.el_torito = true;
        } else if (type == 255) {
            ++s;
            break;
        }
    }
    // UDF volume recognition sequence right after: BEA01, NSR02/NSR03, TEA01
    for (size_t k = info.iso9660 ? s : 16; (k + 1) * kSector <= n && k < s + 8; ++k) {
        const uint8_t* d = head + k * kSector;
        if (has(d + 1, "NSR02") || has(d + 1, "NSR03")) {
            info.udf = true;
            break;
        }
    }
    return info;
}

std::string format_size(uint64_t b) {
    char buf[32];
    if (b >= 1000ull * 1000 * 1000) std::snprintf(buf, sizeof buf, "%.1f GB", static_cast<double>(b) / 1e9);
    else if (b >= 1000ull * 1000) std::snprintf(buf, sizeof buf, "%.0f MB", static_cast<double>(b) / 1e6);
    else std::snprintf(buf, sizeof buf, "%.0f KB", std::max(1.0, static_cast<double>(b) / 1e3));
    return buf;
}

bool is_image_name(const std::string& name) {
    if (!name.empty() && name[0] == '.') return false;
    const std::string e = lower_ext(name);
    return e == "iso" || e == "img" || e == "bin" || e == "cue";
}

}  // namespace dp
