// SPDX-License-Identifier: GPL-2.0-or-later
#include "PenaltyDialog.hpp"
#include "DockStyle.hpp"

#include <obs-module.h>

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
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
    // The type of penalty as a row of chips (one selected at a time), wrapped four to a row.
    auto* chipBox = new QWidget(this);
    auto* chipGrid = new QGridLayout(chipBox);
    chipGrid->setContentsMargins(0, 0, 0, 0);
    chipGrid->setSpacing(6);
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    for (std::size_t i = 0; i < options_.size(); ++i) {
        auto* chip = new QPushButton(QString::fromStdString(options_[i].label), chipBox);
        chip->setCheckable(true);
        chip->setObjectName(QStringLiteral("penaltyChip%1").arg(i));
        dockstyle::setProp(chip, dockstyle::kRole, QStringLiteral("chip"));
        group->addButton(chip, static_cast<int>(i));
        chipGrid->addWidget(chip, static_cast<int>(i / 4), static_cast<int>(i % 4));
        connect(chip, &QPushButton::toggled, this, [this, chip](bool on) {
            dockstyle::setProp(chip, dockstyle::kRole,
                               on ? QStringLiteral("chipOn") : QStringLiteral("chip"));
            if (on) updatePreview();
        });
        chips_.push_back(chip);
    }
    form->addRow(text("Penalty.Option"), chipBox);

    player_ = new QLineEdit(this);
    player_->setObjectName(QStringLiteral("penaltyPlayer"));
    player_->setPlaceholderText(text("Penalty.PlayerHint"));
    connect(player_, &QLineEdit::textChanged, this, [this](const QString&) { updatePreview(); });
    form->addRow(text("Penalty.Player"), player_);
    outer->addLayout(form);
    finishLayout(outer);
    // Last: checking a chip updates the preview, which needs the player field and the label.
    if (!chips_.empty()) chips_.front()->setChecked(true);
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
    setMinimumWidth(340);
    dockstyle::styleDialog(this);
    outer->addWidget(buttons);
}

QString PenaltyDialog::playerText() const {
    return editMode_ ? fixedPlayer_ : player_->text().trimmed();
}

sb::Tenths PenaltyDialog::timeTenths() const {
    if (editMode_) return sb::minutes(minutes_->value()) + sb::seconds(seconds_->value());
    const int index = optionIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= options_.size()) return 0;
    return options_[static_cast<std::size_t>(index)].phases.front().duration;
}

sb::Tenths PenaltyDialog::phase2Tenths() const {
    if (editMode_) return fixedPhase2_;
    const int index = optionIndex();
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
    emit addRequested(optionIndex(), player_->text().trimmed());
    QDialog::accept();
}

int PenaltyDialog::optionIndex() const {
    for (std::size_t i = 0; i < chips_.size(); ++i)
        if (chips_[i]->isChecked()) return static_cast<int>(i);
    return -1;
}

void PenaltyDialog::selectOption(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= chips_.size()) return;
    chips_[static_cast<std::size_t>(index)]->setChecked(true);
}
