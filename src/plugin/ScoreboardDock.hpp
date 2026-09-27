// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>

// The scoreboard panel. Empty in Phase 1: it only proves the dock is registered.
class ScoreboardDock : public QWidget {
    Q_OBJECT
public:
    explicit ScoreboardDock(QWidget* parent = nullptr);
};
