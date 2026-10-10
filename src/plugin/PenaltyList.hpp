// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>

#include <vector>

#include "PanelModel.hpp"

class QLabel;
class QToolButton;
class QVBoxLayout;

// One team's penalties: running ones with their time, queued ones marked, a cross to cancel each.
// The cross cancels by penalty id, never by position: the list moves while the clock runs.
class PenaltyList : public QWidget {
    Q_OBJECT
public:
    explicit PenaltyList(QWidget* parent = nullptr);
    void setRows(const std::vector<sb::PenaltyRow>& rows);
    std::size_t shownCount() const { return shown_; }

signals:
    void cancelRequested(unsigned id);

private:
    struct Slot {
        QWidget* box = nullptr;
        QLabel* label = nullptr;
        QToolButton* cancel = nullptr;
        unsigned id = 0; // the penalty this slot shows right now
    };
    void ensureSlots(std::size_t count);

    QVBoxLayout* layout_ = nullptr;
    std::vector<Slot> slots_;
    std::size_t shown_ = 0;
};
