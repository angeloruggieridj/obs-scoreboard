// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "Fields.hpp"

#include <fstream>
#include <sstream>
#include <string>

using namespace sb;

namespace {

std::string slurp(const std::string& file) {
    std::ifstream in(std::string(SB_LOCALE_DIR) + "/" + file, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

} // namespace

TEST_CASE("every field has a display name in both locales") {
    for (const char* file : {"en-US.ini", "it-IT.ini"}) {
        const std::string content = "\n" + slurp(file);
        CHECK(content.size() > 1);
        for (std::size_t i = 0; i < kFieldCount; ++i) {
            const std::string key = "Field." + std::string(fieldKey(static_cast<FieldId>(i)));
            INFO(file << ": " << key);
            CHECK(content.find("\n" + key + "=") != std::string::npos);
        }
    }
}
