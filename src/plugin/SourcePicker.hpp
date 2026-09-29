// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QComboBox>

#include <optional>

#include "TextSink.hpp"

// Drop-down of the Text sources that exist right now: re-listed every time it opens, so a
// source created or renamed a moment ago is there. The first entry means "not linked".
class SourcePicker : public QComboBox {
public:
    explicit SourcePicker(QWidget* parent = nullptr);

    void showBinding(const TextSink& sink);
    std::optional<TextSink::Binding> selected() const;

protected:
    void showPopup() override;

private:
    void reload(const QString& keepUuid, const QString& keepName);
};
