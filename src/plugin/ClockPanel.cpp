// SPDX-License-Identifier: GPL-2.0-or-later
#include "ClockPanel.hpp"

#include <obs-module.h>

#include <QByteArray>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <algorithm>

#include "Types.hpp"
#include "plugin-support.h"

namespace {

// Wide enough for a "+1:00" label at the theme's button font; also used to size the clock.
constexpr int kArrowColumn = 52;

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

// DSEG7 Classic Bold (the 7-segment look), installed next to the locale as data/fonts. Loaded
// once for the whole process; any failure silently keeps the fixed bold font. obs_module_file()
// returns a char* owned by the module, freed with bfree().
const QString& digitalFontFamily() {
    static const QString family = [] {
        char* path = obs_module_file("fonts/DSEG7Classic-Bold.ttf");
        if (!path) {
            obs_log(LOG_WARNING, "digital clock font not found, using the default clock font");
            return QString();
        }
        QFile file(QString::fromUtf8(path));
        bfree(path);
        if (!file.open(QIODevice::ReadOnly)) {
            obs_log(LOG_WARNING,
                    "could not open the digital clock font, using the default clock font");
            return QString();
        }
        const int id = QFontDatabase::addApplicationFontFromData(file.readAll());
        const QStringList families =
            id >= 0 ? QFontDatabase::applicationFontFamilies(id) : QStringList();
        if (families.isEmpty()) {
            obs_log(LOG_WARNING,
                    "could not load the digital clock font, using the default clock font");
            return QString();
        }
        return families.first();
    }();
    return family;
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
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(3);

    top_ = new QHBoxLayout();
    top_->setSpacing(2);
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
    clock_->installEventFilter(this);
    top_->addLayout(minutesColumn);
    top_->addWidget(clock_, 1);
    top_->addLayout(secondsColumn);
    outer->addLayout(top_);

    // Period stepper, stoppage counter, start/pause and reset share one row (design B+).
    auto* controls = new QHBoxLayout();
    controls->setSpacing(3);
    auto* prev = makeStepButton(QStringLiteral("−"), this);
    auto* next = makeStepButton(QStringLiteral("+"), this);
    connect(prev, &QPushButton::clicked, this, [this]() { emit periodStepRequested(-1); });
    connect(next, &QPushButton::clicked, this, [this]() { emit periodStepRequested(1); });
    period_ = new QLabel(this);
    period_->setAlignment(Qt::AlignCenter);
    QFont bold = period_->font();
    bold.setBold(true);
    period_->setFont(bold);
    period_->setMinimumWidth(24);
    of_ = new QLabel(this);
    controls->addWidget(prev);
    controls->addWidget(period_);
    controls->addWidget(next);
    controls->addWidget(of_);

    stoppageBox_ = new QWidget(this);
    auto* stoppageRow = new QHBoxLayout(stoppageBox_);
    stoppageRow->setContentsMargins(0, 0, 0, 0);
    stoppageRow->setSpacing(2);
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
    controls->addWidget(stoppageBox_);

    startStop_ = new QPushButton(this);
    startStop_->setMinimumHeight(30);
    startStop_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(startStop_, &QPushButton::clicked, this, [this]() { emit toggleRequested(); });
    reset_ = new QPushButton(text("Dock.Reset"), this);
    reset_->setToolTip(text("Dock.Reset"));
    connect(reset_, &QPushButton::clicked, this, [this]() { emit resetRequested(); });
    controls->addWidget(startStop_, 1);
    controls->addWidget(reset_);
    outer->addLayout(controls);

    setRunning(false);
}

void ClockPanel::addTopRightWidget(QWidget* widget) {
    top_->addWidget(widget, 0, Qt::AlignTop);
    cornerWidth_ = std::max(cornerWidth_, widget->sizeHint().width());
    updateClockFont();
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

bool ClockPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == clock_ && event->type() == QEvent::Resize) updateClockFont();
    return QWidget::eventFilter(watched, event);
}

void ClockPanel::updateClockFont() {
    const QString family = digitalFontFamily();
    // The label is the stretchy column of the row: once it has been laid out its width is the
    // exact space the digits may use. Before that, fall back to an estimate from this widget.
    const int labelWidth = clock_->width();
    const double available =
        std::max(40.0, labelWidth > 0 ? labelWidth - 4.0
                                      : width() - 2.0 * kArrowColumn - cornerWidth_ - 12.0);
    // DSEG7's digit advances differ from the fixed font's, so measure instead of guessing a ratio:
    // size the text at a reference pixel size and scale it to the width the clock has.
    QFont font =
        family.isEmpty() ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : QFont(family);
    font.setBold(true);
    constexpr int kReference = 64;
    font.setPixelSize(kReference);
    const int textWidth = QFontMetrics(font).horizontalAdvance(clock_->text());
    const int pixels = std::clamp(
        textWidth > 0 ? static_cast<int>(kReference * available / textWidth) : kReference, 22, 72);
    if (pixels == shownPixelSize_) return;
    shownPixelSize_ = pixels;
    font.setPixelSize(pixels);
    clock_->setFont(font);
}
