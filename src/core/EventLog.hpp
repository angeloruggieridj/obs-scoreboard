// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Types.hpp"

namespace sb {

enum class EventType { PeriodStart, PeriodEnd, Score, Penalty, MatchEnd };

struct MatchEvent {
    EventType type = EventType::PeriodStart;
    Micros at = 0;       // monotonic time of the event
    Tenths playTime = 0; // cumulative play time when it happened
    int period = 1;
    std::string periodLabel;
    std::optional<Team> team;
    int homeScore = 0; // score after the event
    int awayScore = 0;
    std::string detail; // e.g. the penalized player's number
};

class EventLog {
public:
    void add(MatchEvent e) { events_.push_back(std::move(e)); }
    const std::vector<MatchEvent>& events() const { return events_; }
    void clear() { events_.clear(); }
    void restoreFrom(std::vector<MatchEvent> events) { events_ = std::move(events); }

private:
    std::vector<MatchEvent> events_;
};

enum class ChapterBase { StreamStart, PlayTime };
// YouTube needs the first chapter at 0:00 and chapters at least 10 seconds long.
constexpr std::int64_t kMinChapterSeconds = 10;

// One "M:SS Title" (or "H:MM:SS Title") line per event, joined by '\n'. Titles come from
// `describe`, so the core stays free of any language.
std::string youtubeChapters(const std::vector<MatchEvent>& events, ChapterBase base,
                            Micros streamStart, std::string_view firstTitle,
                            const std::function<std::string(const MatchEvent&)>& describe);

} // namespace sb
