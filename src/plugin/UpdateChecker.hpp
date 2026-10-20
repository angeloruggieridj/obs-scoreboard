// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <string>
#include <thread>

// Asks GitHub, once per OBS start and whenever the user asks, whether a release newer than the
// running build exists, and says so with updateAvailable(). It only ever TELLS: nothing is
// downloaded or installed, so the plugin is never replaced under a running OBS.
//
// The request runs on a thread of its own (libcurl, which obs-deps ships and OBS itself loads:
// Qt Network would need a TLS backend this OBS does not have) and nothing on the UI thread waits
// for it. The destructor aborts a request in flight and joins, so unloading the module never
// leaves a thread behind. The version comparison and the reading of GitHub's answer are the pure
// core in UpdateCheck.hpp.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker() override;

    // Starts a check unless one is running. Returns at once. A `manual` check also reports when
    // there is nothing newer (noUpdate) or the question could not be asked (failed); an automatic
    // one only ever speaks up about a newer release.
    void start(const std::string& currentVersion, bool manual = false);

signals:
    // Emitted from the worker thread (delivered queued on the UI thread).
    // `notes` is the Markdown of every release newer than this build, newest first.
    void updateAvailable(QString version, QString url, QString notes);
    void noUpdate();
    void failed();

private:
    void run(std::string currentVersion, bool manual);

    std::thread thread_;
    std::atomic<bool> abort_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> manualAsked_{false}; // a manual check asked while an automatic one ran
};
