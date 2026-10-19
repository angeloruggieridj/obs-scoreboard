// SPDX-License-Identifier: GPL-2.0-or-later
#include "UpdateCheck.hpp"

#include <cctype>
#include <charconv>
#include <cstddef>
#include <system_error>

#include <nlohmann/json.hpp>

namespace sb {

namespace {

bool parseNumber(std::string_view text, int& out) {
    if (text.empty()) return false;
    for (const char c : text)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc() && result.ptr == text.data() + text.size();
}

// "beta4" -> {"beta", 4}; a suffix without digits has number 0.
struct PreParts {
    std::string letters;
    int number = 0;
};

PreParts splitPre(const std::string& pre) {
    PreParts parts;
    std::size_t i = 0;
    while (i < pre.size() && !std::isdigit(static_cast<unsigned char>(pre[i]))) ++i;
    parts.letters = pre.substr(0, i);
    if (i < pre.size()) parseNumber(std::string_view(pre).substr(i), parts.number);
    return parts;
}

int cmp(int a, int b) {
    return a < b ? -1 : (a > b ? 1 : 0);
}

} // namespace

std::optional<Version> parseVersion(std::string_view text) {
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) text.remove_prefix(1);
    Version v;
    const std::size_t dash = text.find('-');
    std::string_view numbers = text.substr(0, dash);
    if (dash != std::string_view::npos) {
        const std::string_view pre = text.substr(dash + 1);
        if (pre.empty()) return std::nullopt;
        v.pre = std::string(pre);
    }
    const std::size_t dot1 = numbers.find('.');
    if (dot1 == std::string_view::npos) return std::nullopt;
    const std::size_t dot2 = numbers.find('.', dot1 + 1);
    if (dot2 == std::string_view::npos) return std::nullopt;
    if (!parseNumber(numbers.substr(0, dot1), v.major)) return std::nullopt;
    if (!parseNumber(numbers.substr(dot1 + 1, dot2 - dot1 - 1), v.minor)) return std::nullopt;
    if (!parseNumber(numbers.substr(dot2 + 1), v.patch)) return std::nullopt;
    return v;
}

int compareVersions(const Version& a, const Version& b) {
    if (const int c = cmp(a.major, b.major)) return c;
    if (const int c = cmp(a.minor, b.minor)) return c;
    if (const int c = cmp(a.patch, b.patch)) return c;
    if (a.pre.empty() && b.pre.empty()) return 0;
    if (a.pre.empty()) return 1; // a final release outranks its own pre-releases
    if (b.pre.empty()) return -1;
    const PreParts pa = splitPre(a.pre);
    const PreParts pb = splitPre(b.pre);
    if (pa.letters != pb.letters) return pa.letters < pb.letters ? -1 : 1;
    return cmp(pa.number, pb.number);
}

std::optional<ReleaseInfo> newestRelease(std::string_view releasesJson, const Version& current,
                                         bool includePrereleases) {
    const nlohmann::json doc = nlohmann::json::parse(releasesJson, nullptr, false);
    if (!doc.is_array()) return std::nullopt;

    std::optional<ReleaseInfo> best;
    Version bestVersion;
    for (const nlohmann::json& entry : doc) {
        if (!entry.is_object()) continue;
        const auto tag = entry.find("tag_name");
        if (tag == entry.end() || !tag->is_string()) continue;
        const auto draft = entry.find("draft");
        if (draft != entry.end() && draft->is_boolean() && draft->get<bool>()) continue;
        const auto pre = entry.find("prerelease");
        const bool isPre = pre != entry.end() && pre->is_boolean() && pre->get<bool>();
        if (isPre && !includePrereleases) continue;
        const std::optional<Version> version = parseVersion(tag->get<std::string>());
        if (!version || compareVersions(*version, current) <= 0) continue;
        if (best && compareVersions(*version, bestVersion) <= 0) continue;

        ReleaseInfo info;
        info.version = tag->get<std::string>();
        const auto name = entry.find("name");
        if (name != entry.end() && name->is_string()) info.name = name->get<std::string>();
        const auto url = entry.find("html_url");
        if (url != entry.end() && url->is_string()) info.url = url->get<std::string>();
        info.prerelease = isPre;
        best = std::move(info);
        bestVersion = *version;
    }
    return best;
}

} // namespace sb
