// SPDX-License-Identifier: GPL-2.0-or-later
#include "EventLogDialog.hpp"
#include "DockStyle.hpp"

#include <obs-module.h>

#include <QAbstractItemView>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <vector>

#include "AtomicFile.hpp"
#include "ChaptersExport.hpp"
#include "ClockDriver.hpp"
#include "MatchEngine.hpp"

namespace {

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

} // namespace

EventLogDialog::EventLogDialog(ClockDriver* driver, std::optional<sb::Micros> streamStart,
                               std::optional<sb::Micros> recordingStart, QWidget* parent)
    : QDialog(parent), driver_(driver), streamStart_(streamStart), recordingStart_(recordingStart) {
    setWindowTitle(text("EventLog.Title"));
    setAttribute(Qt::WA_DeleteOnClose);

    auto* outer = new QVBoxLayout(this);

    auto* baseRow = new QHBoxLayout();
    baseRow->addWidget(new QLabel(text("EventLog.Base"), this));
    base_ = new QComboBox(this);
    base_->setObjectName(QStringLiteral("eventLogBase"));
    if (streamStart_) base_->addItem(text("EventLog.Base.Stream"), 0);
    if (recordingStart_) base_->addItem(text("EventLog.Base.Recording"), 2);
    base_->addItem(text("EventLog.Base.PlayTime"), 1);
    baseRow->addWidget(base_);
    baseRow->addStretch(1);
    outer->addLayout(baseRow);

    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("eventLogTable"));
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels(
        {text("EventLog.Time"), text("EventLog.Team"), text("EventLog.Description")});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    outer->addWidget(table_, 1);

    hint_ = new QLabel(this);
    hint_->setWordWrap(true);
    outer->addWidget(hint_);

    auto* buttons = new QHBoxLayout();
    auto* copyLog = new QPushButton(text("EventLog.CopyLog"), this);
    auto* copyChapters = new QPushButton(text("EventLog.CopyChapters"), this);
    auto* save = new QPushButton(text("EventLog.SaveChapters"), this);
    connect(copyLog, &QPushButton::clicked, this, &EventLogDialog::copyLog);
    connect(copyChapters, &QPushButton::clicked, this, &EventLogDialog::copyChapters);
    connect(save, &QPushButton::clicked, this, &EventLogDialog::saveChapters);
    buttons->addWidget(copyLog);
    buttons->addWidget(copyChapters);
    buttons->addWidget(save);
    buttons->addStretch(1);
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    dockstyle::styleDialog(this);
    buttons->addWidget(close);
    outer->addLayout(buttons);

    connect(base_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { rebuild(); });
    if (driver_) {
        connect(driver_, &ClockDriver::commandApplied, this, &EventLogDialog::rebuild);
        connect(driver_, &ClockDriver::matchReplaced, this, &EventLogDialog::rebuild);
        // The engine logs a PeriodEnd from onTick -> advance when the clock reaches its limit,
        // with no command and no command signal: the stop itself is the only notification.
        connect(driver_, &ClockDriver::runningChanged, this, &EventLogDialog::rebuild);
    }
    rebuild();
}

sb::ChapterBase EventLogDialog::chapterBase() const {
    return base_->currentData().toInt() == 1 ? sb::ChapterBase::PlayTime
                                             : sb::ChapterBase::StreamStart;
}

// The instant the stream/recording base counts from: data 2 is the recording, everything else the
// stream (data 0). The game-time base ignores it.
sb::Micros EventLogDialog::selectedStart() const {
    return base_->currentData().toInt() == 2 ? recordingStart_.value_or(0)
                                             : streamStart_.value_or(0);
}

// A resumed match keeps `at` values from a previous OBS session (the monotonic clock resets on
// reboot): with the stream/recording base only events inside [start, now] are meaningful.
bool EventLogDialog::inBase(const sb::MatchEvent& event) const {
    if (chapterBase() == sb::ChapterBase::PlayTime) return true;
    const sb::Micros start = selectedStart();
    const sb::Micros now = ClockDriver::nowMicros();
    return event.at >= start && event.at <= now;
}

std::vector<sb::MatchEvent> EventLogDialog::visibleEvents() const {
    std::vector<sb::MatchEvent> out;
    if (!driver_) return out;
    for (const sb::MatchEvent& event : driver_->engine().events().events())
        if (inBase(event)) out.push_back(event);
    return out;
}

QString EventLogDialog::teamFor(const sb::MatchEvent& event) const {
    if (!event.team || !driver_) return QString();
    const sb::MatchSettings& settings = driver_->engine().settings();
    return QString::fromStdString(*event.team == sb::Team::Home ? settings.homeName
                                                                : settings.awayName);
}

QString EventLogDialog::timeFor(const sb::MatchEvent& event) const {
    if (!inBase(event)) return QString(); // never a bogus time from another session
    const std::int64_t secs = chapterBase() == sb::ChapterBase::StreamStart
                                  ? std::max<sb::Micros>(0, event.at - selectedStart()) / 1000000
                                  : event.playTime / sb::kTenthsPerSecond;
    return QString::fromStdString(sb::chapterTimeString(secs));
}

QString EventLogDialog::titleFor(const sb::MatchEvent& event) const {
    const QString label = QString::fromStdString(event.periodLabel);
    switch (event.type) {
    case sb::EventType::PeriodStart:
        return text("Event.PeriodStart").arg(label);
    case sb::EventType::PeriodEnd:
        return text("Event.PeriodEnd").arg(label);
    case sb::EventType::Score:
        return text("Event.Score").arg(teamFor(event)).arg(event.homeScore).arg(event.awayScore);
    case sb::EventType::Penalty:
        return text("Event.Penalty").arg(teamFor(event), QString::fromStdString(event.detail));
    case sb::EventType::MatchEnd:
        return text("Event.MatchEnd");
    }
    return QString();
}

sb::ChaptersResult EventLogDialog::chapters() const {
    if (!driver_) return {};
    return sb::buildChapters(
        driver_->engine().events().events(), chapterBase(), selectedStart(),
        text("Event.First").toStdString(),
        [this](const sb::MatchEvent& event) { return titleFor(event).toStdString(); },
        ClockDriver::nowMicros());
}

std::string EventLogDialog::chaptersText() const {
    return chapters().text;
}

void EventLogDialog::rebuild() {
    if (!driver_) return;
    const sb::MatchSettings& settings = driver_->engine().settings();
    const std::vector<sb::MatchEvent> events = visibleEvents();
    // No unchanged work: a rebuild that would not change a single row is skipped (accepted
    // commands like cancelling a queued penalty fire without touching the log or the names).
    if (base_->currentIndex() == lastBase_ && events.size() == lastEventCount_ &&
        settings.homeName == lastHomeName_ && settings.awayName == lastAwayName_) {
        return;
    }
    lastBase_ = base_->currentIndex();
    lastEventCount_ = events.size();
    lastHomeName_ = settings.homeName;
    lastAwayName_ = settings.awayName;
    table_->setRowCount(static_cast<int>(events.size()));
    for (int row = 0; row < static_cast<int>(events.size()); ++row) {
        const sb::MatchEvent& event = events[static_cast<std::size_t>(row)];
        table_->setItem(row, 0, new QTableWidgetItem(timeFor(event)));
        table_->setItem(row, 1, new QTableWidgetItem(teamFor(event)));
        table_->setItem(row, 2, new QTableWidgetItem(titleFor(event)));
    }
    const sb::ChaptersResult result = chapters();
    QString hint = text("EventLog.Count").arg(static_cast<int>(result.chapters.size()));
    if (!result.youtubeReady) hint += QStringLiteral(" ") + text("EventLog.NeedMore");
    if (hint_->text() != hint) hint_->setText(hint);
}

void EventLogDialog::copyLog() {
    if (!driver_) return;
    QString out;
    for (const sb::MatchEvent& event : visibleEvents()) {
        if (!out.isEmpty()) out += QLatin1Char('\n');
        out += timeFor(event) + QLatin1Char('\t') + teamFor(event) + QLatin1Char('\t') +
               titleFor(event);
    }
    QGuiApplication::clipboard()->setText(out);
    hint_->setText(text("EventLog.Copied"));
}

void EventLogDialog::copyChapters() {
    QGuiApplication::clipboard()->setText(QString::fromStdString(chaptersText()));
    hint_->setText(text("EventLog.Copied"));
}

void EventLogDialog::saveChapters() {
    const QString path =
        QFileDialog::getSaveFileName(this, text("EventLog.SaveChapters"),
                                     QStringLiteral("chapters.txt"), text("EventLog.TextFilter"));
    if (path.isEmpty()) return;
    std::string error;
    if (!writeFileAtomic(path.toStdString(), chaptersText(), &error))
        QMessageBox::warning(this, text("EventLog.Title"),
                             text("EventLog.SaveFailed").arg(QString::fromStdString(error)));
    else
        hint_->setText(text("EventLog.Saved").arg(path));
}
