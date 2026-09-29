// SPDX-License-Identifier: GPL-2.0-or-later
#include "ScoreboardDock.hpp"

#include <obs-module.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "ClockDriver.hpp"
#include "FieldSinks.hpp"
#include "SourcePicker.hpp"

namespace {

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

struct Arrow {
    const char* label;
    sb::Tenths delta;
};

} // namespace

ScoreboardDock::ScoreboardDock(ClockDriver* driver, FieldSinks* sinks, QWidget* parent)
    : QWidget(parent), driver_(driver), sinks_(sinks) {
    auto* layout = new QVBoxLayout(this);

    clock_ = new QLabel(this);
    clock_->setAlignment(Qt::AlignCenter);
    QFont big = clock_->font();
    big.setPointSizeF(big.pointSizeF() * 3.0);
    big.setBold(true);
    clock_->setFont(big);
    layout->addWidget(clock_);

    auto* arrows = new QHBoxLayout();
    const Arrow steps[] = {{"-1:00", -sb::minutes(1)},
                           {"-0:01", -sb::seconds(1)},
                           {"+0:01", sb::seconds(1)},
                           {"+1:00", sb::minutes(1)}};
    for (const Arrow& step : steps) {
        auto* button = new QPushButton(QString::fromUtf8(step.label), this);
        button->setAutoRepeat(true); // held down: repeats (CLAUDE.md §4.3)
        button->setAutoRepeatDelay(400);
        button->setAutoRepeatInterval(100);
        const sb::Tenths delta = step.delta;
        connect(button, &QPushButton::clicked, this, [this, delta]() {
            if (driver_) driver_->apply(sb::cmd::ClockAdjust{delta});
        });
        arrows->addWidget(button);
    }
    layout->addLayout(arrows);

    auto* controls = new QHBoxLayout();
    startStop_ = new QPushButton(this);
    connect(startStop_, &QPushButton::clicked, this, [this]() {
        if (driver_) driver_->apply(sb::cmd::ClockToggle{});
    });
    auto* reset = new QPushButton(text("Dock.Reset"), this);
    connect(reset, &QPushButton::clicked, this, [this]() {
        if (driver_) driver_->apply(sb::cmd::ClockReset{});
    });
    controls->addWidget(startStop_, 2);
    controls->addWidget(reset, 1);
    layout->addLayout(controls);

    auto* sourceRow = new QHBoxLayout();
    sourceRow->addWidget(new QLabel(text("Dock.ClockSource"), this));
    source_ = new SourcePicker(this);
    connect(source_, &QComboBox::activated, this, [this](int) { chooseSource(); });
    sourceRow->addWidget(source_, 1);
    layout->addLayout(sourceRow);

    status_ = new QLabel(this);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    layout->addStretch(1);

    if (driver_) {
        connect(driver_, &ClockDriver::fieldsChanged, this, &ScoreboardDock::showFields);
        connect(driver_, &ClockDriver::runningChanged, this, &ScoreboardDock::showRunning);
        showFields(driver_->fields());
        showRunning(driver_->engine().clock().running());
    }
    if (sinks_) {
        connect(sinks_, &FieldSinks::bindingChanged, this, [this](sb::FieldId field) {
            if (field == sb::FieldId::Clock) showStatus();
        });
    }
    showStatus();
}

void ScoreboardDock::showFields(const sb::FieldValues& fields) {
    const QString clock =
        QString::fromStdString(fields[static_cast<std::size_t>(sb::FieldId::Clock)]);
    if (clock == shownClock_) return;
    shownClock_ = clock;
    clock_->setText(clock);
}

void ScoreboardDock::showRunning(bool running) {
    const QString label = text(running ? "Dock.Stop" : "Dock.Start");
    if (startStop_->text() != label) startStop_->setText(label);
}

void ScoreboardDock::showStatus() {
    if (!sinks_) return;
    const TextSink& sink = sinks_->sink(sb::FieldId::Clock);
    source_->showBinding(sink);
    const QString name = QString::fromStdString(sink.binding().name);
    QString message;
    if (sink.bound()) {
        message = text("Dock.Status.Linked").arg(name);
    } else {
        switch (sink.problem()) {
        case TextSink::Problem::Removed:
            message = text("Dock.Status.Removed").arg(name);
            break;
        case TextSink::Problem::WrongType:
            message = text("Dock.Status.WrongType").arg(name);
            break;
        case TextSink::Problem::NotFound:
            message = text("Dock.Status.NotFound").arg(name);
            break;
        case TextSink::Problem::None:
            message = text("Dock.Status.Unlinked");
            break;
        }
    }
    if (status_->text() != message) status_->setText(message);
}

void ScoreboardDock::chooseSource() {
    if (!sinks_) return;
    const auto binding = source_->selected();
    if (binding)
        sinks_->bindTo(sb::FieldId::Clock, *binding);
    else
        sinks_->unbind(sb::FieldId::Clock);
}
