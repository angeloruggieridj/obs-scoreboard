// SPDX-License-Identifier: GPL-2.0-or-later
#include "PenaltyList.hpp"

#include <obs-module.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "DockStyle.hpp"

namespace {

// The thin "time left" bar under a running penalty. Painted by hand: a style sheet for
// QProgressBar loses against the theme's own rules, and this needs no theme at all (a translucent
// track, one fixed accent).
class MeterBar : public QWidget {
public:
    explicit MeterBar(QWidget* parent) : QWidget(parent) { setFixedHeight(3); }
    int value() const { return value_; }
    void setValue(int value) {
        if (value_ == value) return;
        value_ = value;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(128, 128, 128, 70));
        p.fillRect(QRect(0, 0, width() * value_ / 100, height()),
                   QColor(QStringLiteral("#EF7D57")));
    }

private:
    int value_ = 100;
};

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
    layout_->setSpacing(4);
}

void PenaltyList::ensureSlots(std::size_t count) {
    while (slots_.size() < count) {
        Slot slot;
        slot.box = new QFrame(this);
        dockstyle::setProp(slot.box, dockstyle::kCard, QStringLiteral("pen"));
        auto* column = new QVBoxLayout(slot.box);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);
        auto* rowWidget = new QWidget(slot.box);
        auto* row = new QHBoxLayout(rowWidget);
        row->setContentsMargins(6, 2, 3, 2);
        row->setSpacing(3);
        slot.label = new QLabel(rowWidget);
        // Never let a long row (player + "queued"/"in coda" + "+2:00") widen the team column:
        // the label yields, the edit/cancel buttons keep their size.
        slot.label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        slot.label->setMinimumWidth(0);
        slot.edit = new dockstyle::IconButton(dockstyle::Icon::Pencil, rowWidget);
        slot.edit->setToolTip(QString::fromUtf8(obs_module_text("Panel.PenaltyEdit")));
        dockstyle::setProp(slot.edit, dockstyle::kRole, QStringLiteral("small"));
        slot.cancel = new dockstyle::IconButton(dockstyle::Icon::Cross, rowWidget);
        slot.cancel->setToolTip(QString::fromUtf8(obs_module_text("Panel.PenaltyCancel")));
        dockstyle::setProp(slot.cancel, dockstyle::kRole, QStringLiteral("small"));
        row->addWidget(slot.label, 1);
        row->addWidget(slot.edit);
        row->addWidget(slot.cancel);
        slot.meter = new MeterBar(slot.box);
        column->addWidget(rowWidget);
        column->addWidget(slot.meter);
        slot.box->hide(); // explicit: setRows() decides which slots are shown
        layout_->addWidget(slot.box);
        const std::size_t index = slots_.size();
        connect(slot.edit, &QPushButton::clicked, this,
                [this, index]() { emit editRequested(slots_[index].id); });
        connect(slot.cancel, &QPushButton::clicked, this,
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
        if (slot.queued != rows[i].queued || i >= shown_) {
            slot.queued = rows[i].queued;
            // A queued penalty has not started: dimmed, and no bar yet.
            dockstyle::setProp(slot.label, dockstyle::kText,
                               slot.queued ? QStringLiteral("muted") : QStringLiteral("strong"));
            slot.meter->setVisible(!slot.queued);
        }
        static_cast<MeterBar*>(slot.meter)->setValue(rows[i].percent);
        if (i >= shown_) slot.box->show();
    }
    for (std::size_t i = rows.size(); i < shown_; ++i) slots_[i].box->hide();
    shown_ = rows.size();
}
