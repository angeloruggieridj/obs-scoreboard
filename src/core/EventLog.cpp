// SPDX-License-Identifier: GPL-2.0-or-later
#include "EventLog.hpp"

#include "ChaptersExport.hpp"

namespace sb {

// The full chapter builder (list + text + YouTube readiness) lives in ChaptersExport; this keeps
// the historical EventLog entry point as a thin wrapper so there is a single implementation. The
// wrapper has no clock, so it does not apply the resumed-match cutoff (kNoUpperBound).
std::string youtubeChapters(const std::vector<MatchEvent>& events, ChapterBase base,
                            Micros streamStart, std::string_view firstTitle,
                            const std::function<std::string(const MatchEvent&)>& describe) {
    return buildChapters(events, base, streamStart, firstTitle, describe, kNoUpperBound).text;
}

} // namespace sb
