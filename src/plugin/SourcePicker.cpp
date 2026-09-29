// SPDX-License-Identifier: GPL-2.0-or-later
#include "SourcePicker.hpp"

#include <obs-module.h>

#include <QSignalBlocker>

#include <algorithm>
#include <utility>
#include <vector>

namespace {

struct Entry {
    QString name;
    QString uuid;
};

bool collectTextSource(void* param, obs_source_t* source) {
    if (!TextSink::isTextSource(source)) return true;
    auto* out = static_cast<std::vector<Entry>*>(param);
    out->push_back({QString::fromUtf8(obs_source_get_name(source)),
                    QString::fromUtf8(obs_source_get_uuid(source))});
    return true;
}

} // namespace

SourcePicker::SourcePicker(QWidget* parent) : QComboBox(parent) {
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    reload(QString(), QString());
}

void SourcePicker::showBinding(const TextSink& sink) {
    const QString uuid = sink.bound() ? QString::fromStdString(sink.binding().uuid) : QString();
    const QString name = sink.bound() ? QString::fromStdString(sink.binding().name) : QString();
    reload(uuid, name);
}

std::optional<TextSink::Binding> SourcePicker::selected() const {
    const QString uuid = currentData().toString();
    if (uuid.isEmpty()) return std::nullopt;
    return TextSink::Binding{uuid.toStdString(), currentText().toStdString()};
}

void SourcePicker::showPopup() {
    reload(currentData().toString(), currentText());
    QComboBox::showPopup();
}

void SourcePicker::reload(const QString& keepUuid, const QString& keepName) {
    std::vector<Entry> entries;
    obs_enum_sources(&collectTextSource, &entries);
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.name.localeAwareCompare(b.name) < 0; });

    const QSignalBlocker block(this);
    clear();
    addItem(QString::fromUtf8(obs_module_text("Dock.NoSource")), QString());
    int keep = 0;
    for (const Entry& e : entries) {
        addItem(e.name, e.uuid);
        if (!keepUuid.isEmpty() && e.uuid == keepUuid) keep = count() - 1;
    }
    if (keep == 0 && !keepUuid.isEmpty()) { // bound to a source OBS no longer lists
        addItem(keepName, keepUuid);
        keep = count() - 1;
    }
    setCurrentIndex(keep);
}
