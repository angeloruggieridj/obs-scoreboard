// SPDX-License-Identifier: GPL-2.0-or-later
#include "CounterControl.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "PanelModel.hpp"

CounterControl::CounterControl(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);

    top_ = new QHBoxLayout();
    top_->setSpacing(2);
    top_->addWidget(makeButton(QStringLiteral("−1"), -1));
    value_ = new QLabel(this);
    value_->setAlignment(Qt::AlignCenter);
    value_->setMinimumWidth(40);
    value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFont font = value_->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.3);
    value_->setFont(font);
    top_->addWidget(value_, 1);
    outer->addLayout(top_);

    extra_ = new QHBoxLayout();
    extra_->setSpacing(2);
    outer->addLayout(extra_);
}

QPushButton* CounterControl::makeButton(const QString& text, int delta) {
    auto* button = new QPushButton(text, this);
    button->setMinimumWidth(28);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(button, &QPushButton::clicked, this, [this, delta]() { emit deltaRequested(delta); });
    return button;
}

void CounterControl::setPlusValues(const std::vector<int>& values) {
    if (values == plusValues_) return;
    plusValues_ = values;
    qDeleteAll(plusButtons_); // deleting a widget removes it from its layout
    plusButtons_.clear();
    for (int v : plusValues_) {
        QPushButton* button = makeButton(QStringLiteral("+%1").arg(v), v);
        plusButtons_.push_back(button);
        if (plusValues_.size() == 1)
            top_->addWidget(button);
        else
            extra_->addWidget(button);
    }
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
    value_->setStyleSheet(
        QStringLiteral("QLabel{background:%1;color:%2;border-radius:4px;padding:2px 6px;}")
            .arg(hex, QString::fromLatin1(text)));
}
