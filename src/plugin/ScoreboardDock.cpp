// SPDX-License-Identifier: GPL-2.0-or-later
#include "ScoreboardDock.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <variant>

#include "AtomicFile.hpp"
#include "ClockDriver.hpp"
#include "ClockPanel.hpp"
#include "EventLogDialog.hpp"
#include "DockStyle.hpp"
#include "FieldSinks.hpp"
#include "FileOutput.hpp"
#include "GlobalSettings.hpp"
#include "MatchJson.hpp"
#include "MatchStateSaver.hpp"
#include "PanelModel.hpp"
#include "PenaltyDialog.hpp"
#include "SettingsChange.hpp"
#include "TeamPanel.hpp"
#include "UpdateChecker.hpp"
#include "WebServer.hpp"
#include "plugin-support.h"

namespace {

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

QString reasonText(const std::string& reason) {
    const std::string key = "Reason." + reason;
    return QString::fromUtf8(obs_module_text(key.c_str()));
}

QString field(const sb::FieldValues& values, sb::FieldId id) {
    return QString::fromStdString(values[static_cast<std::size_t>(id)]);
}

constexpr int kMessageMs = 4000;

} // namespace

ScoreboardDock::ScoreboardDock(ClockDriver* driver, FieldSinks* sinks, GlobalSettings* settings,
                               MatchStateSaver* saver, QWidget* parent)
    : QWidget(parent), driver_(driver), sinks_(sinks), settings_(settings), saver_(saver) {
    setMinimumWidth(280);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    link_ = new QPushButton(this);
    dockstyle::setProp(link_, dockstyle::kText, QStringLiteral("pill"));
    connect(link_, &QPushButton::clicked, this,
            [this]() { openSettings(MatchSettingsDialog::Page::Links); });

    // The sport of the match, as plain text on the left of the header.
    sport_ = new QLabel(this);
    dockstyle::setProp(sport_, dockstyle::kText, QStringLiteral("sport"));
    sport_->setToolTip(text("Panel.Sport.Tooltip"));
    sport_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // long names are cut

    // Shown only when the update check found a newer release.
    update_ = new QPushButton(this);
    dockstyle::setProp(update_, dockstyle::kText, QStringLiteral("pill"));
    dockstyle::setProp(update_, dockstyle::kLink, QStringLiteral("ok"));
    update_->hide();
    connect(update_, &QPushButton::clicked, this,
            [this]() { openSettings(MatchSettingsDialog::Page::Updates); });

    settingsButton_ = new dockstyle::IconButton(dockstyle::Icon::Gear, this);
    dockstyle::setProp(settingsButton_, dockstyle::kRole, QStringLiteral("iconghost"));
    settingsButton_->setToolTip(text("Settings.Tooltip"));
    connect(settingsButton_, &QPushButton::clicked, this, [this]() { openSettings(); });

    clock_ = new ClockPanel(this);
    layout->addWidget(clock_);

    auto* teams = new QHBoxLayout();
    teams->setSpacing(6);
    home_ = new TeamPanel(this);
    away_ = new TeamPanel(this);
    away_->setSide(true);
    teams->addWidget(home_, 1);
    teams->addWidget(away_, 1);
    layout->addLayout(teams);

    strength_ = new QLabel(this);
    strength_->setAlignment(Qt::AlignCenter);
    dockstyle::setProp(strength_, dockstyle::kText, QStringLiteral("pillLabel"));
    strength_->hide();
    layout->addWidget(strength_, 0, Qt::AlignHCenter);

    auto* toggles = new QVBoxLayout();
    toggles->setSpacing(2);
    countdown_ = new QCheckBox(text("Panel.Countdown"), this);
    connect(countdown_, &QCheckBox::clicked, this, [this](bool checked) {
        if (!driver_) return;
        sb::MatchSettings next = driver_->engine().settings();
        next.sport.direction = checked ? sb::Direction::Down : sb::Direction::Up;
        const std::string problem = sb::settingsProblem(next); // e.g. "needsDuration"
        if (!problem.empty()) {
            const std::string key = "Settings.Problem." + problem;
            showMessage(text(key.c_str()));
        } else {
            applySettings(next);
        }
        refresh(); // puts the box back in the real state if nothing changed
    });
    toggles->addWidget(countdown_);

    runPast_ = new QCheckBox(text("Panel.RunPast"), this);
    runPast_->hide();
    connect(runPast_, &QCheckBox::clicked, this, [this](bool checked) {
        if (!driver_) return;
        sb::MatchSettings next = driver_->engine().settings();
        next.stoppage = sb::stoppageForToggle(next.sport, checked);
        applySettings(next);
        refresh();
    });
    toggles->addWidget(runPast_);
    layout->addLayout(toggles);

    // Always present, so a message appearing or clearing never moves the layout above it.
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("dockStatus"));
    status_->setWordWrap(true);
    dockstyle::setProp(status_, dockstyle::kText, QStringLiteral("status"));
    layout->addWidget(status_);

    layout->addStretch(1);

    // The secondary information sits in a bordered footer: the sport, the source links and the
    // settings.
    auto* footer = new QFrame(this);
    dockstyle::setProp(footer, dockstyle::kCard, QStringLiteral("footer"));
    auto* footerRow = new QHBoxLayout(footer);
    footerRow->setContentsMargins(8, 4, 4, 4);
    footerRow->setSpacing(6);
    footerRow->addWidget(sport_, 1);
    // Shown while a remote page is open: how many devices, and which of them can command.
    remote_ = new QPushButton(this);
    dockstyle::setProp(remote_, dockstyle::kText, QStringLiteral("pill"));
    dockstyle::setProp(remote_, dockstyle::kLink, QStringLiteral("ok"));
    remote_->hide();
    connect(remote_, &QPushButton::clicked, this,
            [this]() { openSettings(MatchSettingsDialog::Page::Remote); });
    footerRow->addWidget(remote_);
    footerRow->addWidget(update_);
    footerRow->addWidget(link_);
    footerRow->addWidget(settingsButton_);
    layout->addWidget(footer);

    messageTimer_.setSingleShot(true);
    messageTimer_.setInterval(kMessageMs);
    connect(&messageTimer_, &QTimer::timeout, this, &ScoreboardDock::clearMessage);

    connect(clock_, &ClockPanel::arrowRequested, this, [this](qint64 delta) {
        dispatch(sb::cmd::ClockAdjust{static_cast<sb::Tenths>(delta)});
    });
    connect(clock_, &ClockPanel::toggleRequested, this,
            [this]() { dispatch(sb::cmd::ClockToggle{}); });
    connect(clock_, &ClockPanel::resetRequested, this,
            [this]() { dispatch(sb::cmd::ClockReset{}); });
    connect(clock_, &ClockPanel::periodStepRequested, this, [this](int direction) {
        if (direction > 0)
            dispatch(sb::cmd::PeriodNext{});
        else
            dispatch(sb::cmd::PeriodPrev{});
    });
    connect(clock_, &ClockPanel::stoppageStepRequested, this, [this](int delta) {
        if (driver_)
            dispatch(sb::stoppageStep(driver_->engine(), delta)); // the rule is in the core
    });
    connectTeam(sb::Team::Home);
    connectTeam(sb::Team::Away);

    if (sinks_) connect(sinks_, &FieldSinks::bindingChanged, this, &ScoreboardDock::showLinkState);

    if (driver_) {
        connect(driver_, &ClockDriver::fieldsChanged, this,
                [this](const sb::FieldValues&) { refresh(); });
        connect(driver_, &ClockDriver::runningChanged, this, [this](bool) { refresh(); });
        // Some accepted commands change no text field (cancelling a queued penalty, a score
        // button the sport does not show): fieldsChanged would not fire and a stale row could
        // be clicked again (penalty.notFound). Refresh after every accepted command too.
        connect(driver_, &ClockDriver::commandApplied, this, [this]() { refresh(); });
        connect(driver_, &ClockDriver::matchReplaced, this, [this]() {
            if (penaltyDialog_) penaltyDialog_->close(); // a new match invalidates its id/options
            applyLayout();
            refresh();
        });
        applyLayout();
        applyColors();
        refresh();
    }
    showLinkState();
    if (settings_)
        dockstyle::setDensity(settings_->prefs().comfortableDock ? dockstyle::Density::Comfortable
                                                                 : dockstyle::Density::Compact);
    dockstyle::install(this);
}

TeamPanel* ScoreboardDock::panel(sb::Team team) const {
    return team == sb::Team::Home ? home_ : away_;
}

void ScoreboardDock::setWebServer(WebServer* web) {
    web_ = web;
    if (!web_) return;
    connect(web_, &WebServer::clientsChanged, this, [this]() { showRemoteClients(); });
    showRemoteClients();
}

void ScoreboardDock::showRemoteClients() {
    const int controllers = web_ ? web_->controllers() : 0;
    const int viewers = web_ ? web_->viewers() : 0;
    const int total = controllers + viewers;
    remote_->setVisible(total > 0);
    if (total == 0) return;
    remote_->setText(text("Panel.Remote").arg(total));
    remote_->setToolTip(text("Panel.Remote.Tip").arg(controllers).arg(viewers));
    // Green while someone can command, gray when everyone only watches.
    dockstyle::setProp(remote_, dockstyle::kLink,
                       controllers > 0 ? QStringLiteral("ok") : QStringLiteral("none"));
}

void ScoreboardDock::setUpdateChecker(UpdateChecker* checker) {
    updater_ = checker;
    if (!updater_) return;
    connect(updater_, &UpdateChecker::updateAvailable, this, &ScoreboardDock::showUpdate);
}

void ScoreboardDock::connectTeam(sb::Team team) {
    TeamPanel* p = panel(team);
    connect(p, &TeamPanel::statDelta, this, [this, team](int stat, int delta) {
        dispatch(sb::cmd::AddStat{team, static_cast<sb::Stat>(stat), delta});
    });
    connect(p, &TeamPanel::nameEdited, this, [this, team](const QString& name) {
        if (!driver_) return;
        const sb::MatchSettings& s = driver_->engine().settings();
        const std::string& current = team == sb::Team::Home ? s.homeName : s.awayName;
        if (name.toStdString() == current) return; // focus loss without an edit: no command
        dispatch(sb::cmd::SetTeamName{team, name.toStdString()});
    });
    connect(p, &TeamPanel::penaltyCancelRequested, this, [this](unsigned id) {
        dispatch(sb::cmd::PenaltyCancel{static_cast<sb::PenaltyId>(id)});
    });
    connect(p, &TeamPanel::penaltyAddRequested, this, [this, team]() { addPenalty(team); });
    connect(p, &TeamPanel::penaltyEditRequested, this,
            [this, team](unsigned id) { editPenalty(team, id); });
}

void ScoreboardDock::dispatch(const sb::Command& command) {
    if (!driver_) return;
    sb::CommandResult result = driver_->apply(command);
    if (result.status == sb::CommandResult::Status::NeedsConfirmation) {
        // The question runs a nested event loop: the clock keeps going and the driver may have
        // been shut down meanwhile.
        const auto answer =
            QMessageBox::question(this, text("Panel.ConfirmTitle"), reasonText(result.reason),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes || !driver_) return;
        sb::Command confirmed = command;
        if (auto* next = std::get_if<sb::cmd::PeriodNext>(&confirmed))
            next->confirmed = true;
        else if (auto* prev = std::get_if<sb::cmd::PeriodPrev>(&confirmed))
            prev->confirmed = true;
        result = driver_->apply(confirmed);
    }
    if (result.isOk())
        clearMessage();
    else
        showMessage(reasonText(result.reason));
}

void ScoreboardDock::showMessage(const QString& message) {
    if (status_->text() != message) status_->setText(message);
    messageTimer_.start();
}

void ScoreboardDock::clearMessage() {
    if (outputFailing_) return; // the file output error stays until the folder is fixed
    if (!status_->text().isEmpty()) status_->setText(QString());
}

void ScoreboardDock::applyLayout() {
    if (!driver_) return;
    const sb::PanelLayout layout = sb::panelLayout(driver_->engine().settings());
    if (layout_ && *layout_ == layout) return;
    layout_ = layout;
    home_->applyLayout(layout);
    away_->applyLayout(layout);
    if (strengthShown_ != layout.strength) {
        strengthShown_ = layout.strength;
        strength_->setVisible(strengthShown_);
    }
}

void ScoreboardDock::applyColors() {
    const sb::GlobalPrefs empty;
    const sb::GlobalPrefs& prefs = settings_ ? settings_->prefs() : empty;
    const QString score = QString::fromStdString(
        prefs.scoreColor.empty() ? std::string(sb::kDefaultScoreColor) : prefs.scoreColor);
    const QString foul = QString::fromStdString(
        prefs.foulColor.empty() ? std::string(sb::kDefaultFoulColor) : prefs.foulColor);
    home_->setColors(score, foul);
    away_->setColors(score, foul);
    home_->setAccent(QString::fromStdString(prefs.homeColor));
    away_->setAccent(QString::fromStdString(prefs.awayColor));
    clock_->setPeriodBar(prefs.periodBar, QString::fromStdString(prefs.periodBarColor));
}

void ScoreboardDock::showLinkState() {
    if (!sinks_) return;
    int bound = 0;
    bool broken = false;
    for (std::size_t i = 0; i < sb::kFieldCount; ++i) {
        const TextSink& sink = sinks_->sink(static_cast<sb::FieldId>(i));
        if (sink.bound())
            ++bound;
        else if (sink.problem() != TextSink::Problem::None)
            broken = true;
    }
    const bool clockBound = sinks_->sink(sb::FieldId::Clock).bound();
    const int state = broken ? 1 : (clockBound ? 0 : 2); // 0 green, 1 yellow, 2 gray
    const QString label = QStringLiteral("● %1/%2").arg(bound).arg(sb::kFieldCount);
    if (link_->text() != label) link_->setText(label);
    if (state == shownLinkState_) return;
    shownLinkState_ = state;
    static const char* levels[] = {"ok", "warn", "none"};
    static const char* tips[] = {"Panel.Link.Ok", "Panel.Link.Broken", "Panel.Link.None"};
    dockstyle::setProp(link_, dockstyle::kLink, QString::fromLatin1(levels[state]));
    link_->setToolTip(text(tips[state]));
}

void ScoreboardDock::refresh() {
    if (!driver_) return;
    const sb::MatchEngine& engine = driver_->engine();
    const sb::FieldValues& values = driver_->fields();
    clock_->setClock(field(values, sb::FieldId::Clock));
    clock_->setPeriod(field(values, sb::FieldId::Period), engine.settings().sport.periods);
    clock_->setStoppage(layout_ && layout_->stoppage, engine.stoppageAnnounced());
    {
        const sb::Clock& c = engine.clock();
        const sb::Direction dir = engine.settings().sport.direction;
        clock_->setPeriodProgress(sb::shareLeft(c.value(), c.duration(), dir));
        const sb::GlobalPrefs none;
        const sb::GlobalPrefs& prefs = settings_ ? settings_->prefs() : none;
        clock_->setDigitColor(QString::fromStdString(
            sb::clockColorAt(prefs.clockColor, sb::timeLeft(c.value(), c.duration(), dir))));
        const QString sportName = text(("Sport." + engine.settings().sport.id).c_str());
        if (shownSport_ != sportName) {
            shownSport_ = sportName;
            sport_->setText(sportName);
        }
    }
    clock_->setRunning(engine.clock().running());
    clock_->setResetEnabled(!engine.clock().running());
    for (sb::Team team : {sb::Team::Home, sb::Team::Away}) {
        TeamPanel* p = panel(team);
        const sb::Counters& c = engine.counters();
        p->setName(QString::fromStdString(team == sb::Team::Home ? engine.settings().homeName
                                                                 : engine.settings().awayName));
        p->setCounts(c.get(team, sb::Stat::Score), c.get(team, sb::Stat::Shots),
                     c.get(team, sb::Stat::Fouls), c.get(team, sb::Stat::Fouls2));
        p->setPenalties(sb::penaltyRows(engine.penalties(team)));
    }
    const QString strength = field(values, sb::FieldId::Strength);
    if (strength_->text() != strength) strength_->setText(strength);
    // An empty strength (everyone on the ice) shows no pill at all.
    const bool strengthVisible = strengthShown_ && !strength.isEmpty();
    if (strength_->isVisibleTo(this) != strengthVisible) strength_->setVisible(strengthVisible);

    const bool started = sb::matchStarted(engine);
    const bool down = engine.settings().sport.direction == sb::Direction::Down;
    if (countdown_->isChecked() != down) countdown_->setChecked(down);
    if (countdown_->isEnabled() == started) countdown_->setEnabled(!started);
    const QString lockedTip = started ? text("Panel.DirectionLocked") : QString();
    if (countdown_->toolTip() != lockedTip) countdown_->setToolTip(lockedTip);

    const bool wantRunPast = layout_ && layout_->stoppage;
    if (runPastShown_ != wantRunPast) {
        runPastShown_ = wantRunPast;
        runPast_->setVisible(wantRunPast);
    }
    const bool runPast = engine.settings().stoppage == sb::StoppageMode::RunPastDuration;
    if (runPast_->isChecked() != runPast) runPast_->setChecked(runPast);

    if (outputFailing_) {
        showMessage(outputError_); // the I/O error stays until the output recovers
    } else if (!outputError_.isEmpty() && status_->text() == outputError_) {
        outputError_.clear();
        clearMessage();
    }
}

bool ScoreboardDock::applySettings(const sb::MatchSettings& next) {
    if (!driver_) return true;
    const sb::MatchSettings current = driver_->engine().settings();
    switch (sb::classifySettingsChange(current, next, sb::matchStarted(driver_->engine()))) {
    case sb::SettingsChange::None:
        return true;
    case sb::SettingsChange::Live: {
        // Rebuilding from a snapshot leaves the clock stopped: stop it exactly now (this also
        // advances penalties), rebuild, and restart it if it was running.
        const bool wasRunning = driver_->engine().clock().running();
        if (wasRunning) driver_->apply(sb::cmd::ClockStop{});
        driver_->replaceMatch(sb::withSettings(driver_->engine().snapshot(), next));
        if (wasRunning) driver_->apply(sb::cmd::ClockStart{});
        break;
    }
    case sb::SettingsChange::ReplaceNow:
        driver_->replaceMatch(sb::MatchEngine(next));
        break;
    case sb::SettingsChange::NeedsNewMatch: {
        const auto answer = QMessageBox::question(
            this, text("Panel.ConfirmTitle"), text("Settings.NewMatchQuestion"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return false;
        if (!driver_) return true; // OBS closed while the question was open
        driver_->replaceMatch(sb::MatchEngine(next));
        break;
    }
    }
    if (saver_) saver_->arm();
    if (settings_) settings_->rememberSettings(next);
    return true;
}

void ScoreboardDock::openEventLog() {
    if (!driver_) return;
    if (eventLog_) {
        eventLog_->raise();
        eventLog_->activateWindow();
        return;
    }
    const StreamStarts starts = streamStartsProvider_ ? streamStartsProvider_() : StreamStarts{};
    auto* dialog = new EventLogDialog(driver_, starts.stream, starts.recording, this);
    eventLog_ = dialog;
    dialog->setModal(false);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void ScoreboardDock::setStreamStartsProvider(std::function<StreamStarts()> provider) {
    streamStartsProvider_ = std::move(provider);
}

void ScoreboardDock::showUpdate(const QString& version, const QString& url, const QString& notes) {
    updateVersion_ = version;
    updateUrl_ = url;
    updateNotes_ = notes;
    update_->setText(text("Panel.UpdatePill").arg(version));
    update_->setToolTip(text("Panel.UpdatePill.Tooltip"));
    update_->show();
    showMessage(text("Update.Available").arg(version));
}

void ScoreboardDock::setFileOutput(FileOutput* output) {
    output_ = output;
    if (!output_) return;
    connect(output_, &FileOutput::outputError, this, [this](const QString& message) {
        outputFailing_ = true;
        outputError_ = message;
        showMessage(message);
    });
    connect(output_, &FileOutput::outputRecovered, this, [this]() {
        outputFailing_ = false; // refresh() clears the message on the next pass
        refresh();
    });
    output_->apply(settings_ ? settings_->prefs() : sb::GlobalPrefs{});
    if (driver_) output_->publish(driver_->fields());
}

void ScoreboardDock::addPenalty(sb::Team team) {
    if (!driver_) return;
    if (penaltyDialog_) {
        penaltyDialog_->raise();
        penaltyDialog_->activateWindow();
        return;
    }
    const sb::MatchSettings& settings = driver_->engine().settings();
    auto* dialog =
        new PenaltyDialog(settings.sport.penaltyOptions, settings.penaltyLabelFormat, this);
    connect(dialog, &PenaltyDialog::addRequested, this,
            [this, team](int index, const QString& player) {
                if (!driver_) return;
                // Re-read now: while the window was open the clock kept running and the sport
                // could have been replaced from the match settings dialog.
                const std::vector<sb::PenaltyOption>& options =
                    driver_->engine().settings().sport.penaltyOptions;
                if (index < 0 || static_cast<std::size_t>(index) >= options.size()) return;
                dispatch(sb::cmd::PenaltyAdd{team, player.toStdString(),
                                             options[static_cast<std::size_t>(index)].phases});
            });
    penaltyDialog_ = dialog;
    dialog->setModal(false);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void ScoreboardDock::editPenalty(sb::Team team, unsigned id) {
    if (!driver_) return;
    if (penaltyDialog_) {
        penaltyDialog_->raise();
        penaltyDialog_->activateWindow();
        return;
    }
    const sb::Penalty* penalty =
        driver_->engine().penalties(team).find(static_cast<sb::PenaltyId>(id));
    if (!penalty) {
        showMessage(reasonText("penalty.notFound"));
        return;
    }
    const sb::Tenths next =
        penalty->hasNextPhase() ? penalty->phases[penalty->phase + 1].duration : 0;
    auto* dialog =
        new PenaltyDialog(id, QString::fromStdString(penalty->player), penalty->remaining, next,
                          driver_->engine().settings().penaltyLabelFormat, this);
    connect(dialog, &PenaltyDialog::editRequested, this, [this](unsigned penaltyId, qint64 tenths) {
        if (!driver_) return;
        // By id at the moment of the click: the penalty may have expired or been promoted.
        dispatch(sb::cmd::PenaltyEdit{static_cast<sb::PenaltyId>(penaltyId),
                                      static_cast<sb::Tenths>(tenths)});
    });
    penaltyDialog_ = dialog;
    dialog->setModal(false);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void ScoreboardDock::openSettings(std::optional<MatchSettingsDialog::Page> page) {
    if (!driver_) return;
    if (settingsDialog_) {
        if (page) settingsDialog_->showPage(*page);
        settingsDialog_->raise();
        settingsDialog_->activateWindow();
        return;
    }
    const sb::GlobalPrefs none;
    MatchSettingsDialog::Services services;
    services.sinks = sinks_;
    services.updater = updater_;
    services.newMatch = [this]() { newMatch(); };
    services.resumePrevious = [this]() { resumePrevious(); };
    services.endMatch = [this]() { endMatch(); };
    services.eventLog = [this]() { openEventLog(); };
    services.web = web_;
    services.setRemote = [this](sb::RemoteMode mode, int port) {
        if (!settings_) return;
        settings_->setRemote(mode, port);
        if (web_) web_->apply();
    };
    services.updateVersion = updateVersion_;
    services.updateUrl = updateUrl_;
    services.updateNotes = updateNotes_;
    settingsDialog_ = new MatchSettingsDialog(
        driver_->engine().settings(), settings_ ? settings_->prefs() : none,
        [this](const MatchSettingsDialog::Result& r) { return applyDialogResult(r); },
        std::move(services), this);
    if (page) settingsDialog_->showPage(*page);
    settingsDialog_->show();
}

bool ScoreboardDock::applyDialogResult(const MatchSettingsDialog::Result& r) {
    if (!driver_) return true;
    sb::MatchSettings next = r.settings;
    // The clock kept running while the dialog was open: names the user did not touch come from
    // the engine as it is now.
    const sb::MatchSettings& current = driver_->engine().settings();
    if (!r.homeNameEdited) next.homeName = current.homeName;
    if (!r.awayNameEdited) next.awayName = current.awayName;
    if (!applySettings(next)) return false; // NO to "start a new match": nothing is saved
    if (settings_) {
        settings_->setColors(r.scoreColor, r.foulColor);
        settings_->setTeamColors(r.homeColor, r.awayColor);
        settings_->setPeriodBar(r.periodBar, r.periodBarColor);
        settings_->setClockColor(r.clockColor);
        settings_->setComfortableDock(r.comfortableDock);
        settings_->setUpdateCheck(r.updateCheck);
        dockstyle::setDensity(r.comfortableDock ? dockstyle::Density::Comfortable
                                                : dockstyle::Density::Compact);
        settings_->setFileOutput(r.fileOutputEnabled, r.fileOutputDir);
        if (output_) {
            output_->apply(settings_->prefs());
            if (driver_) output_->publish(driver_->fields());
        }
    }
    applyColors();
    refresh(); // keeps the file output menu entry in sync
    return true;
}

void ScoreboardDock::offerSportChoice() {
    if (!driver_ || !settings_ || settings_->prefs().sportChosen || sportPicker_) return;
    QStringList names;
    for (const sb::SportPreset& sport : sb::builtInSports()) {
        const std::string key = "Sport." + sport.id;
        names << text(key.c_str());
    }
    auto* dialog = new QInputDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(text("Sport.PickTitle"));
    dialog->setLabelText(text("Sport.PickLabel"));
    dialog->setOption(QInputDialog::UseListViewForComboBoxItems);
    dialog->setComboBoxItems(names);
    const std::string currentKey = "Sport." + driver_->engine().settings().sport.id;
    dialog->setTextValue(text(currentKey.c_str()));
    connect(dialog, &QInputDialog::textValueSelected, this, [this, names](const QString& chosen) {
        const int index = names.indexOf(chosen);
        if (index < 0 || !driver_ || !settings_) return;
        const sb::SportPreset& preset = sb::builtInSports()[static_cast<std::size_t>(index)];
        sb::MatchSettings next = sb::settingsForSport(settings_->prefs(), preset);
        const sb::MatchSettings& current = driver_->engine().settings();
        next.homeName = current.homeName; // names typed in the meantime survive
        next.awayName = current.awayName;
        applySettings(next);
        settings_->rememberSettings(next); // marks the sport as chosen even if nothing changed
    });
    connect(dialog, &QDialog::rejected, this, [this]() {
        if (driver_ && settings_) settings_->rememberSettings(driver_->engine().settings());
    });
    sportPicker_ = dialog;
    dialog->open();
}

QStringList ScoreboardDock::shownRows() const {
    QStringList rows;
    for (const char* id : {"shots", "fouls", "fouls2", "penalties"})
        if (home_->rowShown(id)) rows << QString::fromLatin1(id);
    if (strengthShown_) rows << QStringLiteral("strength");
    if (layout_ && layout_->stoppage) rows << QStringLiteral("stoppage");
    return rows;
}

std::size_t ScoreboardDock::penaltyRowCount(sb::Team team) const {
    return panel(team)->penaltyRowCount();
}

bool ScoreboardDock::penaltyAddShown(sb::Team team) const {
    return panel(team)->penaltyAddShown();
}

void ScoreboardDock::resumePrevious() {
    if (!driver_) return;
    const auto answer = QMessageBox::question(this, text("Resume.Title"), text("Resume.Question"),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    const auto content = readFile(moduleConfigPath("previous-match.json"));
    if (!content) {
        QMessageBox::information(this, text("Resume.Title"), text("Resume.NoFile"));
        return;
    }
    std::string error;
    const auto snapshot = sb::snapshotFromJson(*content, &error);
    if (!snapshot) {
        QMessageBox::warning(this, text("Resume.Title"),
                             text("Resume.Unreadable").arg(QString::fromStdString(error)));
        return;
    }
    // The modal dialog ran a nested event loop: the driver may have been shut down meanwhile.
    if (!driver_) return;
    driver_->replaceMatch(sb::MatchEngine::fromSnapshot(*snapshot));
    if (saver_) saver_->arm();
    if (settings_) settings_->setLastSport(snapshot->settings.sport.id);
    showMessage(text("Resume.Done"));
}

void ScoreboardDock::newMatch() {
    if (!driver_) return;
    if (sb::matchStarted(driver_->engine())) {
        const auto answer =
            QMessageBox::question(this, text("Panel.ConfirmTitle"), text("Menu.NewMatchQuestion"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes || !driver_) return; // the question ran a nested loop
    }
    // Same settings and team names, everything else back to the start.
    driver_->replaceMatch(sb::MatchEngine(driver_->engine().settings()));
    if (saver_) saver_->arm();
}

bool ScoreboardDock::newMatchForSport(const std::string& sportId) {
    if (!driver_) return false;
    const sb::SportPreset* preset = sb::findSport(sportId);
    if (!preset) return false;
    const sb::GlobalPrefs none;
    sb::MatchSettings next = sb::settingsForSport(settings_ ? settings_->prefs() : none, *preset);
    const sb::MatchSettings& current = driver_->engine().settings();
    next.homeName = current.homeName; // names typed by the user survive, like the sport picker
    next.awayName = current.awayName;
    driver_->replaceMatch(sb::MatchEngine(next)); // matchReplaced closes an open penalty window
    if (saver_) saver_->arm();
    if (settings_) settings_->rememberSettings(next);
    return true;
}

void ScoreboardDock::endMatch() {
    if (!driver_) return;
    const auto answer =
        QMessageBox::question(this, text("Panel.ConfirmTitle"), text("Menu.EndMatchQuestion"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    dispatch(sb::cmd::EndMatch{}); // dispatch() rechecks driver_ and reports a rejection
}
