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

constexpr int kArrowColumn = 44;

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

} // namespace

QPushButton* ClockPanel::makeArrow(const QString& glyph, qint64 deltaTenths) {
    auto* button = new QPushButton(glyph, this);
    button->setFixedWidth(kArrowColumn - 4);
    button->setAutoRepeat(true); // held down: repeats (CLAUDE.md §4.3)
    button->setAutoRepeatDelay(400);
    button->setAutoRepeatInterval(100);
    connect(button, &QPushButton::clicked, this,
            [this, deltaTenths]() { emit arrowRequested(deltaTenths); });
    return button;
}

ClockPanel::ClockPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(2, 2, 2, 2);
    outer->setSpacing(4);

    const QString up = QStringLiteral("▲");
    const QString down = QStringLiteral("▼");
    auto* top = new QHBoxLayout();
    auto* minutesColumn = new QVBoxLayout();
    minutesColumn->addWidget(makeArrow(up, sb::minutes(1)));
    minutesColumn->addWidget(new QLabel(text("Panel.MinutesShort"), this), 0, Qt::AlignCenter);
    minutesColumn->addWidget(makeArrow(down, -sb::minutes(1)));
    auto* secondsColumn = new QVBoxLayout();
    secondsColumn->addWidget(makeArrow(up, sb::seconds(1)));
    secondsColumn->addWidget(new QLabel(text("Panel.SecondsShort"), this), 0, Qt::AlignCenter);
    secondsColumn->addWidget(makeArrow(down, -sb::seconds(1)));

    clock_ = new QLabel(this);
    clock_->setAlignment(Qt::AlignCenter);
    clock_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    top->addLayout(minutesColumn);
    top->addWidget(clock_, 1);
    top->addLayout(secondsColumn);
    outer->addLayout(top);

    auto* periodRow = new QHBoxLayout();
    auto* prev = new QPushButton(QStringLiteral("−"), this);
    auto* next = new QPushButton(QStringLiteral("+"), this);
    prev->setFixedWidth(28);
    next->setFixedWidth(28);
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
    auto* stoppageMinus = new QPushButton(QStringLiteral("−"), stoppageBox_);
    auto* stoppagePlus = new QPushButton(QStringLiteral("+"), stoppageBox_);
    stoppageMinus->setFixedWidth(28);
    stoppagePlus->setFixedWidth(28);
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
    reset_ = new QPushButton(QStringLiteral("↺"), this);
    reset_->setToolTip(text("Dock.Reset"));
    reset_->setFixedWidth(36);
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
