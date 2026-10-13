// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QPointer>
#include <QString>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ChaptersExport.hpp"

class ClockDriver;
class QComboBox;
class QLabel;
class QTableWidget;

// The event log (spec §5): a non-modal window listing the match events with a localized title per
// row, and the YouTube chapter export (copy or save) relative to the stream start, the recording
// start or the game time. It holds no game state: it reads the engine's event log and rebuilds on
// every command, on a replaced match and when the clock stops at its limit.
class EventLogDialog : public QDialog {
    Q_OBJECT
public:
    EventLogDialog(ClockDriver* driver, std::optional<sb::Micros> streamStart,
                   std::optional<sb::Micros> recordingStart, QWidget* parent = nullptr);

    // The chapter block as it would be copied or saved. Public for the selftest.
    std::string chaptersText() const;

private:
    void rebuild();
    QString titleFor(const sb::MatchEvent& event) const;
    QString timeFor(const sb::MatchEvent& event) const;
    QString teamFor(const sb::MatchEvent& event) const;
    sb::ChapterBase chapterBase() const;
    sb::Micros selectedStart() const;
    bool inBase(const sb::MatchEvent& event) const;
    std::vector<sb::MatchEvent> visibleEvents() const;
    sb::ChaptersResult chapters() const;
    void copyLog();
    void copyChapters();
    void saveChapters();

    QPointer<ClockDriver> driver_;
    std::optional<sb::Micros> streamStart_;
    std::optional<sb::Micros> recordingStart_;
    QComboBox* base_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* hint_ = nullptr;
    // What the table currently shows: a rebuild is skipped while these are unchanged.
    int lastBase_ = -1;
    std::size_t lastEventCount_ = static_cast<std::size_t>(-1);
    std::string lastHomeName_;
    std::string lastAwayName_;
};
