// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPointer>
#include <QString>
#include <QWidget>

#include "Fields.hpp"

class ClockDriver;
class BindingsDialog;
class FieldSinks;
class GlobalSettings;
class MatchStateSaver;
class QLabel;
class QPushButton;
class SourcePicker;

// Phase 3 panel: the clock, start/stop, arrows, reset and the Text source the clock writes to.
// The full panel is designed in Phase 5.
class ScoreboardDock : public QWidget {
    Q_OBJECT
public:
    ScoreboardDock(ClockDriver* driver, FieldSinks* sinks, GlobalSettings* settings,
                   MatchStateSaver* saver, QWidget* parent = nullptr);

private:
    void showFields(const sb::FieldValues& fields);
    void showRunning(bool running);
    void showStatus();
    void chooseSource();
    void resumePrevious();
    void openBindings();

    QPointer<ClockDriver> driver_;
    QPointer<FieldSinks> sinks_;
    GlobalSettings* settings_ = nullptr; // lives for the whole module
    QPointer<MatchStateSaver> saver_;    // deleted in tearDown() while the dock still exists
    QPointer<BindingsDialog> bindings_;
    QLabel* clock_ = nullptr;
    QPushButton* startStop_ = nullptr;
    SourcePicker* source_ = nullptr;
    QLabel* status_ = nullptr;
    QString shownClock_;
};
