// SPDX-License-Identifier: GPL-2.0-or-later
#include "WebServer.hpp"

#include <obs-module.h>

#include <QDateTime>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QTimer>

#include <array>
#include <string>

#include "ClockDriver.hpp"
#include "GlobalSettings.hpp"
#include "SportCatalog.hpp"
#include "VendorApi.hpp"
#include "WebPage.hpp"
#include "WebState.hpp"
#include "plugin-support.h"

namespace {

constexpr int kMaxOpenConnections = 32;
constexpr int kRequestTimeoutMs = 8000;

// The texts the page shows, by locale key.
constexpr std::array<const char*, 36> kPageKeys = {
    "Dock.Start",
    "Dock.Stop",
    "Dock.Reset",
    "Panel.Shots",
    "Panel.Penalties",
    "Panel.Queued",
    "Panel.PenaltyCancel",
    "Panel.Of",
    "Panel.Segment.Period",
    "Panel.Segment.Quarter",
    "Panel.Segment.Half",
    "Panel.Segment.Segment",
    "Panel.Score.Goal",
    "Panel.Score.Points",
    "Panel.Score.Score",
    "Panel.Fouls.Fouls",
    "Panel.Fouls.Yellow",
    "Panel.Fouls.Flags",
    "Panel.Fouls2.Fouls2",
    "Panel.Fouls2.Red",
    "Reason.clock.atLimit",
    "Reason.clock.running",
    "Reason.match.ended",
    "Reason.period.clockRunning",
    "Reason.period.midPeriod",
    "Reason.period.noNext",
    "Reason.period.noPrev",
    "Reason.stat.disabled",
    "Reason.penalty.notFound",
    "Reason.plugin.shuttingDown",
    "Web.Lost",
    "Web.ViewOnly",
    "Web.Control",
    "Web.Vibration",
    "Web.On",
    "Web.Off",
};

std::string newKey() {
    std::array<unsigned char, 12> bytes{};
    for (unsigned char& b : bytes)
        b = static_cast<unsigned char>(QRandomGenerator::system()->bounded(256));
    return sb::keyFromBytes(bytes);
}

} // namespace

struct WebServer::Connection {
    QTcpSocket* socket = nullptr;
    QTimer* timeout = nullptr;
    std::string buffer;
    bool replied = false;
    bool streaming = false;
    QString peer;         // address of the device
    bool control = false; // for a stream: which state it is sent
};

WebServer::WebServer(ClockDriver* driver, VendorApi* api, GlobalSettings* settings, QObject* parent)
    : QObject(parent), driver_(driver), api_(api), settings_(settings), server_(this) {
    connect(&server_, &QTcpServer::newConnection, this, &WebServer::onNewConnection);
    // The page is told about every change at once instead of asking again and again. A short
    // delay groups the several signals one command makes into one push.
    broadcastTimer_.setSingleShot(true);
    broadcastTimer_.setInterval(25);
    connect(&broadcastTimer_, &QTimer::timeout, this, &WebServer::broadcast);
    keepAliveTimer_.setInterval(15000);
    connect(&keepAliveTimer_, &QTimer::timeout, this, [this]() {
        for (Connection* stream : streams_) stream->socket->write(": ping\n\n");
    });
    keepAliveTimer_.start();
    clientsTimer_.setInterval(1000);
    connect(&clientsTimer_, &QTimer::timeout, this, &WebServer::refreshClients);
    clientsTimer_.start();
    if (driver_) {
        connect(driver_, &ClockDriver::fieldsChanged, this,
                [this](const sb::FieldValues&) { scheduleBroadcast(); });
        connect(driver_, &ClockDriver::runningChanged, this, [this](bool) { scheduleBroadcast(); });
        connect(driver_, &ClockDriver::commandApplied, this, [this]() { scheduleBroadcast(); });
        connect(driver_, &ClockDriver::matchReplaced, this, [this]() { scheduleBroadcast(); });
    }
}

WebServer::~WebServer() {
    closeStreams();
    server_.close();
}

// A device counts while it has a stream open or asked for something in the last 8 seconds.
void WebServer::note(Connection* connection, bool control) {
    Seen& seen = seen_[connection->peer];
    seen.at = QDateTime::currentMSecsSinceEpoch();
    seen.control = seen.control || control;
    if (connection->streaming) connection->control = control;
}

void WebServer::refreshClients() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QHash<QString, bool> devices; // address -> controls
    for (auto it = seen_.begin(); it != seen_.end();) {
        if (now - it->at > 8000) {
            it = seen_.erase(it);
            continue;
        }
        devices[it.key()] = it->control;
        ++it;
    }
    for (Connection* stream : streams_)
        devices[stream->peer] = devices.value(stream->peer, false) || stream->control;
    int controllers = 0;
    int viewers = 0;
    for (auto it = devices.begin(); it != devices.end(); ++it)
        (it.value() ? controllers : viewers)++;
    if (!server_.isListening()) controllers = viewers = 0;
    if (controllers == controllers_ && viewers == viewers_) return;
    controllers_ = controllers;
    viewers_ = viewers;
    emit clientsChanged();
}

void WebServer::scheduleBroadcast() {
    if (!streams_.isEmpty() && !broadcastTimer_.isActive()) broadcastTimer_.start();
}

// Sends the current state to every open stream (the view state and the control state are each
// built once).
void WebServer::broadcast() {
    if (streams_.isEmpty() || !driver_ || !settings_ || driver_->isShutDown()) return;
    const sb::GlobalPrefs& prefs = settings_->prefs();
    std::string json[2];
    for (Connection* stream : streams_) {
        std::string& text = json[stream->control ? 1 : 0];
        if (text.empty())
            text = "data: " + sb::webStateJson(driver_->engine(), stream->control, prefs) + "\n\n";
        // A phone that stopped reading must not make the plugin hold more and more bytes.
        if (stream->socket->bytesToWrite() > 256 * 1024) {
            stream->socket->disconnectFromHost();
            continue;
        }
        stream->socket->write(text.data(), static_cast<qint64>(text.size()));
    }
}

void WebServer::startStream(Connection* connection, bool canControl) {
    connection->replied = true; // nothing else will be answered on this connection
    connection->streaming = true;
    connection->control = canControl;
    connection->timeout->stop();
    const std::string head = sb::httpStreamHead();
    connection->socket->write(head.data(), static_cast<qint64>(head.size()));
    connection->socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    streams_.append(connection);
    refreshClients();
    const std::string first =
        "data: " + sb::webStateJson(driver_->engine(), canControl, settings_->prefs()) + "\n\n";
    connection->socket->write(first.data(), static_cast<qint64>(first.size()));
}

// Streams are closed whenever who may watch changes: a closed key must not keep a window open.
void WebServer::closeStreams() {
    const QList<Connection*> open = streams_;
    for (Connection* stream : open) stream->socket->disconnectFromHost();
    seen_.clear();
    refreshClients();
}

void WebServer::apply() {
    if (!settings_) return;
    const sb::GlobalPrefs& prefs = settings_->prefs();
    if (prefs.remoteMode == sb::RemoteMode::Off) {
        const bool was = server_.isListening();
        closeStreams();
        server_.close();
        error_.clear();
        if (was) obs_log(LOG_INFO, "remote page stopped");
        emit statusChanged();
        return;
    }
    if (prefs.remoteViewKey.empty() || prefs.remoteControlKey.empty())
        settings_->setRemoteKeys(newKey(), newKey());
    const int port = settings_->prefs().remotePort;
    if (server_.isListening() && server_.serverPort() == port) {
        closeStreams(); // the mode may have gone from control to view
        emit statusChanged();
        return;
    }
    closeStreams();
    server_.close();
    if (server_.listen(QHostAddress::Any, static_cast<quint16>(port))) {
        error_.clear();
        obs_log(LOG_INFO, "remote page listening on port %d", port);
    } else {
        error_ = server_.errorString();
        obs_log(LOG_WARNING, "remote page: port %d could not be opened: %s", port,
                qPrintable(error_));
    }
    emit statusChanged();
}

bool WebServer::setKeys(const QString& viewKey, const QString& controlKey, QString* error) {
    if (!settings_) return false;
    const std::string view =
        viewKey.trimmed().isEmpty() ? newKey() : viewKey.trimmed().toStdString();
    const std::string control =
        controlKey.trimmed().isEmpty() ? newKey() : controlKey.trimmed().toStdString();
    if (!sb::validKey(view) || !sb::validKey(control)) {
        if (error) *error = QStringLiteral("invalid");
        return false;
    }
    if (view == control) {
        if (error) *error = QStringLiteral("same");
        return false;
    }
    settings_->setRemoteKeys(view, control);
    closeStreams();
    emit statusChanged();
    return true;
}

QString WebServer::viewKey() const {
    return settings_ ? QString::fromStdString(settings_->prefs().remoteViewKey) : QString();
}

QString WebServer::controlKey() const {
    return settings_ ? QString::fromStdString(settings_->prefs().remoteControlKey) : QString();
}

void WebServer::regenerateKeys() {
    if (!settings_) return;
    settings_->setRemoteKeys(newKey(), newKey());
    closeStreams();
    emit statusChanged();
}

QStringList WebServer::links(bool control) const {
    QStringList out;
    if (!settings_ || !server_.isListening()) return out;
    const sb::GlobalPrefs& prefs = settings_->prefs();
    if (control && prefs.remoteMode != sb::RemoteMode::Control) return out;
    const QString key =
        QString::fromStdString(control ? prefs.remoteControlKey : prefs.remoteViewKey);
    QStringList addresses;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) ||
            (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries())
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                addresses << entry.ip().toString();
    }
    if (addresses.isEmpty()) addresses << QStringLiteral("127.0.0.1");
    for (const QString& address : addresses)
        out << QStringLiteral("http://%1:%2/?k=%3").arg(address).arg(prefs.remotePort).arg(key);
    return out;
}

void WebServer::onNewConnection() {
    while (QTcpSocket* socket = server_.nextPendingConnection()) {
        if (open_ >= kMaxOpenConnections) {
            socket->write(sb::httpResponse(503, "text/plain; charset=utf-8", "Busy").c_str());
            socket->disconnectFromHost();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            continue;
        }
        ++open_;
        auto* connection = new Connection();
        connection->socket = socket;
        {
            QString peer = socket->peerAddress().toString();
            if (peer.startsWith(QLatin1String("::ffff:"))) peer = peer.mid(7);
            connection->peer = peer;
        }
        connection->timeout = new QTimer(socket);
        connection->timeout->setSingleShot(true);
        connect(connection->timeout, &QTimer::timeout, this, [this, connection]() {
            if (!connection->replied)
                reply(connection, 408, "text/plain; charset=utf-8", "Timeout");
        });
        connection->timeout->start(kRequestTimeoutMs);
        connect(socket, &QTcpSocket::readyRead, this, [this, connection]() {
            if (connection->replied) {
                connection->socket->readAll(); // a stream has nothing more to read
                return;
            }
            const QByteArray data = connection->socket->readAll();
            connection->buffer.append(data.constData(), static_cast<std::size_t>(data.size()));
            serve(connection);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, connection]() {
            --open_;
            streams_.removeOne(connection);
            QTimer::singleShot(0, this, &WebServer::refreshClients);
            connection->timeout->stop();
            connection->socket->deleteLater();
            delete connection;
        });
    }
}

void WebServer::serve(Connection* connection) {
    sb::HttpRequest request;
    switch (sb::parseHttpRequest(connection->buffer, request)) {
    case sb::ParseStatus::Incomplete:
        return;
    case sb::ParseStatus::Bad:
        reply(connection, 400, "text/plain; charset=utf-8", "Bad request");
        return;
    case sb::ParseStatus::TooLarge:
        reply(connection, 413, "text/plain; charset=utf-8", "Too large");
        return;
    case sb::ParseStatus::Ready:
        handle(request, connection);
        return;
    }
}

void WebServer::handle(const sb::HttpRequest& request, Connection* connection) {
    static const char* const kText = "text/plain; charset=utf-8";
    static const char* const kJson = "application/json; charset=utf-8";
    if (!settings_ || !driver_ || driver_->isShutDown()) {
        reply(connection, 503, kText, "Not ready");
        return;
    }
    const sb::GlobalPrefs& prefs = settings_->prefs();
    const std::string provided = sb::queryParam(request.query, "k").value_or(std::string());
    const sb::WebAccess access =
        sb::authorizeKey(prefs.remoteMode, prefs.remoteViewKey, prefs.remoteControlKey, provided);
    const sb::WebRoute route = sb::routeWebRequest(request, access);
    if (access != sb::WebAccess::None && route.kind != sb::WebRoute::Kind::Font &&
        route.kind != sb::WebRoute::Kind::Favicon) {
        note(connection, access == sb::WebAccess::Control);
        if (route.kind == sb::WebRoute::Kind::Page) refreshClients();
    }
    using Kind = sb::WebRoute::Kind;
    switch (route.kind) {
    case Kind::Page:
        reply(connection, 200, "text/html; charset=utf-8",
              webPageHtml(route.canControl, pageTexts().toStdString()));
        return;
    case Kind::State:
        reply(connection, 200, kJson, sb::webStateJson(driver_->engine(), route.canControl, prefs));
        return;
    case Kind::Events:
        startStream(connection, route.canControl);
        return;
    case Kind::Command: {
        if (!api_) {
            reply(connection, 503, kText, "Not ready");
            return;
        }
        const QString body =
            request.body.empty() ? QStringLiteral("{}") : QString::fromStdString(request.body);
        const QString answer = api_->handleRequest(QString::fromStdString(route.requestType), body);
        reply(connection, 200, kJson, answer.toStdString());
        return;
    }
    case Kind::Font: {
        if (access == sb::WebAccess::None) {
            reply(connection, 401, kText, "This link is not valid.");
            return;
        }
        if (fontBytes_.isNull()) {
            char* path = obs_module_file("fonts/DSEG7Classic-Bold.ttf");
            if (path) {
                QFile file(QString::fromUtf8(path));
                if (file.open(QIODevice::ReadOnly)) fontBytes_ = file.readAll();
                bfree(path);
            }
        }
        if (fontBytes_.isEmpty())
            reply(connection, 404, kText, "Not found");
        else
            reply(connection, 200, "font/ttf",
                  std::string(fontBytes_.constData(), static_cast<std::size_t>(fontBytes_.size())));
        return;
    }
    case Kind::Favicon:
        reply(connection, 204, kText, std::string());
        return;
    case Kind::Unauthorized:
        reply(connection, 401, kText, "This link is not valid.");
        return;
    case Kind::Forbidden:
        reply(connection, 403, kText, "This link can only watch.");
        return;
    case Kind::MethodNotAllowed:
        reply(connection, 405, kText, "Method not allowed");
        return;
    case Kind::NotFound:
        reply(connection, 404, kText, "Not found");
        return;
    }
}

void WebServer::reply(Connection* connection, int status, const char* contentType,
                      const std::string& body) {
    if (connection->replied) return;
    connection->replied = true;
    const std::string answer = sb::httpResponse(status, contentType, body);
    connection->socket->write(answer.data(), static_cast<qint64>(answer.size()));
    connection->socket->disconnectFromHost();
}

QString WebServer::pageTexts() {
    QJsonObject texts;
    for (const char* key : kPageKeys)
        texts.insert(QString::fromLatin1(key), QString::fromUtf8(obs_module_text(key)));
    for (const sb::SportPreset& sport : sb::builtInSports()) {
        const std::string key = "Sport." + sport.id;
        texts.insert(QString::fromStdString(key), QString::fromUtf8(obs_module_text(key.c_str())));
    }
    QString json = QString::fromUtf8(QJsonDocument(texts).toJson(QJsonDocument::Compact));
    // The text sits inside a <script>: nothing in it may close the element.
    json.replace(QLatin1Char('<'), QStringLiteral("\\u003c"));
    return json;
}
