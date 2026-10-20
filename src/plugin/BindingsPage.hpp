// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QPointer>

#include <array>

#include "Fields.hpp"

class FieldSinks;
class SourcePicker;

// "Source links": one row per scoreboard field, each with its Text source and a "create" button.
// A page of the settings window; changes apply at once.
class BindingsPage : public QWidget {
    Q_OBJECT
public:
    explicit BindingsPage(FieldSinks* sinks, QWidget* parent = nullptr);

private:
    void choose(sb::FieldId field);
    void create(sb::FieldId field);

    QPointer<FieldSinks> sinks_;
    std::array<SourcePicker*, sb::kFieldCount> pickers_{};
};
