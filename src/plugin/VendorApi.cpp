// SPDX-License-Identifier: GPL-2.0-or-later
#include "VendorApi.hpp"

#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ClockDriver.hpp"
#include "SettingsChange.hpp"
#include "VendorProtocol.hpp"
#include "VendorRegistry.hpp"

namespace {
// Every request name of the protocol (see docs/remote-control.md).
constexpr std::array<const char*, 20> kRequests = {"ClockToggle",   "ClockStart",
                                                   "ClockStop",     "ClockReset",
                                                   "ClockAdjust",   "ClockSet",
                                                   "PeriodNext",    "PeriodPrev",
                                                   "AddStat",       "SetTeamName",
                                                   "PenaltyAdd",    "PenaltyEdit",
                                                   "PenaltyCancel", "PenaltyCancelActive",
                                                   "StoppageSet",   "StoppageAdd",
                                                   "EndMatch",      "NewMatch",
                                                   "GetState",      "GetFields"};
// How long a vendor request waits for the UI thread before answering "notReady".
constexpr std::chrono::milliseconds kRequestTimeoutMs{3000};

// Shared between the calling thread and the UI-thread lambda: it outlives VendorApi, so a request
// in flight when the object is destroyed resolves instead of touching freed memory.
struct RequestSlot {
    std::promise<QString> promise;
    std::atomic<bool> done{false};
};
} // namespace

VendorApi::VendorApi(ClockDriver* driver, NewMatchFn newMatch, VendorRegistry* registry,
                     QObject* parent)
    : QObject(parent), driver_(driver), newMatch_(std::move(newMatch)), registry_(registry) {}

VendorApi::~VendorApi() {
    if (driver_) disconnect(driver_, nullptr, this, nullptr);
    // Deregister before the registry or VendorApi disappear: no callback may outlive them.
    if (registry_) registry_->unregisterAll();
}

bool VendorApi::start() {
    if (!registry_ || !registry_->registerVendor()) return false;
    // The transport may run a handler from its own thread after tearDown() deleted this object (the
    // real registry retires the callback while obs-websocket finishes with it): capture a QPointer,
    // not a raw `this`, so a late call answers "notReady" instead of touching freed memory.
    for (const char* type : kRequests) {
        const QPointer<VendorApi> guard(this);
        registry_->registerRequest(type, [guard](std::string_view t, std::string_view json) {
            if (!guard) return sb::vendorErrorJson("notReady");
            return guard
                ->handleRequest(QString::fromUtf8(t.data(), static_cast<int>(t.size())),
                                QString::fromUtf8(json.data(), static_cast<int>(json.size())))
                .toStdString();
        });
    }
    if (driver_) {
        previousFields_ = driver_->fields();
        publishedEvents_ = driver_->engine().events().events().size();
        connect(driver_, &ClockDriver::fieldsChanged, this,
                [this](const sb::FieldValues& f) { onFieldsChanged(f); });
        connect(driver_, &ClockDriver::commandApplied, this, [this]() { onCommandApplied(); });
        connect(driver_, &ClockDriver::matchReplaced, this, [this]() { onMatchReplaced(); });
    }
    return true;
}

QString VendorApi::handleRequest(const QString& type, const QString& requestJson) {
    if (QThread::currentThread() == thread()) return handleRequestUi(type, requestJson);
    auto slot = std::make_shared<RequestSlot>();
    std::future<QString> future = slot->promise.get_future();
    // QPointer, not a raw `this`: a queued call may run after VendorApi is destroyed.
    const QPointer<VendorApi> guard(this);
    // QueuedConnection, never BlockingQueuedConnection: a non-cancellable block on obs-websocket's
    // thread could deadlock while OBS shuts down. The lambda touches only `slot` and `guard`.
    const bool posted = QMetaObject::invokeMethod(
        this,
        [guard, slot, type, requestJson]() {
            slot->promise.set_value(guard
                                        ? guard->handleRequestUi(type, requestJson)
                                        : QString::fromStdString(sb::vendorErrorJson("notReady")));
            slot->done.store(true);
        },
        Qt::QueuedConnection);
    if (!posted) return QString::fromStdString(sb::vendorErrorJson("notReady"));
    if (future.wait_for(kRequestTimeoutMs) != std::future_status::ready)
        return QString::fromStdString(sb::vendorErrorJson("notReady"));
    return future.get();
}

QString VendorApi::handleRequestUi(const QString& type, const QString& requestJson) {
    if (!driver_ || driver_->isShutDown())
        return QString::fromStdString(sb::vendorErrorJson("notReady"));
    const sb::VendorRequest request =
        sb::parseVendorRequest(type.toStdString(), requestJson.toStdString());
    switch (request.kind) {
    case sb::VendorRequest::Kind::Invalid:
        return QString::fromStdString(sb::vendorErrorJson(request.errorCode));
    case sb::VendorRequest::Kind::GetFields:
        return QString::fromStdString(sb::vendorFieldsJson(driver_->fields()));
    case sb::VendorRequest::Kind::GetState:
        return QString::fromStdString(sb::vendorStateJson(driver_->engine()));
    case sb::VendorRequest::Kind::NewMatch:
        // A live match is destroyed: ask for the same confirmation the dock asks for.
        if (sb::matchStarted(driver_->engine()) && !request.confirmed)
            return QString::fromStdString(
                sb::vendorResultJson(sb::CommandResult::confirm("match.inProgress")));
        return QString::fromStdString(newMatch_ && newMatch_(request.sport)
                                          ? sb::vendorResultJson(sb::CommandResult::ok())
                                          : sb::vendorErrorJson("unknownSport"));
    case sb::VendorRequest::Kind::Command:
        return QString::fromStdString(sb::vendorResultJson(driver_->apply(request.command)));
    }
    return QString::fromStdString(sb::vendorErrorJson("badRequest"));
}

void VendorApi::emitEvent(const QString& type, const QString& json) {
    if (registry_) registry_->emitEvent(type.toStdString(), json.toStdString());
    emit eventEmitted(type, json);
}

void VendorApi::sendEvent(const char* type, const std::string& json) {
    emitEvent(QString::fromLatin1(type), QString::fromStdString(json));
}

void VendorApi::onFieldsChanged(const sb::FieldValues& fields) {
    // replaceMatch() emits fieldsChanged before matchReplaced (ClockDriver.cpp): that first push
    // belongs to the new match and is published as a full StateChanged by onMatchReplaced, so it
    // must not be diffed against the old match here.
    if (!driver_ || driver_->isReplacingMatch()) return;
    sendEvent("StateChanged",
              sb::vendorStateChangedJson(previousFields_, fields,
                                         driver_->engine().clock().running(), matchEnded()));
    publishScoreChanges(fields);
    previousFields_ = fields;
    publishNewEvents();
}

void VendorApi::onCommandApplied() {
    publishNewEvents();
}

void VendorApi::onMatchReplaced() {
    if (!driver_) return;
    // A new match may have changed everything, "" fields included: publish all 28 and forget the
    // old events BEFORE publishing, so a resume does not re-announce them (item 5).
    previousFields_ = sb::FieldValues{};
    publishedEvents_ = driver_->engine().events().events().size();
    sendEvent("StateChanged",
              sb::vendorFullStateJson(driver_->fields(), driver_->engine().clock().running(),
                                      matchEnded()));
    publishedEvents_ = driver_->engine().events().events().size();
    previousFields_ = driver_->fields();
}

void VendorApi::publishScoreChanges(const sb::FieldValues& current) {
    if (!driver_) return;
    const auto value = [](const sb::FieldValues& f, sb::FieldId id) {
        return f[static_cast<std::size_t>(id)];
    };
    const int home = driver_->engine().counters().get(sb::Team::Home, sb::Stat::Score);
    const int away = driver_->engine().counters().get(sb::Team::Away, sb::Stat::Score);
    if (value(current, sb::FieldId::HomeScore) != value(previousFields_, sb::FieldId::HomeScore))
        sendEvent("ScoreChanged", sb::vendorScoreChangedJson(sb::Team::Home, home, home, away));
    if (value(current, sb::FieldId::AwayScore) != value(previousFields_, sb::FieldId::AwayScore))
        sendEvent("ScoreChanged", sb::vendorScoreChangedJson(sb::Team::Away, away, home, away));
}

bool VendorApi::matchEnded() const {
    if (!driver_) return false;
    const std::vector<sb::MatchEvent>& events = driver_->engine().events().events();
    return std::any_of(events.begin(), events.end(),
                       [](const sb::MatchEvent& e) { return e.type == sb::EventType::MatchEnd; });
}

void VendorApi::publishNewEvents() {
    if (!driver_) return;
    const std::vector<sb::MatchEvent>& events = driver_->engine().events().events();
    for (std::size_t i = publishedEvents_; i < events.size(); ++i) {
        const sb::MatchEvent& e = events[i];
        if (e.type == sb::EventType::PeriodEnd)
            sendEvent("PeriodEnded", sb::vendorPeriodEndedJson(e));
        else if (e.type == sb::EventType::MatchEnd)
            sendEvent("MatchEnded", sb::vendorMatchEndedJson(e));
    }
    publishedEvents_ = events.size();
}
