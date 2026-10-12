// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "Fields.hpp"

class ClockDriver;
class VendorRegistry;

// Bridges the obs-scoreboard vendor (obs-websocket) and the match engine: parses each request with
// the core codec, runs the command on the UI thread through ClockDriver, answers with the
// CommandResult, and turns ClockDriver's signals into the vendor events. It holds no game logic.
class VendorApi : public QObject {
    Q_OBJECT
public:
    using NewMatchFn = std::function<bool(const std::string& sportId)>;

    VendorApi(ClockDriver* driver, NewMatchFn newMatch, VendorRegistry* registry,
              QObject* parent = nullptr);
    ~VendorApi() override;

    // Registers the vendor and every request; false when obs-websocket is absent. UI thread only.
    bool start();

    // Runs one request and returns its response envelope as JSON. Safe from any thread: it hops to
    // the UI thread, waits at most kRequestTimeoutMs, and answers "notReady" on timeout/shutdown.
    QString handleRequest(const QString& type, const QString& requestJson);
    void emitEvent(const QString& type, const QString& json);

signals:
    void eventEmitted(const QString& type, const QString& json);

private:
    QString handleRequestUi(const QString& type, const QString& requestJson);
    void onFieldsChanged(const sb::FieldValues& fields);
    void onCommandApplied();
    void onMatchReplaced();
    void publishNewEvents();
    void publishScoreChanges(const sb::FieldValues& current);
    bool matchEnded() const; // reads the engine's event log (not game logic)
    // Named sendEvent, not emit: `emit` is a Qt macro and cannot be a member name.
    void sendEvent(const char* type, const std::string& json);

    QPointer<ClockDriver> driver_;
    NewMatchFn newMatch_;
    VendorRegistry* registry_ = nullptr;
    sb::FieldValues previousFields_{};
    std::size_t publishedEvents_ = 0;
};
