// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <string>
#include <thread>

// Asks GitHub, once per OBS start, whether a release newer than the running build exists, and says
// so with updateAvailable(). It only ever TELLS: nothing is downloaded or installed, so the plugin
// is never replaced under a running OBS.
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

    // Starts the check unless one already ran or runs. Returns at once.
    void start(const std::string& currentVersion);

signals:
    // Emitted at most once, from the worker thread (delivered queued on the UI thread).
    void updateAvailable(QString version, QString url);

private:
    void run(std::string currentVersion);

    std::thread thread_;
    std::atomic<bool> abort_{false};
    bool started_ = false;
};
