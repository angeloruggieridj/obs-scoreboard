// SPDX-License-Identifier: GPL-2.0-or-later
#include "MatchSettingsDialog.hpp"
#include "DockStyle.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QListWidget>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

#include "BindingsPage.hpp"
#include "Format.hpp"
#include "PanelModel.hpp"
#include "SettingsChange.hpp"
#include "SportCatalog.hpp"
#include "UpdateChecker.hpp"
#include "plugin-support.h"

namespace {

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

QSpinBox* makeSpin(int min, int max, QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(min, max);
    return spin;
}

// A form whose fields keep the size of their content: a small number gets a small box.
QFormLayout* makeForm(QWidget* parent) {
    auto* form = new QFormLayout(parent);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(8);
    return form;
}

// A titled group of a page, with its own form.
QFormLayout* addGroup(QVBoxLayout* page, const QString& title, QWidget* parent) {
    auto* group = new QGroupBox(title, parent);
    auto* form = makeForm(group);
    page->addWidget(group);
    return form;
}

QLineEdit* makeLine(int minWidth, QWidget* parent) {
    auto* edit = new QLineEdit(parent);
    edit->setMinimumWidth(minWidth);
    return edit;
}

} // namespace

MatchSettingsDialog::MatchSettingsDialog(const sb::MatchSettings& current,
                                         const sb::GlobalPrefs& prefs, Applier applier,
                                         Services services, QWidget* parent)
    : QDialog(parent), prefs_(prefs), base_(current), openedHome_(current.homeName),
      openedAway_(current.awayName), applier_(std::move(applier)), services_(std::move(services)) {
    setWindowTitle(text("Settings.Title"));
    resize(780, 620);
    setAttribute(Qt::WA_DeleteOnClose);
    scoreColor_ = prefs_.scoreColor;
    foulColor_ = prefs_.foulColor;
    homeColor_ = prefs_.homeColor;
    awayColor_ = prefs_.awayColor;
    barColor_ = prefs_.periodBarColor;
    clockBase_ = prefs_.clockColor.base;

    auto* outer = new QVBoxLayout(this);
    auto* body = new QHBoxLayout();
    nav_ = new QListWidget(this);
    nav_->setFixedWidth(160);
    dockstyle::setProp(nav_, "sbNav", QStringLiteral("1"));
    pages_ = new QStackedWidget(this);
    // Each page scrolls by itself when the window is smaller than its content.
    const auto addPage = [this](const QString& title, QWidget* content, bool scrolls) {
        QWidget* shown = content;
        if (scrolls) {
            auto* scroll = new QScrollArea(pages_);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setWidget(content);
            shown = scroll;
        }
        pages_->addWidget(shown);
        nav_->addItem(title);
    };
    auto* matchPage = new QWidget(pages_);
    auto* textsPage = new QWidget(pages_);
    auto* lookPage = new QWidget(pages_);
    auto* outputPage = new QWidget(pages_);
    auto* actionsPage = new QWidget(pages_);
    auto* updatesPage = new QWidget(pages_);
    buildMatchTab(matchPage);
    buildTextsTab(textsPage);
    buildLookTab(lookPage);
    buildOutputTab(outputPage);
    buildActionsPage(actionsPage);
    buildUpdatesPage(updatesPage);
    addPage(text("Settings.Tab.Match"), matchPage, true);
    addPage(text("Settings.Tab.Texts"), textsPage, true);
    addPage(text("Settings.Tab.Look"), lookPage, true);
    addPage(text("Settings.Tab.Output"), outputPage, true);
    addPage(text("Settings.Tab.Links"), new BindingsPage(services_.sinks, pages_), false);
    addPage(text("Settings.Tab.Actions"), actionsPage, true);
    addPage(text("Settings.Tab.Updates"), updatesPage, true);
    connect(nav_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    nav_->setCurrentRow(0);
    body->addWidget(nav_);
    body->addWidget(pages_, 1);
    outer->addLayout(body, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &MatchSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    dockstyle::styleDialog(this);
    outer->addWidget(buttons);

    // Cross-tab connections: every widget exists now.
    connect(direction_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateEnabled(); });
    connect(durationMinutes_, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int) { updateEnabled(); });
    connect(durationSeconds_, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int) { updateEnabled(); });
    connect(penaltyFormat_, &QLineEdit::textChanged, this,
            [this](const QString&) { updatePreview(); });

    loadFrom(current, false); // LAST: fills every tab, then updateEnabled() and updatePreview()
}

void MatchSettingsDialog::buildMatchTab(QWidget* page) {
    auto* outer = new QVBoxLayout(page);

    QFormLayout* form = addGroup(outer, text("Settings.Group.Match"), page);
    sport_ = new QComboBox(page);
    for (const sb::SportPreset& s : sb::builtInSports()) {
        const std::string key = "Sport." + s.id;
        sport_->addItem(QString::fromUtf8(obs_module_text(key.c_str())),
                        QString::fromStdString(s.id));
    }
    connect(sport_, &QComboBox::activated, this, [this](int) { sportActivated(); });
    form->addRow(text("Settings.Sport"), sport_);
    home_ = makeLine(240, page);
    away_ = makeLine(240, page);
    form->addRow(text("Settings.HomeName"), home_);
    form->addRow(text("Settings.AwayName"), away_);

    form = addGroup(outer, text("Settings.Group.Time"), page);
    periods_ = makeSpin(1, 20, page);
    form->addRow(text("Settings.Periods"), periods_);
    auto* duration = new QHBoxLayout();
    durationMinutes_ = makeSpin(0, 240, page);
    durationSeconds_ = makeSpin(0, 59, page);
    duration->addWidget(durationMinutes_);
    duration->addWidget(new QLabel(text("Settings.Minutes"), page));
    duration->addWidget(durationSeconds_);
    duration->addWidget(new QLabel(text("Settings.Seconds"), page));
    duration->addStretch(1);
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
    overtime->addStretch(1);
    form->addRow(text("Settings.Overtime"), overtime);
    overtimeMinutes_ = makeSpin(0, 120, page);
    form->addRow(text("Settings.OvertimeMinutes"), overtimeMinutes_);

    form = addGroup(outer, text("Settings.Group.Players"), page);
    players_ = makeSpin(0, 99, page);
    minPlayers_ = makeSpin(0, 99, page);
    connect(players_, qOverload<int>(&QSpinBox::valueChanged), minPlayers_,
            [this](int value) { minPlayers_->setMaximum(value); });
    form->addRow(text("Settings.Players"), players_);
    form->addRow(text("Settings.MinPlayers"), minPlayers_);
    scoreValues_ = makeLine(140, page);
    scoreValues_->setToolTip(text("Settings.ScoreButtonsHelp"));
    form->addRow(text("Settings.ScoreButtons"), scoreValues_);

    auto* defaults = new QPushButton(text("Settings.Defaults"), page);
    connect(defaults, &QPushButton::clicked, this, &MatchSettingsDialog::resetToSportDefaults);
    auto* defaultsRow = new QHBoxLayout();
    defaultsRow->addWidget(defaults);
    defaultsRow->addStretch(1);
    outer->addLayout(defaultsRow);
    outer->addStretch(1);
}

void MatchSettingsDialog::buildTextsTab(QWidget* page) {
    auto* outer = new QVBoxLayout(page);

    QFormLayout* form = addGroup(outer, text("Settings.Group.Clock"), page);
    twoDigit_ = new QCheckBox(text("Settings.TwoDigit"), page);
    form->addRow(twoDigit_);
    tenths_ = new QCheckBox(text("Settings.Tenths"), page);
    form->addRow(tenths_);
    stoppage_ = new QComboBox(page);
    stoppage_->addItem(text("Settings.Stoppage.Stop"),
                       static_cast<int>(sb::StoppageMode::StopAtDuration));
    stoppage_->addItem(text("Settings.Stoppage.Counter"),
                       static_cast<int>(sb::StoppageMode::SecondaryCounter));
    stoppage_->addItem(text("Settings.Stoppage.RunPast"),
                       static_cast<int>(sb::StoppageMode::RunPastDuration));
    form->addRow(text("Settings.Stoppage"), stoppage_);
    playTime_ = new QComboBox(page);
    playTime_->addItem(text("Settings.PlayTime.MS"),
                       static_cast<int>(sb::TimeFormat::MinutesSeconds));
    playTime_->addItem(text("Settings.PlayTime.HMS"),
                       static_cast<int>(sb::TimeFormat::HoursMinutesSeconds));
    form->addRow(text("Settings.PlayTime"), playTime_);

    form = addGroup(outer, text("Settings.Group.Labels"), page);
    regulationLabels_ = makeLine(200, page);
    regulationLabels_->setToolTip(text("Settings.LabelsHelp"));
    form->addRow(text("Settings.RegulationLabels"), regulationLabels_);
    overtimeLabels_ = makeLine(200, page);
    overtimeLabels_->setToolTip(text("Settings.LabelsHelp"));
    form->addRow(text("Settings.OvertimeLabels"), overtimeLabels_);

    form = addGroup(outer, text("Settings.Group.Penalties"), page);
    penaltyFormat_ = makeLine(260, page);
    penaltyFormat_->setToolTip(text("Settings.PenaltyHelp"));
    form->addRow(text("Settings.PenaltyFormat"), penaltyFormat_);
    penaltyPreview_ = new QLabel(page);
    form->addRow(QString(), penaltyPreview_);
    strengthEven_ = makeLine(100, page);
    form->addRow(text("Settings.StrengthEven"), strengthEven_);
    strengthUneven_ = makeLine(160, page);
    form->addRow(text("Settings.StrengthUneven"), strengthUneven_);
    outer->addStretch(1);
}

QPushButton* MatchSettingsDialog::addColorRow(QFormLayout* form, const QString& label,
                                              std::string* target, const std::string& fallback) {
    auto* swatch = new QPushButton(this);
    swatch->setFixedWidth(150);
    swatchDefaults_.emplace_back(swatch, fallback);
    connect(swatch, &QPushButton::clicked, this,
            [this, target, swatch]() { pickColor(*target, swatch); });
    if (form) { // a row of the form has a "default" button; the interval rows lay out their own
        auto* reset = new QPushButton(text("Settings.Default"), this);
        auto* row = new QHBoxLayout();
        row->addWidget(swatch);
        row->addWidget(reset);
        row->addStretch(1);
        connect(reset, &QPushButton::clicked, this, [this, target, swatch]() {
            target->clear();
            showColor(swatch, *target);
        });
        form->addRow(label, row);
    }
    showColor(swatch, *target);
    return swatch;
}

void MatchSettingsDialog::buildLookTab(QWidget* page) {
    auto* outer = new QVBoxLayout(page);

    // --- Colors of the dock -----------------------------------------------------------------
    auto* colors = new QGroupBox(text("Settings.Group.Colors"), page);
    auto* colorsForm = makeForm(colors);
    scoreSwatch_ = addColorRow(colorsForm, text("Settings.ScoreColor"), &scoreColor_,
                               std::string(sb::kDefaultScoreColor));
    foulSwatch_ = addColorRow(colorsForm, text("Settings.FoulColor"), &foulColor_,
                              std::string(sb::kDefaultFoulColor));
    addColorRow(colorsForm, text("Settings.HomeColor"), &homeColor_,
                dockstyle::accentColor(QApplication::palette()).name().toUpper().toStdString());
    addColorRow(colorsForm, text("Settings.AwayColor"), &awayColor_,
                dockstyle::accentAwayColor().name().toUpper().toStdString());
    // Taller buttons and more air, for a mouse that is not as precise as the match is fast.
    density_ = new QComboBox(colors);
    density_->addItem(text("Settings.Density.Compact"), 0);
    density_->addItem(text("Settings.Density.Comfortable"), 1);
    density_->setCurrentIndex(prefs_.comfortableDock ? 1 : 0);
    colorsForm->addRow(text("Settings.Density"), density_);
    outer->addWidget(colors);

    // --- Period progress on the border of the clock frame -----------------------------------
    auto* frame = new QGroupBox(text("Settings.Group.Frame"), page);
    auto* frameForm = makeForm(frame);
    periodBar_ = new QComboBox(frame);
    periodBar_->addItem(text("Settings.PeriodBar.Off"), static_cast<int>(sb::PeriodBarMode::Off));
    periodBar_->addItem(text("Settings.PeriodBar.Remaining"),
                        static_cast<int>(sb::PeriodBarMode::Remaining));
    periodBar_->addItem(text("Settings.PeriodBar.Consumed"),
                        static_cast<int>(sb::PeriodBarMode::Consumed));
    const int barIndex = periodBar_->findData(static_cast<int>(prefs_.periodBar));
    periodBar_->setCurrentIndex(barIndex < 0 ? 1 : barIndex);
    frameForm->addRow(text("Settings.PeriodBar"), periodBar_);
    QPushButton* barSwatch =
        addColorRow(frameForm, text("Settings.PeriodBarColor"), &barColor_,
                    dockstyle::accentColor(QApplication::palette()).name().toUpper().toStdString());
    connect(periodBar_, &QComboBox::currentIndexChanged, this, [this, barSwatch](int) {
        barSwatch->setEnabled(periodBar_->currentData().toInt() !=
                              static_cast<int>(sb::PeriodBarMode::Off));
    });
    barSwatch->setEnabled(prefs_.periodBar != sb::PeriodBarMode::Off);
    outer->addWidget(frame);

    // --- Color of the clock digits ------------------------------------------------------------
    auto* digits = new QGroupBox(text("Settings.Group.Digits"), page);
    auto* digitsForm = makeForm(digits);
    clockMode_ = new QComboBox(digits);
    clockMode_->addItem(text("Settings.ClockColor.Theme"),
                        static_cast<int>(sb::ClockColorMode::Theme));
    clockMode_->addItem(text("Settings.ClockColor.Single"),
                        static_cast<int>(sb::ClockColorMode::Single));
    clockMode_->addItem(text("Settings.ClockColor.ByTime"),
                        static_cast<int>(sb::ClockColorMode::ByTime));
    clockMode_->setCurrentIndex(
        std::max(0, clockMode_->findData(static_cast<int>(prefs_.clockColor.mode))));
    digitsForm->addRow(text("Settings.ClockColor"), clockMode_);
    clockBaseSwatch_ = addColorRow(
        digitsForm, text("Settings.ClockBase"), &clockBase_,
        QApplication::palette().color(QPalette::WindowText).name().toUpper().toStdString());

    static const std::pair<int, const char*> kRuleDefaults[sb::kMaxClockColorRules] = {
        {60, "#F2C94C"}, {30, "#E5484D"}, {10, "#E5484D"}, {5, "#E5484D"}};
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        RuleRow& row = rules_[i];
        const bool saved = i < prefs_.clockColor.rules.size();
        row.on = new QCheckBox(text("Settings.ClockRule"), digits);
        row.seconds = makeSpin(0, sb::kMaxRuleSeconds, digits);
        row.seconds->setSuffix(text("Settings.ClockRuleSuffix"));
        row.seconds->setValue(saved ? prefs_.clockColor.rules[i].secondsLeft
                                    : kRuleDefaults[i].first);
        row.color = saved ? prefs_.clockColor.rules[i].color : std::string(kRuleDefaults[i].second);
        // With no saved rules the first two are on, as a starting point for "turn red at the end".
        row.on->setChecked(saved || (prefs_.clockColor.rules.empty() && i < 2));
        auto* line = new QHBoxLayout();
        line->addWidget(row.on);
        line->addWidget(row.seconds);
        row.swatch = addColorRow(nullptr, QString(), &row.color, kRuleDefaults[i].second);
        line->addWidget(row.swatch);
        line->addStretch(1);
        digitsForm->addRow(line);
        connect(row.on, &QCheckBox::toggled, this, [this](bool) { updateClockColorEnabled(); });
    }
    clockBlend_ = new QCheckBox(text("Settings.ClockBlend"), digits);
    clockBlend_->setChecked(prefs_.clockColor.blend);
    digitsForm->addRow(clockBlend_);
    connect(clockMode_, &QComboBox::currentIndexChanged, this,
            [this](int) { updateClockColorEnabled(); });
    updateClockColorEnabled();
    outer->addWidget(digits);
    outer->addStretch(1);
}

void MatchSettingsDialog::updateClockColorEnabled() {
    const auto mode = static_cast<sb::ClockColorMode>(clockMode_->currentData().toInt());
    clockBaseSwatch_->setEnabled(mode != sb::ClockColorMode::Theme);
    const bool byTime = mode == sb::ClockColorMode::ByTime;
    clockBlend_->setEnabled(byTime);
    for (RuleRow& row : rules_) {
        row.on->setEnabled(byTime);
        row.seconds->setEnabled(byTime && row.on->isChecked());
        row.swatch->setEnabled(byTime && row.on->isChecked());
    }
}

void MatchSettingsDialog::buildOutputTab(QWidget* page) {
    auto* outer = new QVBoxLayout(page);
    QFormLayout* form = addGroup(outer, text("Settings.Group.FileOutput"), page);
    fileOutput_ = new QCheckBox(text("Settings.FileOutput"), page);
    connect(fileOutput_, &QCheckBox::toggled, this, [this](bool) { refreshFileOutput(); });
    form->addRow(fileOutput_);

    fileOutputDirLabel_ = new QLabel(page);
    fileOutputDirLabel_->setWordWrap(true);
    fileOutputChoose_ = new QPushButton(text("Settings.FileOutputChoose"), page);
    connect(fileOutputChoose_, &QPushButton::clicked, this,
            &MatchSettingsDialog::chooseFileOutputDir);
    auto* row = new QHBoxLayout();
    row->setSpacing(10);
    row->addWidget(fileOutputChoose_);
    row->addWidget(fileOutputDirLabel_, 1);
    form->addRow(text("Settings.FileOutputFolder"), row);

    auto* help = new QLabel(text("Settings.FileOutputHelp"), page);
    help->setWordWrap(true);
    dockstyle::setProp(help, dockstyle::kText, QStringLiteral("muted"));
    form->addRow(help);
    outer->addStretch(1);

    fileOutput_->setChecked(prefs_.fileOutputEnabled);
    fileOutputDir_ = prefs_.fileOutputDir;
    refreshFileOutput();
}

// A page of buttons for what happens to the match as a whole.
void MatchSettingsDialog::buildActionsPage(QWidget* page) {
    auto* outer = new QVBoxLayout(page);
    auto* group = new QGroupBox(text("Settings.Group.Actions"), page);
    auto* grid = new QGridLayout(group);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);
    struct Entry {
        const char* label;
        const char* help;
        std::function<void()> action;
        bool closeFirst;
    };
    const Entry entries[] = {
        {"Menu.NewMatch", "Settings.Action.NewMatch", services_.newMatch, true},
        {"Dock.Resume", "Settings.Action.Resume", services_.resumePrevious, true},
        {"Menu.EndMatch", "Settings.Action.EndMatch", services_.endMatch, true},
        {"Menu.EventLog", "Settings.Action.EventLog", services_.eventLog, false},
    };
    int row = 0;
    for (const Entry& entry : entries) {
        auto* button = new QPushButton(text(entry.label), group);
        button->setMinimumWidth(190);
        auto* help = new QLabel(text(entry.help), group);
        help->setWordWrap(true);
        dockstyle::setProp(help, dockstyle::kText, QStringLiteral("muted"));
        connect(button, &QPushButton::clicked, this,
                [this, action = entry.action, closeFirst = entry.closeFirst]() {
                    runAction(action, closeFirst);
                });
        grid->addWidget(button, row, 0);
        grid->addWidget(help, row, 1);
        grid->setColumnStretch(1, 1);
        ++row;
    }
    outer->addWidget(group);
    outer->addStretch(1);
}

void MatchSettingsDialog::runAction(const std::function<void()>& action, bool closeFirst) {
    if (!action) return;
    const std::function<void()> copy = action; // this window may be gone once it closes
    if (closeFirst) reject();
    copy();
}

void MatchSettingsDialog::buildUpdatesPage(QWidget* page) {
    auto* outer = new QVBoxLayout(page);
    auto* group = new QGroupBox(text("Settings.Group.Updates"), page);
    auto* layout = new QVBoxLayout(group);
    auto* version = new QLabel(text("Menu.Version").arg(QString::fromUtf8(PLUGIN_VERSION)), group);
    dockstyle::setProp(version, dockstyle::kText, QStringLiteral("strong"));
    layout->addWidget(version);
    updateAtStart_ = new QCheckBox(text("Menu.UpdateCheck"), group);
    updateAtStart_->setChecked(prefs_.updateCheck);
    layout->addWidget(updateAtStart_);

    auto* row = new QHBoxLayout();
    auto* check = new QPushButton(text("Menu.UpdateNow"), group);
    updateStatus_ = new QLabel(group);
    updateStatus_->setWordWrap(true);
    updateOpen_ = new QPushButton(text("Settings.Updates.OpenPage"), group);
    updateOpen_->hide();
    row->addWidget(check);
    row->addWidget(updateStatus_, 1);
    row->addWidget(updateOpen_);
    layout->addLayout(row);

    updateNotes_ = new QTextBrowser(group);
    updateNotes_->setOpenExternalLinks(true);
    updateNotes_->setMinimumHeight(220);
    updateNotes_->hide();
    layout->addWidget(updateNotes_, 1);
    outer->addWidget(group, 1);

    connect(updateOpen_, &QPushButton::clicked, this, [this]() {
        if (!updateUrl_.isEmpty()) QDesktopServices::openUrl(QUrl(updateUrl_));
    });
    connect(check, &QPushButton::clicked, this, [this]() {
        if (!services_.updater) {
            updateStatus_->setText(text("Update.Failed"));
            return;
        }
        updateStatus_->setText(text("Update.Checking"));
        services_.updater->start(PLUGIN_VERSION, true);
    });
    if (services_.updater) {
        connect(services_.updater, &UpdateChecker::updateAvailable, this,
                [this](QString version, QString url, QString notes) {
                    showUpdateInfo(version, url, notes);
                });
        connect(services_.updater, &UpdateChecker::noUpdate, this, [this]() {
            updateStatus_->setText(text("Update.None").arg(QString::fromUtf8(PLUGIN_VERSION)));
            updateNotes_->hide();
            updateOpen_->hide();
        });
        connect(services_.updater, &UpdateChecker::failed, this,
                [this]() { updateStatus_->setText(text("Update.Failed")); });
    }
    if (!services_.updateVersion.isEmpty())
        showUpdateInfo(services_.updateVersion, services_.updateUrl, services_.updateNotes);
}

void MatchSettingsDialog::showUpdateInfo(const QString& version, const QString& url,
                                         const QString& notes) {
    updateUrl_ = url;
    updateStatus_->setText(text("Settings.Updates.Available").arg(version));
    updateOpen_->setVisible(!url.isEmpty());
    updateNotes_->setMarkdown(notes.isEmpty() ? text("Settings.Updates.NoNotes") : notes);
    updateNotes_->show();
}

void MatchSettingsDialog::showPage(Page page) {
    nav_->setCurrentRow(static_cast<int>(page));
}

void MatchSettingsDialog::refreshFileOutput() {
    const QString label = fileOutputDir_.empty() ? text("Settings.FileOutputNone")
                                                 : QString::fromStdString(fileOutputDir_);
    if (fileOutputDirLabel_->text() != label) fileOutputDirLabel_->setText(label);
    fileOutputChoose_->setEnabled(fileOutput_->isChecked());
}

void MatchSettingsDialog::chooseFileOutputDir() {
    const QString dir = QFileDialog::getExistingDirectory(this, text("Settings.FileOutputChoose"),
                                                          QString::fromStdString(fileOutputDir_));
    if (dir.isEmpty()) return;
    fileOutputDir_ = dir.toStdString();
    refreshFileOutput();
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

    regulationLabels_->setText(QString::fromStdString(sb::formatLabelList(s.regulationLabels)));
    overtimeLabels_->setText(QString::fromStdString(sb::formatLabelList(s.overtimeLabels)));
    twoDigit_->setChecked(s.twoDigitMinutes);
    tenths_->setChecked(s.tenthsInLastMinute);
    stoppage_->setCurrentIndex(stoppage_->findData(static_cast<int>(s.stoppage)));
    penaltyFormat_->setText(QString::fromStdString(s.penaltyLabelFormat));
    strengthEven_->setText(QString::fromStdString(s.strength.even));
    strengthUneven_->setText(QString::fromStdString(s.strength.uneven));
    playTime_->setCurrentIndex(playTime_->findData(static_cast<int>(s.playTimeFormat)));
    updateEnabled();
    updatePreview();
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

void MatchSettingsDialog::updateEnabled() {
    const bool up =
        static_cast<sb::Direction>(direction_->currentData().toInt()) == sb::Direction::Up;
    const bool hasDuration = durationMinutes_->value() > 0 || durationSeconds_->value() > 0;
    stoppage_->setEnabled(up && hasDuration);
    penaltyFormat_->setEnabled(base_.sport.hasPenalties());
    const bool hasStrength = base_.sport.strengthSource != sb::StrengthSource::None;
    strengthEven_->setEnabled(hasStrength);
    strengthUneven_->setEnabled(hasStrength);
}

void MatchSettingsDialog::updatePreview() {
    const QString preview = QString::fromStdString(
        sb::applyTemplate(penaltyFormat_->text().toStdString(),
                          {{"player", "24"}, {"time", "1:32"}, {"phase2", "+2:00"}}));
    if (penaltyPreview_->text() != preview) penaltyPreview_->setText(preview);
}

void MatchSettingsDialog::showColor(QPushButton* swatch, const std::string& hex) {
    // The swatch always shows the color in use: the default one, marked as such, when none was
    // picked.
    std::string fallback;
    for (const auto& entry : swatchDefaults_)
        if (entry.first == swatch) fallback = entry.second;
    const std::string effective = !hex.empty() ? hex : fallback;
    const QString label =
        hex.empty() ? QStringLiteral("%1  (%2)")
                          .arg(QString::fromStdString(effective), text("Settings.ColorDefault"))
                    : QString::fromStdString(effective);
    if (swatch->text() != label) swatch->setText(label);
    const auto rgb = sb::parseColor(effective);
    QString style;
    if (rgb)
        style =
            QStringLiteral("QPushButton{background:%1;color:%2;}")
                .arg(QString::fromStdString(effective),
                     sb::darkTextOn(*rgb) ? QStringLiteral("#111111") : QStringLiteral("#FFFFFF"));
    if (swatch->styleSheet() != style) swatch->setStyleSheet(style);
}

void MatchSettingsDialog::pickColor(std::string& target, QPushButton* swatch) {
    const auto rgb = sb::parseColor(target);
    const QColor initial = rgb ? QColor::fromRgb(*rgb) : QColor(Qt::white);
    const QColor chosen = QColorDialog::getColor(initial, this); // nested event loop: harmless here
    if (!chosen.isValid()) return;
    target = sb::formatColor(chosen.rgb() & 0xFFFFFFu);
    showColor(swatch, target);
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
    s.regulationLabels = sb::parseLabelList(regulationLabels_->text().toStdString());
    s.overtimeLabels = sb::parseLabelList(overtimeLabels_->text().toStdString());
    s.twoDigitMinutes = twoDigit_->isChecked();
    s.tenthsInLastMinute = tenths_->isChecked();
    if (stoppage_->isEnabled()) // a countdown or a sport without a length keeps the loaded mode
        s.stoppage = static_cast<sb::StoppageMode>(stoppage_->currentData().toInt());
    s.penaltyLabelFormat = penaltyFormat_->text().toStdString();
    s.strength.even = strengthEven_->text().toStdString();
    s.strength.uneven = strengthUneven_->text().toStdString();
    s.playTimeFormat = static_cast<sb::TimeFormat>(playTime_->currentData().toInt());
    if (const std::string why = sb::settingsProblem(s); !why.empty()) { // rules live in the core
        const std::string key = "Settings.Problem." + why;
        *problem = text(key.c_str());
        return false;
    }
    out.settings = std::move(s);
    out.homeNameEdited = out.settings.homeName != openedHome_;
    out.awayNameEdited = out.settings.awayName != openedAway_;
    out.scoreColor = scoreColor_;
    out.foulColor = foulColor_;
    out.comfortableDock = density_ && density_->currentData().toInt() == 1;
    out.updateCheck = updateAtStart_->isChecked();
    out.homeColor = homeColor_;
    out.awayColor = awayColor_;
    out.periodBar = static_cast<sb::PeriodBarMode>(periodBar_->currentData().toInt());
    out.periodBarColor = barColor_;
    sb::ClockColorSettings clock;
    clock.mode = static_cast<sb::ClockColorMode>(clockMode_->currentData().toInt());
    clock.blend = clockBlend_->isChecked();
    // While the base is "default" the theme's text color is the base the rules start from.
    for (const auto& entry : swatchDefaults_)
        if (entry.first == clockBaseSwatch_)
            clock.base = clockBase_.empty() ? entry.second : clockBase_;
    for (const RuleRow& row : rules_) {
        if (!row.on->isChecked()) continue;
        std::string color = row.color;
        if (color.empty()) // "default" on a rule row: the color its swatch shows
            for (const auto& entry : swatchDefaults_)
                if (entry.first == row.swatch) color = entry.second;
        clock.rules.push_back({row.seconds->value(), color});
    }
    out.clockColor = std::move(clock);
    if (fileOutput_->isChecked() && fileOutputDir_.empty()) {
        *problem = text("Settings.FileOutputNeedsFolder");
        return false;
    }
    out.fileOutputEnabled = fileOutput_->isChecked();
    out.fileOutputDir = fileOutputDir_;
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
