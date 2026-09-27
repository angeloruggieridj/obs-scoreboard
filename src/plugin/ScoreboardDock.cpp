// SPDX-License-Identifier: GPL-2.0-or-later
#include "ScoreboardDock.hpp"

#include <obs-module.h>
#include <QLabel>
#include <QVBoxLayout>

ScoreboardDock::ScoreboardDock(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* placeholder = new QLabel(QString::fromUtf8(obs_module_text("Dock.Placeholder")), this);
    placeholder->setWordWrap(true);
    layout->addWidget(placeholder);
    layout->addStretch(1);
}
