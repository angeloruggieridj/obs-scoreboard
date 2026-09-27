// SPDX-License-Identifier: GPL-2.0-or-later
#include "Periods.hpp"

#include <utility>

namespace sb {

void Periods::configure(PeriodConfig cfg) {
    cfg_ = std::move(cfg);
    if (cfg_.regulationPeriods < 1) cfg_.regulationPeriods = 1;
    index_ = 1;
}

Tenths Periods::durationOf(int index) const {
    return index > cfg_.regulationPeriods ? cfg_.overtimeDuration : cfg_.regulationDuration;
}

Tenths Periods::displayOffset() const {
    if (!cfg_.continuousDisplay) return 0;
    Tenths offset = 0;
    for (int i = 1; i < index_; ++i) offset += durationOf(i);
    return offset;
}

std::string Periods::label() const {
    if (!isOvertime()) {
        const auto i = static_cast<std::size_t>(index_ - 1);
        if (i < cfg_.regulationLabels.size() && !cfg_.regulationLabels[i].empty())
            return cfg_.regulationLabels[i];
        return std::to_string(index_);
    }
    const int ot = overtimeNumber();
    const auto i = static_cast<std::size_t>(ot - 1);
    if (i < cfg_.overtimeLabels.size() && !cfg_.overtimeLabels[i].empty())
        return cfg_.overtimeLabels[i];
    return ot == 1 ? std::string("OT") : "OT" + std::to_string(ot);
}

bool Periods::valid(int index) const {
    if (index < 1) return false;
    if (cfg_.overtimePeriods == kUnlimited) return true;
    return index <= cfg_.regulationPeriods + cfg_.overtimePeriods;
}

bool Periods::hasNext() const {
    return valid(index_ + 1);
}

bool Periods::next() {
    return setIndex(index_ + 1);
}

bool Periods::prev() {
    return setIndex(index_ - 1);
}

bool Periods::setIndex(int index) {
    if (!valid(index)) return false;
    index_ = index;
    return true;
}

} // namespace sb
