// SPDX-License-Identifier: GPL-2.0-or-later
#include "PanelModel.hpp"

#include <algorithm>

#include "Format.hpp"

namespace sb {

namespace {

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string trimmed(std::string_view s) {
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && (s[begin] == ' ' || s[begin] == '\t')) ++begin;
    while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\t')) --end;
    return std::string(s.substr(begin, end - begin));
}

} // namespace

std::optional<std::uint32_t> parseColor(std::string_view text) {
    if (text.size() != 7 || text[0] != '#') return std::nullopt;
    std::uint32_t value = 0;
    for (std::size_t i = 1; i < 7; ++i) {
        const int digit = hexValue(text[i]);
        if (digit < 0) return std::nullopt;
        value = (value << 4) | static_cast<std::uint32_t>(digit);
    }
    return value;
}

std::string formatColor(std::uint32_t rgb) {
    static const char* digits = "0123456789ABCDEF";
    std::string out = "#";
    for (int shift = 20; shift >= 0; shift -= 4) out += digits[(rgb >> shift) & 0xFu];
    return out;
}

bool darkTextOn(std::uint32_t rgb) {
    const double r = static_cast<double>((rgb >> 16) & 0xFFu);
    const double g = static_cast<double>((rgb >> 8) & 0xFFu);
    const double b = static_cast<double>(rgb & 0xFFu);
    return 0.299 * r + 0.587 * g + 0.114 * b > 140.0;
}

std::optional<std::vector<int>> parseScoreValues(std::string_view text) {
    std::vector<int> out;
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == ',' || c == ' ' || c == ';') {
            ++i;
            continue;
        }
        if (c < '0' || c > '9') return std::nullopt;
        int value = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            value = value * 10 + (text[i] - '0');
            if (value > 99) return std::nullopt;
            ++i;
        }
        if (value < 1) return std::nullopt;
        if (std::find(out.begin(), out.end(), value) != out.end()) return std::nullopt;
        out.push_back(value);
        if (out.size() > 8) return std::nullopt;
    }
    if (out.empty()) return std::nullopt;
    return out;
}

std::string formatScoreValues(const std::vector<int>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += ", ";
        out += std::to_string(values[i]);
    }
    return out;
}

std::vector<std::string> parseLabelList(std::string_view text) {
    std::vector<std::string> out;
    if (trimmed(text).empty()) return out;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = text.find(',', start);
        if (comma == std::string_view::npos) {
            out.push_back(trimmed(text.substr(start)));
            break;
        }
        out.push_back(trimmed(text.substr(start, comma - start)));
        start = comma + 1;
    }
    return out;
}

std::string formatLabelList(const std::vector<std::string>& labels) {
    std::string out;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (i) out += ", ";
        out += labels[i];
    }
    return out;
}

std::vector<PenaltyRow> penaltyRows(const PenaltyBox& box) {
    const std::vector<const Penalty*> running = box.running();
    const auto isRunning = [&running](const Penalty& p) {
        return std::any_of(running.begin(), running.end(),
                           [&p](const Penalty* r) { return r->id == p.id; });
    };
    const auto rowOf = [](const Penalty& p, bool queued) {
        PenaltyRow row;
        row.id = p.id;
        row.player = p.player;
        row.time = formatPenaltyTime(p.remaining);
        if (p.hasNextPhase()) row.phase2 = "+" + formatPenaltyTime(p.phases[p.phase + 1].duration);
        row.queued = queued;
        return row;
    };
    std::vector<PenaltyRow> rows;
    for (const Penalty& p : box.all())
        if (isRunning(p)) rows.push_back(rowOf(p, false));
    for (const Penalty& p : box.all())
        if (!isRunning(p)) rows.push_back(rowOf(p, true));
    return rows;
}

} // namespace sb
