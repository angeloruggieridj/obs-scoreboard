// SPDX-License-Identifier: GPL-2.0-or-later
#include "BindingsDialog.hpp"
#include "DockStyle.hpp"

#include <obs-module.h>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <string>

#include "FieldSinks.hpp"
#include "SourcePicker.hpp"
#include "TextSourceFactory.hpp"

namespace {

QString text(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

QString fieldLabel(sb::FieldId field) {
    const std::string key = "Field." + std::string(sb::fieldKey(field));
    return QString::fromUtf8(obs_module_text(key.c_str()));
}

} // namespace

BindingsDialog::BindingsDialog(FieldSinks* sinks, QWidget* parent)
    : QDialog(parent), sinks_(sinks) {
    setWindowTitle(text("Bindings.Title"));
    setAttribute(Qt::WA_DeleteOnClose);
    auto* outer = new QVBoxLayout(this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* page = new QWidget(scroll);
    auto* form = new QFormLayout(page);
    for (std::size_t i = 0; i < sb::kFieldCount; ++i) {
        const auto field = static_cast<sb::FieldId>(i);
        auto* row = new QHBoxLayout();
        auto* picker = new SourcePicker(page);
        auto* add = new dockstyle::IconButton(dockstyle::Icon::Plus, page);
        dockstyle::setProp(add, dockstyle::kRole, QStringLiteral("icon"));
        add->setToolTip(text("Bindings.Create"));
        row->addWidget(picker, 1);
        row->addWidget(add);
        form->addRow(fieldLabel(field), row);
        pickers_[i] = picker;
        connect(picker, &QComboBox::activated, this, [this, field](int) { choose(field); });
        connect(add, &QPushButton::clicked, this, [this, field]() { create(field); });
        if (sinks_) picker->showBinding(sinks_->sink(field));
    }
    scroll->setWidget(page);
    outer->addWidget(scroll);
    resize(560, 520);
    dockstyle::styleDialog(this);
    if (sinks_) {
        connect(sinks_, &FieldSinks::bindingChanged, this, [this](int index) {
            if (index < 0 || static_cast<std::size_t>(index) >= sb::kFieldCount || !sinks_) return;
            pickers_[static_cast<std::size_t>(index)]->showBinding(
                sinks_->sink(static_cast<sb::FieldId>(index)));
        });
    }
    resize(520, 640);
}

void BindingsDialog::choose(sb::FieldId field) {
    if (!sinks_) return;
    const auto binding = pickers_[static_cast<std::size_t>(field)]->selected();
    if (binding)
        sinks_->bindTo(field, *binding);
    else
        sinks_->unbind(field);
}

void BindingsDialog::create(sb::FieldId field) {
    if (!sinks_) return;
    std::string error;
    const QString name = text("Bindings.SourceName").arg(fieldLabel(field));
    OBSSourceAutoRelease source = createTextSource(name.toStdString(), &error);
    if (!source) {
        QMessageBox::warning(this, text("Bindings.Title"),
                             text("Bindings.CreateFailed").arg(QString::fromStdString(error)));
        return;
    }
    sinks_->bind(field, source);
}
