// SPDX-License-Identifier: GPL-2.0-or-later
#include "ClockColor.hpp"

#include <algorithm>
#include <cstdint>

#include "PanelModel.hpp"

namespace sb {

namespace {

struct Stop {
    double seconds;
    std::uint32_t rgb;
};

std::uint32_t lerp(std::uint32_t a, std::uint32_t b, double t) {
    const auto channel = [&](int shift) {
        const double from = static_cast<double>((a >> shift) & 0xFFu);
        const double to = static_cast<double>((b >> shift) & 0xFFu);
        return static_cast<std::uint32_t>(from + (to - from) * t + 0.5) & 0xFFu;
    };
    return (channel(16) << 16) | (channel(8) << 8) | channel(0);
}

} // namespace

std::string clockColorAt(const ClockColorSettings& settings, Tenths remaining) {
    if (settings.mode == ClockColorMode::Theme) return {};
    const auto base = parseColor(settings.base);
    if (settings.mode == ClockColorMode::Single) return base ? formatColor(*base) : std::string();
    if (remaining < 0) return base ? formatColor(*base) : std::string();

    std::vector<Stop> stops;
    for (const ClockColorRule& rule : settings.rules) {
        const auto rgb = parseColor(rule.color);
        if (!rgb || rule.secondsLeft < 0 || rule.secondsLeft > kMaxRuleSeconds) continue;
        stops.push_back({static_cast<double>(rule.secondsLeft), *rgb});
    }
    std::stable_sort(stops.begin(), stops.end(),
                     [](const Stop& a, const Stop& b) { return a.seconds > b.seconds; });

    const double left = static_cast<double>(remaining) / static_cast<double>(kTenthsPerSecond);
    if (stops.empty() || left > stops.front().seconds)
        return base ? formatColor(*base) : std::string();

    // `index` is the last rule that has started.
    std::size_t index = 0;
    while (index + 1 < stops.size() && left <= stops[index + 1].seconds) ++index;
    if (!settings.blend || index + 1 >= stops.size()) return formatColor(stops[index].rgb);

    const Stop& from = stops[index];
    const Stop& to = stops[index + 1];
    const double span = from.seconds - to.seconds;
    const double t = span > 0.0 ? std::clamp((from.seconds - left) / span, 0.0, 1.0) : 1.0;
    return formatColor(lerp(from.rgb, to.rgb, t));
}

} // namespace sb
