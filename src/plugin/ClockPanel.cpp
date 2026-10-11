// SPDX-License-Identifier: GPL-2.0-or-later
#include "ClockPanel.hpp"

#include <obs-module.h>

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <algorithm>

#include "Types.hpp"

namespace {

// Wide enough for a "+1:00" label at the theme's button font; also used to size the clock.
constexpr int kArrowColumn = 52;

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

// A lone "-" or "+" in a narrow button is elided by the OBS theme's button padding: drop the
// padding and fix a width just wide enough for the glyph. The rest of the theme look is kept.
QPushButton* makeStepButton(const QString& label, QWidget* parent) {
    auto* button = new QPushButton(label, parent);
    button->setStyleSheet(QStringLiteral("QPushButton{padding:0px;}"));
    button->setFixedWidth(26);
    return button;
}

} // namespace

QPushButton* ClockPanel::makeArrow(const QString& label, const QString& tooltip,
                                   qint64 deltaTenths) {
    auto* button = new QPushButton(label, this);
    button->setMinimumWidth(kArrowColumn);
    button->setAutoRepeat(true); // held down: repeats (CLAUDE.md §4.3)
    button->setAutoRepeatDelay(400);
    button->setAutoRepeatInterval(100);
    button->setToolTip(tooltip);
    connect(button, &QPushButton::clicked, this,
            [this, deltaTenths]() { emit arrowRequested(deltaTenths); });
    return button;
}

ClockPanel::ClockPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(2, 2, 2, 2);
    outer->setSpacing(4);

    auto* minutesColumn = new QVBoxLayout();
    minutesColumn->setSpacing(2);
    minutesColumn->addWidget(
        makeArrow(QStringLiteral("+1:00"), text("Hotkey.clock_min_up"), sb::minutes(1)));
    minutesColumn->addWidget(
        makeArrow(QStringLiteral("-1:00"), text("Hotkey.clock_min_down"), -sb::minutes(1)));
    auto* secondsColumn = new QVBoxLayout();
    secondsColumn->setSpacing(2);
    secondsColumn->addWidget(
        makeArrow(QStringLiteral("+0:01"), text("Hotkey.clock_sec_up"), sb::seconds(1)));
    secondsColumn->addWidget(
        makeArrow(QStringLiteral("-0:01"), text("Hotkey.clock_sec_down"), -sb::seconds(1)));

    clock_ = new QLabel(this);
    clock_->setAlignment(Qt::AlignCenter);
    clock_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* top = new QHBoxLayout();
    top->addLayout(minutesColumn);
    top->addWidget(clock_, 1);
    top->addLayout(secondsColumn);
    outer->addLayout(top);

    auto* periodRow = new QHBoxLayout();
    auto* prev = makeStepButton(QStringLiteral("−"), this);
    auto* next = makeStepButton(QStringLiteral("+"), this);
    connect(prev, &QPushButton::clicked, this, [this]() { emit periodStepRequested(-1); });
    connect(next, &QPushButton::clicked, this, [this]() { emit periodStepRequested(1); });
    period_ = new QLabel(this);
    period_->setAlignment(Qt::AlignCenter);
    QFont bold = period_->font();
    bold.setBold(true);
    period_->setFont(bold);
    of_ = new QLabel(this);
    periodRow->addStretch(1);
    periodRow->addWidget(prev);
    periodRow->addWidget(period_);
    periodRow->addWidget(next);
    periodRow->addWidget(of_);

    stoppageBox_ = new QWidget(this);
    auto* stoppageRow = new QHBoxLayout(stoppageBox_);
    stoppageRow->setContentsMargins(0, 0, 0, 0);
    auto* stoppageMinus = makeStepButton(QStringLiteral("−"), stoppageBox_);
    auto* stoppagePlus = makeStepButton(QStringLiteral("+"), stoppageBox_);
    connect(stoppageMinus, &QPushButton::clicked, this,
            [this]() { emit stoppageStepRequested(-1); });
    connect(stoppagePlus, &QPushButton::clicked, this, [this]() { emit stoppageStepRequested(1); });
    stoppage_ = new QLabel(stoppageBox_);
    stoppage_->setAlignment(Qt::AlignCenter);
    stoppageRow->addWidget(stoppageMinus);
    stoppageRow->addWidget(stoppage_);
    stoppageRow->addWidget(stoppagePlus);
    stoppageBox_->setToolTip(text("Panel.Stoppage"));
    stoppageBox_->hide();
    periodRow->addWidget(stoppageBox_);
    periodRow->addStretch(1);
    outer->addLayout(periodRow);

    auto* controls = new QHBoxLayout();
    startStop_ = new QPushButton(this);
    startStop_->setMinimumHeight(36);
    connect(startStop_, &QPushButton::clicked, this, [this]() { emit toggleRequested(); });
    reset_ = new QPushButton(text("Dock.Reset"), this);
    reset_->setToolTip(text("Dock.Reset"));
    connect(reset_, &QPushButton::clicked, this, [this]() { emit resetRequested(); });
    controls->addWidget(startStop_, 1);
    controls->addWidget(reset_);
    outer->addLayout(controls);

    setRunning(false);
}

void ClockPanel::setClock(const QString& value) {
    if (clock_->text() == value) return;
    clock_->setText(value);
    updateClockFont(); // "9:59" and "09:59" want different sizes
}

void ClockPanel::setPeriod(const QString& label, int regulationPeriods) {
    if (period_->text() != label) period_->setText(label);
    const QString of = text("Panel.Of").arg(regulationPeriods);
    if (of_->text() != of) of_->setText(of);
}

void ClockPanel::setStoppage(bool visible, int minutes) {
    if (stoppageShown_ != visible) {
        stoppageShown_ = visible;
        stoppageBox_->setVisible(visible);
    }
    const QString value = QStringLiteral("+%1").arg(minutes);
    if (stoppage_->text() != value) stoppage_->setText(value);
}

void ClockPanel::setRunning(bool running) {
    const int state = running ? 1 : 0;
    if (shownRunning_ == state) return;
    shownRunning_ = state;
    startStop_->setText(text(running ? "Dock.Stop" : "Dock.Start"));
    startStop_->setStyleSheet(
        running ? QStringLiteral("QPushButton{background:#C0392B;color:white;font-weight:bold;}")
                : QStringLiteral("QPushButton{background:#27AE60;color:white;font-weight:bold;}"));
}

void ClockPanel::setResetEnabled(bool enabled) {
    if (reset_->isEnabled() != enabled) reset_->setEnabled(enabled);
}

void ClockPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateClockFont();
}

void ClockPanel::updateClockFont() {
    const int chars = std::max(5, static_cast<int>(clock_->text().size()));
    const double available = width() - 2.0 * kArrowColumn - 16.0;
    const int pixels = std::clamp(static_cast<int>(available / (chars * 0.62)), 22, 72);
    if (pixels == shownPixelSize_) return;
    shownPixelSize_ = pixels;
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setBold(true);
    font.setPixelSize(pixels);
    clock_->setFont(font);
}
