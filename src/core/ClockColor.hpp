// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// The color of the dock's clock digits, optionally driven by the time left in the period: pure, so
// it is tested without Qt. Off by default (the digits keep the theme's text color).

#include <cstddef>
#include <string>
#include <vector>

#include "Types.hpp"

namespace sb {

enum class ClockColorMode {
    Theme,  // the theme's text color (the default)
    Single, // always `base`
    ByTime, // `base`, then the rules below as the time left shrinks
};

// "From `secondsLeft` seconds left, this color." With `blend` off a rule applies as a step from its
// moment on; with `blend` on its color fades linearly into the next rule's color, reaching it at
// the next rule's moment (the last rule keeps its color).
struct ClockColorRule {
    int secondsLeft = 30; // 0 .. kMaxRuleSeconds
    std::string color;    // "#RRGGBB"
};

constexpr std::size_t kMaxClockColorRules = 4;
constexpr int kMaxRuleSeconds = 3600;

struct ClockColorSettings {
    ClockColorMode mode = ClockColorMode::Theme;
    std::string base; // "#RRGGBB": the color before any rule (and the color of Single)
    std::vector<ClockColorRule> rules;
    bool blend = false;

    bool operator==(const ClockColorSettings& other) const {
        if (mode != other.mode || base != other.base || blend != other.blend ||
            rules.size() != other.rules.size())
            return false;
        for (std::size_t i = 0; i < rules.size(); ++i)
            if (rules[i].secondsLeft != other.rules[i].secondsLeft ||
                rules[i].color != other.rules[i].color)
                return false;
        return true;
    }
    bool operator!=(const ClockColorSettings& other) const { return !(*this == other); }
};

// The color of the digits with `remaining` of the period left: "" means "the theme's own color".
// A negative `remaining` (a period without a length) never triggers a rule. Rules may come in any
// order; invalid colors are ignored.
std::string clockColorAt(const ClockColorSettings& settings, Tenths remaining);

} // namespace sb
