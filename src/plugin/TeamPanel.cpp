// SPDX-License-Identifier: GPL-2.0-or-later
#include "TeamPanel.hpp"

#include <obs-module.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <cstring>
#include <string>

#include "Counters.hpp"
#include "CounterControl.hpp"
#include "DockStyle.hpp"
#include "PenaltyList.hpp"

namespace {

QString text(const std::string& key) {
    return QString::fromUtf8(obs_module_text(key.c_str()));
}

void setIfChanged(QLabel* label, const QString& value) {
    if (label->text() != value) label->setText(value);
}

// Small grey caption above a control; set through setCaption() so the text is upper-cased once.
QLabel* makeCaption(QWidget* parent, Qt::Alignment alignment = Qt::AlignCenter) {
    auto* label = new QLabel(parent);
    label->setAlignment(alignment);
    dockstyle::setProp(label, dockstyle::kText, QStringLiteral("caption"));
    return label;
}

void setCaption(QLabel* label, const QString& value) {
    setIfChanged(label, value.toUpper());
}

} // namespace

TeamPanel::TeamPanel(QWidget* parent) : QFrame(parent) {
    dockstyle::setProp(this, dockstyle::kCard, QStringLiteral("team"));
    dockstyle::setProp(this, dockstyle::kTeam, QStringLiteral("home"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    name_ = new QLineEdit(this);
    name_->setAlignment(Qt::AlignCenter);
    dockstyle::setProp(name_, dockstyle::kText, QStringLiteral("name"));
    connect(name_, &QLineEdit::editingFinished, this, [this]() { emit nameEdited(name_->text()); });
    layout->addWidget(name_);

    scoreCaption_ = makeCaption(this);
    layout->addWidget(scoreCaption_);
    score_ = new CounterControl(this);
    score_->setLarge(true);
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

    auto* penaltiesRow = new QHBoxLayout();
    penaltiesCaption_ = makeCaption(this, Qt::AlignLeft | Qt::AlignVCenter);
    penaltiesAdd_ = new dockstyle::IconButton(dockstyle::Icon::Plus, this);
    dockstyle::setProp(penaltiesAdd_, dockstyle::kRole, QStringLiteral("small"));
    penaltiesAdd_->setObjectName(QStringLiteral("penaltyAdd"));
    penaltiesAdd_->setToolTip(QString::fromUtf8(obs_module_text("Panel.PenaltyAdd")));
    connect(penaltiesAdd_, &QPushButton::clicked, this, &TeamPanel::penaltyAddRequested);
    penaltiesRow->addWidget(penaltiesCaption_, 1);
    penaltiesRow->addWidget(penaltiesAdd_);
    penaltiesCaption_->hide();
    penaltiesAdd_->hide();
    penalties_ = new PenaltyList(this);
    penalties_->hide();
    connect(penalties_, &PenaltyList::cancelRequested, this, &TeamPanel::penaltyCancelRequested);
    connect(penalties_, &PenaltyList::editRequested, this, &TeamPanel::penaltyEditRequested);
    layout->addLayout(penaltiesRow);
    layout->addWidget(penalties_);
    layout->addStretch(1);
}

void TeamPanel::setSide(bool away) {
    dockstyle::setProp(this, dockstyle::kTeam,
                       away ? QStringLiteral("away") : QStringLiteral("home"));
}

void TeamPanel::setAccent(const QString& hex) {
    if (accent_ == hex) return;
    accent_ = hex;
    // The rule is scoped to the card, so the penalty cards inside keep their own look.
    setStyleSheet(hex.isEmpty()
                      ? QString()
                      : QStringLiteral("QFrame[sbCard=\"team\"]{border-top-color:%1;}").arg(hex));
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
    setCaption(scoreCaption_, text(scoreKey));
    score_->setPlusValues(layout.scoreValues);
    setCaption(shots_.caption, text("Panel.Shots"));
    setCaption(fouls_.caption, text(foulsKey));
    setCaption(fouls2_.caption, text(fouls2Key));
    setCaption(penaltiesCaption_, text("Panel.Penalties"));
    shots_.counter->setPlusValues({1});
    fouls_.counter->setPlusValues({1});
    fouls2_.counter->setPlusValues({1});
    setRowShown(shots_, layout.shots);
    setRowShown(fouls_, layout.fouls);
    setRowShown(fouls2_, layout.fouls2);
    if (penaltiesShown_ != layout.penalties) {
        penaltiesShown_ = layout.penalties;
        penaltiesCaption_->setVisible(penaltiesShown_);
        penaltiesAdd_->setVisible(penaltiesShown_);
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
