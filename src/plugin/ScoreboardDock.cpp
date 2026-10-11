// SPDX-License-Identifier: GPL-2.0-or-later
#include "ScoreboardDock.hpp"

#include <obs-module.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <variant>

#include "AtomicFile.hpp"
#include "BindingsDialog.hpp"
#include "ClockDriver.hpp"
#include "ClockPanel.hpp"
#include "FieldSinks.hpp"
#include "GlobalSettings.hpp"
#include "MatchJson.hpp"
#include "MatchStateSaver.hpp"
#include "PanelModel.hpp"
#include "TeamPanel.hpp"

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
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);

    clock_ = new ClockPanel(content);
    layout->addWidget(clock_);

    auto* teams = new QHBoxLayout();
    home_ = new TeamPanel(content);
    away_ = new TeamPanel(content);
    teams->addWidget(home_, 1);
    teams->addWidget(away_, 1);
    layout->addLayout(teams);

    strength_ = new QLabel(content);
    strength_->setAlignment(Qt::AlignCenter);
    QFont big = strength_->font();
    big.setBold(true);
    big.setPointSizeF(big.pointSizeF() * 1.4);
    strength_->setFont(big);
    strength_->hide();
    layout->addWidget(strength_);

    status_ = new QLabel(content);
    status_->setWordWrap(true);
    layout->addWidget(status_);

    auto* resume = new QPushButton(text("Dock.Resume"), content);
    connect(resume, &QPushButton::clicked, this, &ScoreboardDock::resumePrevious);
    layout->addWidget(resume);
    auto* bindings = new QPushButton(text("Dock.Bindings"), content);
    connect(bindings, &QPushButton::clicked, this, &ScoreboardDock::openBindings);
    layout->addWidget(bindings);
    layout->addStretch(1);
    scroll->setWidget(content);

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

    if (driver_) {
        connect(driver_, &ClockDriver::fieldsChanged, this,
                [this](const sb::FieldValues&) { refresh(); });
        connect(driver_, &ClockDriver::runningChanged, this, [this](bool) { refresh(); });
        // Some accepted commands change no text field (cancelling a queued penalty, a score
        // button the sport does not show): fieldsChanged would not fire and a stale row could
        // be clicked again (penalty.notFound). Refresh after every accepted command too.
        connect(driver_, &ClockDriver::commandApplied, this, [this]() { refresh(); });
        connect(driver_, &ClockDriver::matchReplaced, this, [this]() {
            applyLayout();
            refresh();
        });
        applyLayout();
        applyColors();
        refresh();
    }
}

TeamPanel* ScoreboardDock::panel(sb::Team team) const {
    return team == sb::Team::Home ? home_ : away_;
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
}

void ScoreboardDock::refresh() {
    if (!driver_) return;
    const sb::MatchEngine& engine = driver_->engine();
    const sb::FieldValues& values = driver_->fields();
    clock_->setClock(field(values, sb::FieldId::Clock));
    clock_->setPeriod(field(values, sb::FieldId::Period), engine.settings().sport.periods);
    clock_->setStoppage(layout_ && layout_->stoppage, engine.stoppageAnnounced());
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
}

void ScoreboardDock::openBindings() {
    if (!sinks_) return;
    if (bindings_) {
        bindings_->raise();
        bindings_->activateWindow();
        return;
    }
    bindings_ = new BindingsDialog(sinks_, this);
    bindings_->show();
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
