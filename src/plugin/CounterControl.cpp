// SPDX-License-Identifier: GPL-2.0-or-later
#include "CounterControl.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "DockStyle.hpp"
#include "PanelModel.hpp"

CounterControl::CounterControl(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);

    // The cells of a row touch each other: their borders form the separators of one segmented
    // group.
    top_ = new QHBoxLayout();
    top_->setSpacing(0);
    minus_ = makeButton(QStringLiteral("−1"), -1);
    top_->addWidget(minus_);
    value_ = new QLabel(this);
    value_->setAlignment(Qt::AlignCenter);
    value_->setMinimumWidth(32);
    value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dockstyle::setProp(value_, dockstyle::kRole, QStringLiteral("segval"));
    top_->addWidget(value_, 1);
    outer->addLayout(top_);

    extra_ = new QHBoxLayout();
    extra_->setSpacing(0);
    outer->addLayout(extra_);
    restyle();
}

QPushButton* CounterControl::makeButton(const QString& text, int delta) {
    auto* button = new QPushButton(text, this);
    button->setSizePolicy(QSizePolicy::Expanding,
                          QSizePolicy::Fixed); // QSS max-height is not enforced on buttons
    button->setFocusPolicy(Qt::TabFocus);
    dockstyle::setProp(button, dockstyle::kRole, QStringLiteral("seg"));
    connect(button, &QPushButton::clicked, this, [this, delta]() { emit deltaRequested(delta); });
    return button;
}

void CounterControl::restyle() {
    // First and last cell of each row get the rounded outer corners.
    const QString size = large_ ? QStringLiteral("large") : QString();
    for (QWidget* w : {static_cast<QWidget*>(minus_), static_cast<QWidget*>(value_)}) {
        dockstyle::setProp(w, dockstyle::kSize, size);
    }
    dockstyle::setProp(minus_, dockstyle::kPos, QStringLiteral("first"));
    const std::size_t topPlus = plusButtons_.empty() ? 0 : 1; // plusButtons_[0] ends the top row
    for (std::size_t i = 0; i < plusButtons_.size(); ++i) {
        QString pos;
        if (i == 0) {
            pos = QStringLiteral("last");
        } else {
            const std::size_t extras = plusButtons_.size() - topPlus;
            const std::size_t k = i - topPlus; // index inside the extra row
            pos =
                extras == 1
                    ? QStringLiteral("only")
                    : (k == 0 ? QStringLiteral("first")
                              : (k + 1 == extras ? QStringLiteral("last") : QStringLiteral("mid")));
        }
        dockstyle::setProp(plusButtons_[i], dockstyle::kPos, pos);
        dockstyle::setProp(plusButtons_[i], dockstyle::kSize, size);
    }
}

void CounterControl::setLarge(bool large) {
    if (large_ == large) return;
    large_ = large;
    restyle();
}

void CounterControl::setPlusValues(const std::vector<int>& values) {
    if (values == plusValues_) return;
    plusValues_ = values;
    qDeleteAll(plusButtons_); // deleting a widget removes it from its layout
    plusButtons_.clear();
    for (int v : plusValues_) {
        QPushButton* button = makeButton(QStringLiteral("+%1").arg(v), v);
        plusButtons_.push_back(button);
        if (plusButtons_.size() == 1)
            top_->addWidget(button); // the first plus ends the top row...
        else
            extra_->addWidget(button); // ...the others share a second row
    }
    restyle();
}

void CounterControl::setValue(int value) {
    if (shownValue_ && *shownValue_ == value) return;
    shownValue_ = value;
    value_->setText(QString::number(value));
}

void CounterControl::setColor(const QString& hex) {
    if (shownColor_ && *shownColor_ == hex) return;
    shownColor_ = hex;
    const auto rgb = sb::parseColor(hex.toStdString());
    if (hex.isEmpty() || !rgb) {
        value_->setStyleSheet(QString());
        return;
    }
    const char* text = sb::darkTextOn(*rgb) ? "#111111" : "#FFFFFF";
    // Only the fill and the text color: the border, size and font come from the shared style.
    value_->setStyleSheet(
        QStringLiteral("QLabel{background:%1;color:%2;}").arg(hex, QString::fromLatin1(text)));
}
