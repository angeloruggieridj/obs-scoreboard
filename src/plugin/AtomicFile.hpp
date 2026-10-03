// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <optional>
#include <string>

// Path of `file` in this module's config folder (created if needed), as UTF-8.
std::string moduleConfigPath(const char* file);
// Writes `bytes` to `path.tmp`, then renames it over `path`: a reader never sees half a file.
bool writeFileAtomic(const std::string& path, const std::string& bytes, std::string* error);
// Whole file as bytes; nullopt when it does not exist or cannot be read.
std::optional<std::string> readFile(const std::string& path);
