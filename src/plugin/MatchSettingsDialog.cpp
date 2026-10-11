// SPDX-License-Identifier: GPL-2.0-or-later
#include "MatchSettingsDialog.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

#include "PanelModel.hpp"
#include "SettingsChange.hpp"
#include "SportCatalog.hpp"

namespace {

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

QSpinBox* makeSpin(int min, int max, QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(min, max);
    return spin;
}

} // namespace

MatchSettingsDialog::MatchSettingsDialog(const sb::MatchSettings& current,
                                         const sb::GlobalPrefs& prefs, Applier applier,
                                         QWidget* parent)
    : QDialog(parent), prefs_(prefs), base_(current), openedHome_(current.homeName),
      openedAway_(current.awayName), applier_(std::move(applier)) {
    setWindowTitle(text("Settings.Title"));
    setAttribute(Qt::WA_DeleteOnClose);
    auto* outer = new QVBoxLayout(this);
    tabs_ = new QTabWidget(this);
    auto* matchPage = new QWidget(tabs_);
    buildMatchTab(matchPage);
    tabs_->addTab(matchPage, text("Settings.Tab.Match"));
    outer->addWidget(tabs_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &MatchSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    loadFrom(current, false);
}

void MatchSettingsDialog::buildMatchTab(QWidget* page) {
    auto* form = new QFormLayout(page);

    sport_ = new QComboBox(page);
    for (const sb::SportPreset& s : sb::builtInSports()) {
        const std::string key = "Sport." + s.id;
        sport_->addItem(QString::fromUtf8(obs_module_text(key.c_str())),
                        QString::fromStdString(s.id));
    }
    connect(sport_, &QComboBox::activated, this, [this](int) { sportActivated(); });
    form->addRow(text("Settings.Sport"), sport_);

    home_ = new QLineEdit(page);
    away_ = new QLineEdit(page);
    form->addRow(text("Settings.HomeName"), home_);
    form->addRow(text("Settings.AwayName"), away_);

    periods_ = makeSpin(1, 20, page);
    form->addRow(text("Settings.Periods"), periods_);

    auto* duration = new QHBoxLayout();
    durationMinutes_ = makeSpin(0, 240, page);
    durationSeconds_ = makeSpin(0, 59, page);
    duration->addWidget(durationMinutes_);
    duration->addWidget(new QLabel(text("Settings.Minutes"), page));
    duration->addWidget(durationSeconds_);
    duration->addWidget(new QLabel(text("Settings.Seconds"), page));
    form->addRow(text("Settings.Duration"), duration);

    direction_ = new QComboBox(page);
    direction_->addItem(text("Settings.Direction.Up"), static_cast<int>(sb::Direction::Up));
    direction_->addItem(text("Settings.Direction.Down"), static_cast<int>(sb::Direction::Down));
    form->addRow(text("Settings.Direction"), direction_);

    auto* overtime = new QHBoxLayout();
    overtimeUnlimited_ = new QCheckBox(text("Settings.OvertimeUnlimited"), page);
    overtimeCount_ = makeSpin(0, 9, page);
    connect(overtimeUnlimited_, &QCheckBox::toggled, overtimeCount_,
            [this](bool unlimited) { overtimeCount_->setEnabled(!unlimited); });
    overtime->addWidget(overtimeCount_);
    overtime->addWidget(overtimeUnlimited_);
    form->addRow(text("Settings.Overtime"), overtime);

    overtimeMinutes_ = makeSpin(0, 120, page);
    form->addRow(text("Settings.OvertimeMinutes"), overtimeMinutes_);

    players_ = makeSpin(0, 99, page);
    minPlayers_ = makeSpin(0, 99, page);
    connect(players_, qOverload<int>(&QSpinBox::valueChanged), minPlayers_,
            [this](int value) { minPlayers_->setMaximum(value); });
    form->addRow(text("Settings.Players"), players_);
    form->addRow(text("Settings.MinPlayers"), minPlayers_);

    scoreValues_ = new QLineEdit(page);
    scoreValues_->setToolTip(text("Settings.ScoreButtonsHelp"));
    form->addRow(text("Settings.ScoreButtons"), scoreValues_);

    auto* defaults = new QPushButton(text("Settings.Defaults"), page);
    connect(defaults, &QPushButton::clicked, this, &MatchSettingsDialog::resetToSportDefaults);
    form->addRow(defaults);
}

std::string MatchSettingsDialog::currentSportId() const {
    return sport_->currentData().toString().toStdString();
}

void MatchSettingsDialog::loadFrom(const sb::MatchSettings& s, bool keepNames) {
    base_ = s;
    const int index = sport_->findData(QString::fromStdString(s.sport.id));
    if (index >= 0) sport_->setCurrentIndex(index);
    if (!keepNames) {
        home_->setText(QString::fromStdString(s.homeName));
        away_->setText(QString::fromStdString(s.awayName));
    }
    periods_->setValue(s.sport.periods);
    durationMinutes_->setValue(static_cast<int>(s.sport.periodDuration / sb::kTenthsPerMinute));
    durationSeconds_->setValue(
        static_cast<int>((s.sport.periodDuration % sb::kTenthsPerMinute) / sb::kTenthsPerSecond));
    direction_->setCurrentIndex(direction_->findData(static_cast<int>(s.sport.direction)));
    const bool unlimited = s.sport.overtimePeriods == sb::kUnlimited;
    overtimeUnlimited_->setChecked(unlimited);
    overtimeCount_->setEnabled(!unlimited);
    overtimeCount_->setValue(unlimited ? 0 : std::min(9, s.sport.overtimePeriods));
    overtimeMinutes_->setValue(static_cast<int>(s.sport.overtimeDuration / sb::kTenthsPerMinute));
    players_->setValue(s.sport.playersPerSide);
    minPlayers_->setMaximum(s.sport.playersPerSide);
    minPlayers_->setValue(s.sport.minPlayers);
    scoreValues_->setText(QString::fromStdString(sb::formatScoreValues(s.sport.scoreValues)));
}

void MatchSettingsDialog::sportActivated() {
    const sb::SportPreset* preset = sb::findSport(currentSportId());
    if (!preset) return;
    loadFrom(sb::settingsForSport(prefs_, *preset), true);
}

void MatchSettingsDialog::resetToSportDefaults() {
    const sb::SportPreset* preset = sb::findSport(currentSportId());
    if (!preset) return;
    loadFrom(sb::MatchSettings::forSport(*preset), true);
}

bool MatchSettingsDialog::collect(Result& out, QString* problem) const {
    sb::MatchSettings s = base_;
    s.homeName = home_->text().toStdString();
    s.awayName = away_->text().toStdString();
    s.sport.periods = periods_->value();
    s.sport.periodDuration =
        sb::minutes(durationMinutes_->value()) + sb::seconds(durationSeconds_->value());
    s.sport.direction = static_cast<sb::Direction>(direction_->currentData().toInt());
    s.sport.overtimePeriods =
        overtimeUnlimited_->isChecked() ? sb::kUnlimited : overtimeCount_->value();
    s.sport.overtimeDuration = sb::minutes(overtimeMinutes_->value());
    s.sport.playersPerSide = players_->value();
    s.sport.minPlayers = minPlayers_->value(); // the spin's maximum already follows the players
    const auto values = sb::parseScoreValues(scoreValues_->text().toStdString());
    if (!values) {
        *problem = text("Settings.BadScoreValues");
        return false;
    }
    s.sport.scoreValues = *values;
    if (const std::string why = sb::settingsProblem(s); !why.empty()) { // rules live in the core
        const std::string key = "Settings.Problem." + why;
        *problem = text(key.c_str());
        return false;
    }
    out.settings = std::move(s);
    out.homeNameEdited = out.settings.homeName != openedHome_;
    out.awayNameEdited = out.settings.awayName != openedAway_;
    out.scoreColor = prefs_.scoreColor;
    out.foulColor = prefs_.foulColor;
    return true;
}

void MatchSettingsDialog::accept() {
    Result result;
    QString problem;
    if (!collect(result, &problem)) {
        QMessageBox::warning(this, text("Settings.Title"), problem);
        return;
    }
    if (!applier_ || applier_(result)) QDialog::accept();
}
