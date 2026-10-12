// SPDX-License-Identifier: GPL-2.0-or-later
#include "ChaptersExport.hpp"

#include <algorithm>

namespace sb {

std::string chapterTimeString(std::int64_t seconds) {
    const std::int64_t h = seconds / 3600;
    const std::int64_t m = (seconds / 60) % 60;
    const std::int64_t s = seconds % 60;
    const std::string ss = (s < 10 ? "0" : "") + std::to_string(s);
    if (h > 0) return std::to_string(h) + ":" + (m < 10 ? "0" : "") + std::to_string(m) + ":" + ss;
    return std::to_string(m) + ":" + ss;
}

ChaptersResult buildChapters(const std::vector<MatchEvent>& events, ChapterBase base,
                             Micros streamStart, std::string_view firstTitle,
                             const std::function<std::string(const MatchEvent&)>& describe,
                             Micros now) {
    ChaptersResult out;
    out.chapters.push_back({0, std::string(firstTitle)});
    std::int64_t last = 0;
    for (const MatchEvent& e : events) {
        // A resumed match keeps `at` values from a previous OBS session: the stream/recording
        // base must ignore anything outside [streamStart, now] or the chapter list is stretched.
        if (base == ChapterBase::StreamStart && (e.at < streamStart || e.at > now)) continue;
        const std::int64_t t = base == ChapterBase::StreamStart
                                   ? std::max<Micros>(0, e.at - streamStart) / 1000000
                                   : e.playTime / kTenthsPerSecond;
        if (t < last + kMinChapterSeconds) continue;
        out.chapters.push_back({t, describe(e)});
        last = t;
    }
    for (const Chapter& c : out.chapters) {
        if (!out.text.empty()) out.text += '\n';
        out.text += chapterTimeString(c.seconds);
        out.text += ' ';
        out.text += c.title;
    }
    out.youtubeReady = out.chapters.size() >= kMinChapters;
    return out;
}

} // namespace sb
