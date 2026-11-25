#include "library.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace dp {
namespace fs = std::filesystem;

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

std::vector<Image> scan(const fs::path& dir, std::string& error) {
    std::vector<Image> out;
    error.clear();
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        error = "no folder " + dir.string();
        return out;
    }
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        const std::string name = e.path().filename().string();
        if (!is_image_name(name)) continue;
        std::error_code fe;
        if (!e.is_regular_file(fe)) continue;
        const uint64_t size = e.file_size(fe);
        std::vector<uint8_t> head(kProbeBytes);
        size_t n = 0;
        {
            std::ifstream in(e.path(), std::ios::binary);
            in.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
            n = static_cast<size_t>(in.gcount());
        }
        Image img;
        img.path = fs::absolute(e.path()).string();
        img.name = name;
        img.info = inspect(head.data(), n, size, name);
        out.push_back(std::move(img));
    }
    if (ec) error = "cannot read " + dir.string() + ": " + ec.message();
    std::sort(out.begin(), out.end(), [](const Image& a, const Image& b) { return lower(a.name) < lower(b.name); });
    return out;
}

int find_path(const std::vector<Image>& images, const std::string& path) {
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].path == path) return static_cast<int>(i);
    }
    return -1;
}

}  // namespace dp
