// SPDX-License-Identifier: GPL-2.0-or-later
#include "FileOutput.hpp"

#include <obs-module.h>

#include <filesystem>
#include <system_error>

#include "AtomicFile.hpp"
#include "plugin-support.h"

namespace {

std::filesystem::path fileFor(const std::string& directory, sb::FieldId field) {
    return std::filesystem::u8path(directory) /
           std::filesystem::u8path(std::string(sb::fieldKey(field)) + ".txt");
}

} // namespace

FileOutput::FileOutput(QObject* parent) : QObject(parent) {}

void FileOutput::apply(const sb::GlobalPrefs& prefs) {
    const bool changed = prefs.fileOutputEnabled != enabled_ || prefs.fileOutputDir != directory_;
    enabled_ = prefs.fileOutputEnabled;
    directory_ = prefs.fileOutputDir;
    if (changed) {
        lastWritten_.fill(std::nullopt); // a newly enabled output rewrites every file once
        failing_ = false;
        written_ = 0;
    }
    if (enabled_ && !directory_.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::u8path(directory_), ec);
    }
}

void FileOutput::publish(const sb::FieldValues& fields) {
    if (!enabled_ || directory_.empty()) return;
    bool failed = false;
    for (std::size_t i = 0; i < sb::kFieldCount; ++i) {
        if (lastWritten_[i] && *lastWritten_[i] == fields[i]) continue;
        if (writeField(static_cast<sb::FieldId>(i), fields[i]))
            lastWritten_[i] = fields[i];
        else
            failed = true;
    }
    // The latch is per pass: one persistently bad file keeps it armed, so the error is not
    // re-emitted on every fieldsChanged (and a recovered output is announced exactly once).
    if (!failed && failing_) {
        failing_ = false;
        emit outputRecovered();
    }
}

bool FileOutput::writeField(sb::FieldId field, const std::string& text) {
    std::string error;
    if (!writeFileAtomic(fileFor(directory_, field).u8string(), text, &error)) {
        reportFailure(error);
        return false;
    }
    ++written_;
    return true;
}

void FileOutput::reportFailure(const std::string& detail) {
    if (failing_) return; // once per pass: never one message per field
    failing_ = true;
    obs_log(LOG_WARNING, "file output: %s", detail.c_str());
    emit outputError(
        QString::fromUtf8(obs_module_text("Output.Error")).arg(QString::fromStdString(detail)));
}
