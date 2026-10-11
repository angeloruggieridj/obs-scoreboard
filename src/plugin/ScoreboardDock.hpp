// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <cstddef>
#include <optional>

#include "MatchEngine.hpp"
#include "MatchSettingsDialog.hpp"
#include "PanelLayout.hpp"

class BindingsDialog;
class ClockDriver;
class ClockPanel;
class FieldSinks;
class GlobalSettings;
class MatchStateSaver;
class QCheckBox;
class QInputDialog;
class QLabel;
class QPushButton;
class QToolButton;
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

    // Offers the first-launch sport choice once, unless the sport was already chosen or a picker
    // is already open. Called on OBS_FRONTEND_EVENT_FINISHED_LOADING.
    void offerSportChoice();

    // The rows the home column currently shows, in order: "shots", "fouls", "fouls2",
    // "penalties", then "strength" and "stoppage" when they are visible. Read by the selftest.
    QStringList shownRows() const;
    // How many penalty rows the given team column shows. Read by the selftest.
    std::size_t penaltyRowCount(sb::Team team) const;

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
    void openSettings();
    bool applyDialogResult(const MatchSettingsDialog::Result& result);

    // Applies `next` to the running match, classifying the change (5.3). False only when the user
    // answered NO to the "new match?" question.
    bool applySettings(const sb::MatchSettings& next);
    void showLinkState();
    void newMatch();
    void endMatch();
    void about();

    QPointer<ClockDriver> driver_;
    QPointer<FieldSinks> sinks_;
    GlobalSettings* settings_ = nullptr; // lives for the whole module
    QPointer<MatchStateSaver> saver_;    // deleted in tearDown() while the dock still exists
    QPointer<BindingsDialog> bindings_;
    QPointer<MatchSettingsDialog> settingsDialog_;
    QPointer<QInputDialog> sportPicker_;
    QPushButton* link_ = nullptr;
    QToolButton* menu_ = nullptr;
    ClockPanel* clock_ = nullptr;
    TeamPanel* home_ = nullptr;
    TeamPanel* away_ = nullptr;
    QLabel* strength_ = nullptr;
    QCheckBox* countdown_ = nullptr;
    QCheckBox* runPast_ = nullptr;
    QLabel* status_ = nullptr;
    QTimer messageTimer_;
    std::optional<sb::PanelLayout> layout_;
    bool strengthShown_ = false;
    bool runPastShown_ = false;
    int shownLinkState_ = -1;
};
