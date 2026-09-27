// SPDX-License-Identifier: GPL-2.0-or-later
#include "EventLog.hpp"

#include <algorithm>

namespace sb {
namespace {

std::string chapterTime(std::int64_t secs) {
    const std::int64_t h = secs / 3600;
    const std::int64_t m = (secs / 60) % 60;
    const std::int64_t s = secs % 60;
    const std::string ss = (s < 10 ? "0" : "") + std::to_string(s);
    if (h > 0) return std::to_string(h) + ":" + (m < 10 ? "0" : "") + std::to_string(m) + ":" + ss;
    return std::to_string(m) + ":" + ss;
}

} // namespace

std::string youtubeChapters(const std::vector<MatchEvent>& events, ChapterBase base,
                            Micros streamStart, std::string_view firstTitle,
                            const std::function<std::string(const MatchEvent&)>& describe) {
    std::string out = "0:00 " + std::string(firstTitle);
    std::int64_t last = 0;
    for (const MatchEvent& e : events) {
        const std::int64_t t = base == ChapterBase::StreamStart
                                   ? std::max<Micros>(0, e.at - streamStart) / 1000000
                                   : e.playTime / kTenthsPerSecond;
        if (t < last + kMinChapterSeconds) continue;
        out += "\n" + chapterTime(t) + " " + describe(e);
        last = t;
    }
    return out;
}

} // namespace sb
