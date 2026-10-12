// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// YouTube chapter export for the event log (spec §5, CLAUDE.md §4.11). Pure core: the caller
// supplies the (localized) titles, so the core stays free of any language.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "EventLog.hpp"
#include "Types.hpp"

namespace sb {

// YouTube disables chapters when there are fewer than three (and wants the first one at 0:00).
constexpr std::size_t kMinChapters = 3;
// Upper bound for `now` meaning "no filtering": the legacy youtubeChapters wrapper has no clock.
constexpr Micros kNoUpperBound = std::numeric_limits<Micros>::max();

// One exported chapter: `seconds` is relative to the chosen base, `title` is the caller's text.
struct Chapter {
    std::int64_t seconds = 0;
    std::string title;
};

struct ChaptersResult {
    std::vector<Chapter> chapters; // the first one is always at 0 s
    std::string text;              // the ready-to-paste YouTube chapter block
    bool youtubeReady = false;     // chapters.size() >= kMinChapters
};

// "M:SS" (minutes may exceed 99, no leading zero) or "H:MM:SS" from one hour up: the exact time
// format YouTube chapter markers use. Shared by the export and the event-log window.
std::string chapterTimeString(std::int64_t seconds);

// Builds the chapters and their text from the match events. `base` selects whether the times are
// measured from `streamStart` (the OBS streaming/recording start) or from the game clock
// (`MatchEvent::playTime`). The first chapter is always at 0 with `firstTitle`; a later event
// closer than kMinChapterSeconds to the previous chapter is skipped, so the list is strictly
// ascending. Titles come from `describe`, so the core stays language-free. With the
// stream/recording base, events outside [streamStart, now] are ignored: a resumed match keeps
// `at` values from a previous OBS session (the monotonic clock resets on reboot), so its old
// events must not stretch the chapter list. `now` defaults to "no upper bound".
ChaptersResult buildChapters(const std::vector<MatchEvent>& events, ChapterBase base,
                             Micros streamStart, std::string_view firstTitle,
                             const std::function<std::string(const MatchEvent&)>& describe,
                             Micros now = kNoUpperBound);

} // namespace sb
