// SPDX-License-Identifier: GPL-2.0-or-later
#include "PenaltyDialog.hpp"

#include <obs-module.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cstddef>
#include <string>

#include "Format.hpp"

namespace {

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

} // namespace

PenaltyDialog::PenaltyDialog(const std::vector<sb::PenaltyOption>& options,
                             const std::string& labelFormat, QWidget* parent)
    : QDialog(parent), labelFormat_(labelFormat), options_(options) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(text("Penalty.AddTitle"));

    auto* outer = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    option_ = new QComboBox(this);
    option_->setObjectName(QStringLiteral("penaltyOption"));
    for (const sb::PenaltyOption& o : options_) option_->addItem(QString::fromStdString(o.label));
    connect(option_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updatePreview(); });
    form->addRow(text("Penalty.Option"), option_);

    player_ = new QLineEdit(this);
    player_->setObjectName(QStringLiteral("penaltyPlayer"));
    player_->setPlaceholderText(text("Penalty.PlayerHint"));
    connect(player_, &QLineEdit::textChanged, this, [this](const QString&) { updatePreview(); });
    form->addRow(text("Penalty.Player"), player_);
    outer->addLayout(form);
    finishLayout(outer);
    updatePreview();
}

PenaltyDialog::PenaltyDialog(unsigned id, const QString& player, sb::Tenths remaining,
                             sb::Tenths nextPhase, const std::string& labelFormat, QWidget* parent)
    : QDialog(parent), editMode_(true), id_(id), fixedPlayer_(player), fixedPhase2_(nextPhase),
      labelFormat_(labelFormat) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(text("Penalty.EditTitle"));

    auto* outer = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    playerShown_ = new QLabel(player, this);
    form->addRow(text("Penalty.Player"), playerShown_);

    auto* timeRow = new QHBoxLayout();
    minutes_ = new QSpinBox(this);
    minutes_->setObjectName(QStringLiteral("penaltyMinutes"));
    minutes_->setRange(0, 99);
    seconds_ = new QSpinBox(this);
    seconds_->setObjectName(QStringLiteral("penaltySeconds"));
    seconds_->setRange(0, 59);
    const int initialMinutes = static_cast<int>(remaining / sb::kTenthsPerMinute);
    const int initialSeconds =
        static_cast<int>((remaining % sb::kTenthsPerMinute) / sb::kTenthsPerSecond);
    minutes_->setValue(initialMinutes);
    // 0:00 would end the phase (PenaltyBox::edit): the spins never go below 0:01. With minutes 0
    // the seconds start at 1; the X in the dock is what cancels a penalty.
    seconds_->setRange(initialMinutes == 0 ? 1 : 0, 59);
    seconds_->setValue(initialSeconds);
    // The spins may have clamped the requested value (e.g. seconds raised to 1 with 0 minutes):
    // compare the no-op guard against what the window actually shows, not the raw remaining time.
    initialTenths_ = sb::minutes(minutes_->value()) + sb::seconds(seconds_->value());
    connect(minutes_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        seconds_->setRange(value == 0 ? 1 : 0, 59);
        updatePreview();
    });
    connect(seconds_, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int) { updatePreview(); });
    timeRow->addWidget(minutes_);
    timeRow->addWidget(new QLabel(text("Settings.Minutes"), this));
    timeRow->addWidget(seconds_);
    timeRow->addWidget(new QLabel(text("Settings.Seconds"), this));
    form->addRow(text("Penalty.Time"), timeRow);
    outer->addLayout(form);
    finishLayout(outer);
    updatePreview();
}

void PenaltyDialog::finishLayout(QVBoxLayout* outer) {
    outer->addWidget(new QLabel(text("Penalty.Preview"), this));
    preview_ = new QLabel(this);
    preview_->setObjectName(QStringLiteral("penaltyPreview"));
    preview_->setWordWrap(true);
    outer->addWidget(preview_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &PenaltyDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);
}

QString PenaltyDialog::playerText() const {
    return editMode_ ? fixedPlayer_ : player_->text().trimmed();
}

sb::Tenths PenaltyDialog::timeTenths() const {
    if (editMode_) return sb::minutes(minutes_->value()) + sb::seconds(seconds_->value());
    const int index = option_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= options_.size()) return 0;
    return options_[static_cast<std::size_t>(index)].phases.front().duration;
}

sb::Tenths PenaltyDialog::phase2Tenths() const {
    if (editMode_) return fixedPhase2_;
    const int index = option_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= options_.size()) return 0;
    const std::vector<sb::PenaltyPhase>& phases = options_[static_cast<std::size_t>(index)].phases;
    return phases.size() > 1 ? phases[1].duration : 0;
}

QString PenaltyDialog::previewText() const {
    const sb::Tenths phase2 = phase2Tenths();
    return QString::fromStdString(sb::applyTemplate(
        labelFormat_,
        {{"player", playerText().toStdString()},
         {"time", sb::formatPenaltyTime(timeTenths())},
         {"phase2", phase2 > 0 ? "+" + sb::formatPenaltyTime(phase2) : std::string()}}));
}

void PenaltyDialog::updatePreview() {
    const QString preview = previewText();
    if (preview_->text() != preview) preview_->setText(preview);
}

bool PenaltyDialog::validPlayer() const {
    return !playerText().isEmpty();
}

void PenaltyDialog::accept() {
    if (editMode_) {
        // Only a real change is sent: re-sending the value read when the window opened would give
        // the penalty back the time the running clock has consumed in the meantime.
        if (timeTenths() != initialTenths_)
            emit editRequested(id_, static_cast<qint64>(timeTenths()));
        QDialog::accept();
        return;
    }
    if (!validPlayer()) {
        QMessageBox::warning(this, windowTitle(), text("Penalty.BadPlayer"));
        return;
    }
    emit addRequested(option_->currentIndex(), player_->text().trimmed());
    QDialog::accept();
}
