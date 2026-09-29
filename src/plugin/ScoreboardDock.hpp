// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPointer>
#include <QString>
#include <QWidget>

#include "Fields.hpp"

class ClockDriver;
class FieldSinks;
class QLabel;
class QPushButton;
class SourcePicker;

// Phase 3 panel: the clock, start/stop, arrows, reset and the Text source the clock writes to.
// The full panel is designed in Phase 5.
class ScoreboardDock : public QWidget {
    Q_OBJECT
public:
    ScoreboardDock(ClockDriver* driver, FieldSinks* sinks, QWidget* parent = nullptr);

private:
    void showFields(const sb::FieldValues& fields);
    void showRunning(bool running);
    void showStatus();
    void chooseSource();

    QPointer<ClockDriver> driver_;
    QPointer<FieldSinks> sinks_;
    QLabel* clock_ = nullptr;
    QPushButton* startStop_ = nullptr;
    SourcePicker* source_ = nullptr;
    QLabel* status_ = nullptr;
    QString shownClock_;
};
