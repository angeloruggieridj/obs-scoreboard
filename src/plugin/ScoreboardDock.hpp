// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <optional>

#include "MatchEngine.hpp"
#include "PanelLayout.hpp"

class BindingsDialog;
class ClockDriver;
class ClockPanel;
class FieldSinks;
class GlobalSettings;
class MatchStateSaver;
class QLabel;
class TeamPanel;

// The dock: composes the clock block and the two team columns and is the single entry point of
// every command (buttons, menu, hotkeys). It decides nothing about sports: it shows what
// sb::panelLayout() says and sends the commands the user asks for.
class ScoreboardDock : public QWidget {
    Q_OBJECT
public:
    ScoreboardDock(ClockDriver* driver, FieldSinks* sinks, GlobalSettings* settings,
                   MatchStateSaver* saver, QWidget* parent = nullptr);

    // Applies the command; asks the confirmation the engine requests; reports a rejection in the
    // status line. Safe to call while a modal window of this dock is open.
    void dispatch(const sb::Command& command);

private:
    TeamPanel* panel(sb::Team team) const;
    void connectTeam(sb::Team team);
    void refresh();     // values, on every fieldsChanged
    void applyLayout(); // rows and labels, on every matchReplaced
    void applyColors();
    void showMessage(const QString& message);
    void clearMessage();
    void resumePrevious();
    void openBindings();

    QPointer<ClockDriver> driver_;
    QPointer<FieldSinks> sinks_;
    GlobalSettings* settings_ = nullptr; // lives for the whole module
    QPointer<MatchStateSaver> saver_;    // deleted in tearDown() while the dock still exists
    QPointer<BindingsDialog> bindings_;
    ClockPanel* clock_ = nullptr;
    TeamPanel* home_ = nullptr;
    TeamPanel* away_ = nullptr;
    QLabel* strength_ = nullptr;
    QLabel* status_ = nullptr;
    QTimer messageTimer_;
    std::optional<sb::PanelLayout> layout_;
    bool strengthShown_ = false;
};
