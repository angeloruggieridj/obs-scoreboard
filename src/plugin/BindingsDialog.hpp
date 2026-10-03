// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QPointer>

#include <array>

#include "Fields.hpp"

class FieldSinks;
class SourcePicker;

// "Source links": one row per scoreboard field, each with its Text source and a "create" button.
// Non-modal, so the clock and the dock stay usable while it is open.
class BindingsDialog : public QDialog {
    Q_OBJECT
public:
    BindingsDialog(FieldSinks* sinks, QWidget* parent = nullptr);

private:
    void choose(sb::FieldId field);
    void create(sb::FieldId field);

    QPointer<FieldSinks> sinks_;
    std::array<SourcePicker*, sb::kFieldCount> pickers_{};
};
