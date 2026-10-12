// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "Fields.hpp"
#include "PluginState.hpp"

// Optional .txt output (spec §6, CLAUDE.md §4.12): one file per field, named after the field key,
// in a folder the user picks. OFF by default. Fed by the same fieldsChanged stream as the Text
// sinks (no timer, UI thread). A file is rewritten (atomically) only when its text changed; the
// files are never read back. An I/O error is reported once per failure streak.
class FileOutput : public QObject {
    Q_OBJECT
public:
    explicit FileOutput(QObject* parent = nullptr);

    // Applies the global preferences (enabled + folder). Enabling or changing the folder makes the
    // next publish rewrite every file, so a freshly enabled output is complete.
    void apply(const sb::GlobalPrefs& prefs);

    bool enabled() const { return enabled_; }
    const std::string& directory() const { return directory_; }
    // Files written since the output was last (re)enabled. Read by the selftest.
    std::size_t written() const { return written_; }

public slots:
    void publish(const sb::FieldValues& fields);

signals:
    // Emitted once when writes start failing; reset by the first pass that writes without errors.
    void outputError(const QString& message);
    void outputRecovered();

private:
    bool writeField(sb::FieldId field, const std::string& text);
    void reportFailure(const std::string& detail);

    bool enabled_ = false;
    std::string directory_;
    std::array<std::optional<std::string>, sb::kFieldCount> lastWritten_;
    bool failing_ = false;
    std::size_t written_ = 0;
};
