// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>
#include <vector>

#include "Types.hpp"

namespace sb {

struct PeriodConfig {
    int regulationPeriods = 1;
    Tenths regulationDuration = 0;
    int overtimePeriods = 0; // kUnlimited allowed
    Tenths overtimeDuration = 0;
    // Count-up sports whose shown time continues across periods (2nd half 45:00 -> 90:00).
    bool continuousDisplay = false;
    std::vector<std::string> regulationLabels; // empty entries fall back to "1", "2", ...
    std::vector<std::string> overtimeLabels;   // fall back to "OT", "OT2", ...
};

// Which period is being played. Index is 1-based across regulation and overtime.
class Periods {
public:
    void configure(PeriodConfig cfg);
    int index() const { return index_; }
    bool isOvertime() const { return index_ > cfg_.regulationPeriods; }
    int overtimeNumber() const { return isOvertime() ? index_ - cfg_.regulationPeriods : 0; }
    Tenths duration() const { return durationOf(index_); }
    Tenths durationOf(int index) const;
    Tenths displayOffset() const;
    std::string label() const;
    bool hasNext() const;
    bool hasPrev() const { return index_ > 1; }
    bool next();
    bool prev();
    bool setIndex(int index);
    const PeriodConfig& config() const { return cfg_; }

private:
    bool valid(int index) const;

    PeriodConfig cfg_;
    int index_ = 1;
};

} // namespace sb
