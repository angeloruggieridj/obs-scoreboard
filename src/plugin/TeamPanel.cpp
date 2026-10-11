// SPDX-License-Identifier: GPL-2.0-or-later
#include "TeamPanel.hpp"

#include <obs-module.h>

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <cstring>
#include <string>

#include "Counters.hpp"
#include "CounterControl.hpp"
#include "PenaltyList.hpp"

namespace {

QString text(const std::string& key) {
    return QString::fromUtf8(obs_module_text(key.c_str()));
}

void setIfChanged(QLabel* label, const QString& value) {
    if (label->text() != value) label->setText(value);
}

QLabel* makeCaption(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setAlignment(Qt::AlignCenter);
    QFont font = label->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 0.8);
    label->setFont(font);
    return label;
}

} // namespace

TeamPanel::TeamPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(1);

    name_ = new QLineEdit(this);
    name_->setAlignment(Qt::AlignCenter);
    connect(name_, &QLineEdit::editingFinished, this, [this]() { emit nameEdited(name_->text()); });
    layout->addWidget(name_);

    scoreCaption_ = makeCaption(this);
    layout->addWidget(scoreCaption_);
    score_ = new CounterControl(this);
    connect(score_, &CounterControl::deltaRequested, this,
            [this](int delta) { emit statDelta(static_cast<int>(sb::Stat::Score), delta); });
    layout->addWidget(score_);

    shots_ = makeRow(static_cast<int>(sb::Stat::Shots));
    fouls_ = makeRow(static_cast<int>(sb::Stat::Fouls));
    fouls2_ = makeRow(static_cast<int>(sb::Stat::Fouls2));
    for (Row* row : {&shots_, &fouls_, &fouls2_}) {
        layout->addWidget(row->caption);
        layout->addWidget(row->counter);
    }

    penaltiesCaption_ = makeCaption(this);
    penaltiesCaption_->hide();
    penalties_ = new PenaltyList(this);
    penalties_->hide();
    connect(penalties_, &PenaltyList::cancelRequested, this, &TeamPanel::penaltyCancelRequested);
    layout->addWidget(penaltiesCaption_);
    layout->addWidget(penalties_);
    layout->addStretch(1);
}

TeamPanel::Row TeamPanel::makeRow(int stat) {
    Row row;
    row.caption = makeCaption(this);
    row.counter = new CounterControl(this);
    row.caption->hide();
    row.counter->hide();
    connect(row.counter, &CounterControl::deltaRequested, this,
            [this, stat](int delta) { emit statDelta(stat, delta); });
    return row;
}

void TeamPanel::setRowShown(Row& row, bool shown) {
    if (row.shown == shown) return;
    row.shown = shown;
    row.caption->setVisible(shown);
    row.counter->setVisible(shown);
}

void TeamPanel::applyLayout(const sb::PanelLayout& layout) {
    if (applied_ && *applied_ == layout) return;
    applied_ = layout;
    // Built at run time, so the locale-key checker must not see a literal prefix: see the plan's
    // convention for runtime keys (Global Constraints; Task 5.13).
    const std::string scoreKey = "Panel.Score." + layout.scoreLabel;
    const std::string foulsKey = "Panel.Fouls." + layout.foulsLabel;
    const std::string fouls2Key = "Panel.Fouls2." + layout.fouls2Label;
    setIfChanged(scoreCaption_, text(scoreKey));
    score_->setPlusValues(layout.scoreValues);
    setIfChanged(shots_.caption, text("Panel.Shots"));
    setIfChanged(fouls_.caption, text(foulsKey));
    setIfChanged(fouls2_.caption, text(fouls2Key));
    setIfChanged(penaltiesCaption_, text("Panel.Penalties"));
    shots_.counter->setPlusValues({1});
    fouls_.counter->setPlusValues({1});
    fouls2_.counter->setPlusValues({1});
    setRowShown(shots_, layout.shots);
    setRowShown(fouls_, layout.fouls);
    setRowShown(fouls2_, layout.fouls2);
    if (penaltiesShown_ != layout.penalties) {
        penaltiesShown_ = layout.penalties;
        penaltiesCaption_->setVisible(penaltiesShown_);
        penalties_->setVisible(penaltiesShown_);
    }
}

void TeamPanel::setName(const QString& name) {
    if (name_->text() == name || name_->hasFocus()) return;
    name_->setText(name);
}

void TeamPanel::setCounts(int score, int shots, int fouls, int fouls2) {
    score_->setValue(score);
    shots_.counter->setValue(shots);
    fouls_.counter->setValue(fouls);
    fouls2_.counter->setValue(fouls2);
}

void TeamPanel::setColors(const QString& scoreHex, const QString& foulHex) {
    score_->setColor(scoreHex);
    fouls_.counter->setColor(foulHex);
    fouls2_.counter->setColor(foulHex);
}

void TeamPanel::setPenalties(const std::vector<sb::PenaltyRow>& rows) {
    penalties_->setRows(rows);
}

std::size_t TeamPanel::penaltyRowCount() const {
    return penalties_->shownCount();
}

bool TeamPanel::rowShown(const char* id) const {
    if (std::strcmp(id, "shots") == 0) return shots_.shown;
    if (std::strcmp(id, "fouls") == 0) return fouls_.shown;
    if (std::strcmp(id, "fouls2") == 0) return fouls2_.shown;
    if (std::strcmp(id, "penalties") == 0) return penaltiesShown_;
    return false;
}
