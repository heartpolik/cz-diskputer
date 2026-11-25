// What a disc image is, from its first sectors (pure C++, host-tested).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dp {

// Bytes needed from the start of an image: the volume descriptors live in
// 2048-byte sectors 16.. (ISO 9660) and the UDF recognition sequence follows.
constexpr size_t kProbeBytes = 64 * 1024;

enum class Mode { Cd, Disk };
const char* mode_name(Mode m);  // "CD", "DISK"

struct ImageInfo {
    uint64_t size{0};
    bool iso9660{false};
    bool udf{false};
    bool el_torito{false};  // bootable CD (BIOS and/or UEFI)
    bool mbr{false};        // partition table at sector 0: a "hybrid" ISO or a disk image
    bool gpt{false};
    bool cue{false};        // .cue / .bin: needs iteration B
    std::string label;      // ISO 9660 volume id, trimmed

    // how the computer should see it
    Mode recommended() const { return (iso9660 || udf) ? Mode::Cd : Mode::Disk; }
    bool hybrid() const { return (iso9660 || udf) && (mbr || gpt); }
    bool supported() const { return !cue; }
    // short tags for the list: "BOOT HYBRID UDF"
    std::string tags() const;
};

// head: the first bytes of the file (up to kProbeBytes); size: the whole file
ImageInfo inspect(const uint8_t* head, size_t n, uint64_t size, const std::string& filename);

// "4.7 GB", "650 MB", "900 KB"
std::string format_size(uint64_t bytes);

bool is_image_name(const std::string& filename);  // .iso .img .bin .cue (case-insensitive)

}  // namespace dp
