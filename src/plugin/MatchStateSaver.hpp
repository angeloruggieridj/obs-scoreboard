// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <string>

class ClockDriver;

// Keeps last-match.json up to date, at most every 2 s. Armed only by the user's first command
// (or a resume): opening and closing OBS without touching the scoreboard keeps the previous match.
class MatchStateSaver : public QObject {
    Q_OBJECT
public:
    MatchStateSaver(ClockDriver* driver, std::string path, QObject* parent = nullptr);
    void arm();
    void flush(); // writes now if there is something unsaved

private:
    void markDirty();

    QPointer<ClockDriver> driver_;
    std::string path_;
    QTimer timer_;
    bool armed_ = false;
    bool dirty_ = false;
};
