// SPDX-License-Identifier: GPL-2.0-or-later
#include "PenaltyList.hpp"

#include <obs-module.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString rowText(const sb::PenaltyRow& row) {
    const QString player =
        row.player.empty() ? QStringLiteral("?") : QString::fromStdString(row.player);
    const QString time = QString::fromStdString(row.time);
    const QString phase2 =
        row.phase2.empty() ? QString() : QStringLiteral(" ") + QString::fromStdString(row.phase2);
    if (row.queued)
        return QStringLiteral("%1  %2 %3%4")
            .arg(player, QString::fromUtf8(obs_module_text("Panel.Queued")), time, phase2);
    return QStringLiteral("%1  %2%3").arg(player, time, phase2);
}

} // namespace

PenaltyList::PenaltyList(QWidget* parent) : QWidget(parent) {
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(1);
}

void PenaltyList::ensureSlots(std::size_t count) {
    while (slots_.size() < count) {
        Slot slot;
        slot.box = new QWidget(this);
        auto* row = new QHBoxLayout(slot.box);
        row->setContentsMargins(0, 0, 0, 0);
        slot.label = new QLabel(slot.box);
        // Never let a long row (player + "queued"/"in coda" + "+2:00") widen the team column:
        // the label yields, the edit/cancel buttons keep their size.
        slot.label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        slot.label->setMinimumWidth(0);
        slot.edit = new QToolButton(slot.box);
        slot.edit->setText(QString::fromUtf8(obs_module_text("Panel.PenaltyEditShort")));
        slot.edit->setAutoRaise(true);
        slot.edit->setToolTip(QString::fromUtf8(obs_module_text("Panel.PenaltyEdit")));
        slot.cancel = new QToolButton(slot.box);
        slot.cancel->setText(QStringLiteral("X"));
        slot.cancel->setAutoRaise(true);
        slot.cancel->setToolTip(QString::fromUtf8(obs_module_text("Panel.PenaltyCancel")));
        row->addWidget(slot.label, 1);
        row->addWidget(slot.edit);
        row->addWidget(slot.cancel);
        slot.box->hide(); // explicit: setRows() decides which slots are shown
        layout_->addWidget(slot.box);
        const std::size_t index = slots_.size();
        connect(slot.edit, &QToolButton::clicked, this,
                [this, index]() { emit editRequested(slots_[index].id); });
        connect(slot.cancel, &QToolButton::clicked, this,
                [this, index]() { emit cancelRequested(slots_[index].id); });
        slots_.push_back(slot);
    }
}

void PenaltyList::setRows(const std::vector<sb::PenaltyRow>& rows) {
    ensureSlots(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        Slot& slot = slots_[i];
        slot.id = rows[i].id;
        const QString text = rowText(rows[i]);
        if (slot.label->text() != text) slot.label->setText(text);
        if (i >= shown_) slot.box->show();
    }
    for (std::size_t i = rows.size(); i < shown_; ++i) slots_[i].box->hide();
    shown_ = rows.size();
}
