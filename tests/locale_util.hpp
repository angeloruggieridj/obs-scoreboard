// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Reads data/locale/<file> (SB_LOCALE_DIR is set by tests/CMakeLists.txt) and says whether it
// defines `key`. Used to cover the locale keys the code builds at run time.

#include <fstream>
#include <sstream>
#include <string>

inline bool localeHasKey(const char* file, const std::string& key) {
    std::ifstream in(std::string(SB_LOCALE_DIR) + "/" + file, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return ("\n" + out.str()).find("\n" + key + "=") != std::string::npos;
}
