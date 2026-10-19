// SPDX-License-Identifier: GPL-2.0-or-later
#include "ClockPanel.hpp"

#include <obs-module.h>

#include <QByteArray>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <algorithm>

#include "DockStyle.hpp"
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

// One cell of a segmented group: [ cell | value | cell ].
void setSeg(QWidget* w, const char* pos) {
    dockstyle::setProp(w, dockstyle::kRole, QStringLiteral("seg"));
    dockstyle::setProp(w, dockstyle::kPos, QString::fromLatin1(pos));
}

} // namespace

QPushButton* ClockPanel::makeArrow(const QString& label, const QString& tooltip,
                                   qint64 deltaTenths) {
    auto* button = new QPushButton(label, this);
    dockstyle::setProp(button, dockstyle::kRole, QStringLiteral("btn"));
    button->setMinimumWidth(kArrowColumn);
    button->setFocusPolicy(Qt::TabFocus);
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
    outer->setSpacing(6);

    // The clock "display": a framed card with the digits between the minute and second arrows, and
    // the period and stoppage steppers under them.
    auto* card = new QFrame(this);
    dockstyle::setProp(card, dockstyle::kCard, QStringLiteral("clock"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 8, 8, 8);
    cardLayout->setSpacing(6);

    top_ = new QHBoxLayout();
    top_->setSpacing(6);
    auto* minutesColumn = new QVBoxLayout();
    minutesColumn->setSpacing(4);
    minutesColumn->addWidget(
        makeArrow(QStringLiteral("+1:00"), text("Hotkey.clock_min_up"), sb::minutes(1)));
    minutesColumn->addWidget(
        makeArrow(QStringLiteral("-1:00"), text("Hotkey.clock_min_down"), -sb::minutes(1)));
    auto* secondsColumn = new QVBoxLayout();
    secondsColumn->setSpacing(4);
    secondsColumn->addWidget(
        makeArrow(QStringLiteral("+0:01"), text("Hotkey.clock_sec_up"), sb::seconds(1)));
    secondsColumn->addWidget(
        makeArrow(QStringLiteral("-0:01"), text("Hotkey.clock_sec_down"), -sb::seconds(1)));

    clock_ = new QLabel(card);
    clock_->setAlignment(Qt::AlignCenter);
    clock_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    clock_->installEventFilter(this);
    top_->addLayout(minutesColumn);
    top_->addWidget(clock_, 1);
    top_->addLayout(secondsColumn);
    cardLayout->addLayout(top_);

    auto* sub = new QHBoxLayout();
    sub->setSpacing(8);
    sub->addStretch(1);
    auto* periodGroup = new QHBoxLayout();
    periodGroup->setSpacing(0);
    auto* prev = new dockstyle::IconButton(dockstyle::Icon::Prev, card);
    prev->setToolTip(text("Hotkey.period_prev"));
    setSeg(prev, "first");
    auto* next = new dockstyle::IconButton(dockstyle::Icon::Next, card);
    next->setToolTip(text("Hotkey.period_next"));
    setSeg(next, "last");
    connect(prev, &QPushButton::clicked, this, [this]() { emit periodStepRequested(-1); });
    connect(next, &QPushButton::clicked, this, [this]() { emit periodStepRequested(1); });
    period_ = new QLabel(card);
    period_->setAlignment(Qt::AlignCenter);
    period_->setMinimumWidth(34);
    dockstyle::setProp(period_, dockstyle::kRole, QStringLiteral("segval"));
    periodGroup->addWidget(prev);
    periodGroup->addWidget(period_);
    periodGroup->addWidget(next);
    sub->addLayout(periodGroup);
    of_ = new QLabel(card);
    dockstyle::setProp(of_, dockstyle::kText, QStringLiteral("muted"));
    sub->addWidget(of_);

    stoppageBox_ = new QWidget(card);
    auto* stoppageRow = new QHBoxLayout(stoppageBox_);
    stoppageRow->setContentsMargins(0, 0, 0, 0);
    stoppageRow->setSpacing(0);
    auto* stoppageMinus = new QPushButton(QStringLiteral("−"), stoppageBox_);
    setSeg(stoppageMinus, "first");
    auto* stoppagePlus = new QPushButton(QStringLiteral("+"), stoppageBox_);
    setSeg(stoppagePlus, "last");
    connect(stoppageMinus, &QPushButton::clicked, this,
            [this]() { emit stoppageStepRequested(-1); });
    connect(stoppagePlus, &QPushButton::clicked, this, [this]() { emit stoppageStepRequested(1); });
    stoppage_ = new QLabel(stoppageBox_);
    stoppage_->setAlignment(Qt::AlignCenter);
    stoppage_->setMinimumWidth(34);
    dockstyle::setProp(stoppage_, dockstyle::kRole, QStringLiteral("segval"));
    stoppageRow->addWidget(stoppageMinus);
    stoppageRow->addWidget(stoppage_);
    stoppageRow->addWidget(stoppagePlus);
    stoppageBox_->setToolTip(text("Panel.Stoppage"));
    stoppageBox_->hide();
    sub->addWidget(stoppageBox_);
    sub->addStretch(1);
    cardLayout->addLayout(sub);
    outer->addWidget(card);

    // The two actions of the match clock, the first one large.
    auto* controls = new QHBoxLayout();
    controls->setSpacing(6);
    auto* startStop = new dockstyle::IconButton(dockstyle::Icon::Play, this);
    startStop->setTint(QColor(Qt::white));
    startStop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    startStop_ = startStop;
    connect(startStop_, &QPushButton::clicked, this, [this]() { emit toggleRequested(); });
    reset_ = new dockstyle::IconButton(dockstyle::Icon::Reset, this);
    reset_->setText(text("Dock.Reset"));
    reset_->setToolTip(text("Dock.Reset"));
    dockstyle::setProp(reset_, dockstyle::kRole, QStringLiteral("ghost"));
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
    // Green to start, red to stop; the role carries the colors (see DockStyle).
    dockstyle::setProp(startStop_, dockstyle::kRole,
                       running ? QStringLiteral("stop") : QStringLiteral("go"));
    static_cast<dockstyle::IconButton*>(startStop_)
        ->setIconKind(running ? dockstyle::Icon::Pause : dockstyle::Icon::Play);
}

void ClockPanel::setResetEnabled(bool enabled) {
    if (reset_->isEnabled() != enabled) reset_->setEnabled(enabled);
}

void ClockPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateClockFont();
}

bool ClockPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == clock_) {
        if (event->type() == QEvent::Resize) {
            updateClockFont();
        } else if (event->type() == QEvent::Polish || event->type() == QEvent::PaletteChange) {
            // A style sheet (re)resolves the label's font; ours must win again.
            shownPixelSize_ = 0;
            updateClockFont();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ClockPanel::updateClockFont() {
    const QString family = digitalFontFamily();
    // The label is the stretchy column of the row: once it has been laid out its width is the
    // exact space the digits may use. Before that, fall back to an estimate from this widget.
    const int labelWidth = clock_->width();
    const double available =
        std::max(40.0, labelWidth > 0 ? labelWidth - 4.0 : width() - 2.0 * kArrowColumn - 40.0);
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
    // Under a style sheet the label's font is resolved by the sheet machinery and a plain setFont()
    // can lose: say the same thing in a rule of the label's own.
    // The text color is part of the rule too: a label with a sheet of its own keeps the color it
    // was polished with, so it would not follow a change of OBS's theme.
    const QString css =
        QStringLiteral("QLabel{font-family:\"%1\";font-size:%2px;font-weight:700;color:%3;}")
            .arg(font.family())
            .arg(pixels)
            .arg(clock_->palette().color(QPalette::WindowText).name());
    if (clock_->styleSheet() != css) clock_->setStyleSheet(css); // (a style change re-enters here)
}
