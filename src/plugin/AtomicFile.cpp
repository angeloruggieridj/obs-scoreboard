// SPDX-License-Identifier: GPL-2.0-or-later
#include "AtomicFile.hpp"

#include <obs-module.h>
#include <util/bmem.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

std::string moduleConfigPath(const char* file) {
    char* raw = obs_module_config_path(file);
    std::string path = raw ? raw : "";
    bfree(raw);
    if (!path.empty()) {
        std::error_code ec;
        fs::create_directories(fs::u8path(path).parent_path(), ec);
    }
    return path;
}

bool writeFileAtomic(const std::string& path, const std::string& bytes, std::string* error) {
    const fs::path target = fs::u8path(path);
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            if (error) *error = "cannot write " + tmp.u8string();
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, target, ec); // replaces an existing target (MoveFileExW on Windows)
    if (ec) {
        if (error) *error = ec.message();
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream in(fs::u8path(path), std::ios::binary);
    if (!in) return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
