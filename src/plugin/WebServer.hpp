// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTcpServer>

#include "WebRemote.hpp"

class ClockDriver;
class GlobalSettings;
class VendorApi;

// A small HTTP server for the remote page: it answers the page, the state of the match and, for a
// control link, the same commands the dock sends (through VendorApi, so there is one command path).
// Plain HTTP on the local network, off until the user turns it on; every request must carry a key
// of the view or control link, and a view key can never change anything.
//
// Everything runs on the UI thread: QTcpServer delivers its signals there, so a command is applied
// exactly as a click on the dock would be.
class WebServer : public QObject {
    Q_OBJECT
public:
    WebServer(ClockDriver* driver, VendorApi* api, GlobalSettings* settings,
              QObject* parent = nullptr);
    ~WebServer() override;

    // Starts, restarts or stops the listener to match the saved preferences. Makes the keys on the
    // first use.
    void apply();

    QString viewKey() const;
    QString controlKey() const;
    bool listening() const { return server_.isListening(); }
    // Why the port could not be opened ("" when it could, or when the server is off).
    QString error() const { return error_; }
    // The links to show, one per address of this computer ("http://192.168.1.20:8765/?k=...").
    QStringList links(bool control) const;
    // New random keys: every link given out before stops working.
    void regenerateKeys();
    // The user's own keys (see sb::validKey). False, with `*error` set, when they are not valid or
    // are the same; nothing changes then. An empty key asks for a random one.
    bool setKeys(const QString& viewKey, const QString& controlKey, QString* error = nullptr);

signals:
    void statusChanged();

private:
    struct Connection;
    void onNewConnection();
    void serve(Connection* connection);
    void handle(const sb::HttpRequest& request, Connection* connection);
    void reply(Connection* connection, int status, const char* contentType,
               const std::string& body);
    QString i18nJson() const;

    QPointer<ClockDriver> driver_;
    QPointer<VendorApi> api_;
    GlobalSettings* settings_ = nullptr; // lives for the whole module
    QTcpServer server_;
    QString error_;
    QByteArray fontBytes_; // the clock font, read on first use
    int open_ = 0;
};
