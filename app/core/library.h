// The images folder: what is there and what each file is (C++17 filesystem, no LVGL).
#pragma once

#include "image_info.h"

#include <filesystem>
#include <string>
#include <vector>

namespace dp {

struct Image {
    std::string path;  // absolute
    std::string name;  // file name
    ImageInfo info;
};

// Images in dir, sorted by name (case-insensitive). error is set when the folder cannot be read.
std::vector<Image> scan(const std::filesystem::path& dir, std::string& error);

// index of the image with this path, or -1
int find_path(const std::vector<Image>& images, const std::string& path);

}  // namespace dp
