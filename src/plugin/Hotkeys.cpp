// SPDX-License-Identifier: GPL-2.0-or-later
#include "Hotkeys.hpp"

#include <obs-module.h>
#include <util/base.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
#include <QThread>

#include <atomic>
#include <deque>
#include <string>
#include <utility>

#include "ClockDriver.hpp"
#include "HotkeyCatalog.hpp"
#include "PanelLayout.hpp"
#include "plugin-support.h"

namespace {
constexpr std::size_t kNone = static_cast<std::size_t>(-1);
constexpr int kRepeatDelayMs = 400;
constexpr int kRepeatIntervalMs = 100;
constexpr const char* kHotkeysKey = "obs-scoreboard-hotkeys";

QString text(const std::string& key) {
    return QString::fromUtf8(obs_module_text(key.c_str()));
}
} // namespace

struct Hotkeys::Entry {
    Entry(Hotkeys* o, std::size_t i) : owner(o), index(i) {}
    std::atomic<Hotkeys*> owner;
    std::size_t index;
};

Hotkeys::Hotkeys(ClockDriver* driver, Dispatcher dispatch, QObject* parent)
    : QObject(parent), driver_(driver), dispatch_(std::move(dispatch)) {
    static std::deque<Entry> storage; // stable addresses
    repeat_.setSingleShot(true);
    connect(&repeat_, &QTimer::timeout, this, [this]() {
        if (repeating_ == kNone) return;
        fire(repeating_);
        repeat_.start(kRepeatIntervalMs);
    });

    const auto& catalog = sb::hotkeyCatalog();
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        const sb::HotkeySpec& spec = catalog[i];
        storage.emplace_back(this, i);
        Entry* entry = &storage.back();
        entries_.push_back(entry);
        const std::string actionKey = "Hotkey." + spec.action;
        QString description = text(actionKey);
        if (spec.team)
            description = QStringLiteral("%1 – %2").arg(
                text(*spec.team == sb::Team::Home ? "Hotkey.Team.Home" : "Hotkey.Team.Away"),
                description);
        const obs_hotkey_id id = obs_hotkey_register_frontend(
            spec.name.c_str(), description.toUtf8().constData(), &Hotkeys::onHotkey, entry);
        if (id == OBS_INVALID_HOTKEY_ID)
            obs_log(LOG_WARNING, "could not register hotkey %s", spec.name.c_str());
        ids_.push_back(id);
    }
}

Hotkeys::~Hotkeys() {
    stopRepeat();
    for (Entry* entry : entries_) entry->owner.store(nullptr);
    for (obs_hotkey_id id : ids_)
        if (id != OBS_INVALID_HOTKEY_ID) obs_hotkey_unregister(id);
}

obs_hotkey_id Hotkeys::idOf(std::string_view name) const {
    const auto& catalog = sb::hotkeyCatalog();
    for (std::size_t i = 0; i < catalog.size(); ++i)
        if (catalog[i].name == name) return ids_[i];
    return OBS_INVALID_HOTKEY_ID;
}

void Hotkeys::save(obs_data_t* data) const {
    OBSDataAutoRelease object = obs_data_create();
    const auto& catalog = sb::hotkeyCatalog();
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        if (ids_[i] == OBS_INVALID_HOTKEY_ID) continue;
        OBSDataArrayAutoRelease array = obs_hotkey_save(ids_[i]); // owned by the caller
        obs_data_set_array(object, catalog[i].name.c_str(), array);
    }
    obs_data_set_obj(data, kHotkeysKey, object);
}

void Hotkeys::load(obs_data_t* data) {
    stopRepeat(); // a key held across a collection switch must not keep repeating
    OBSDataAutoRelease object = obs_data_get_obj(data, kHotkeysKey);
    const auto& catalog = sb::hotkeyCatalog();
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        if (ids_[i] == OBS_INVALID_HOTKEY_ID) continue;
        OBSDataArrayAutoRelease array =
            object ? obs_data_get_array(object, catalog[i].name.c_str()) : nullptr;
        if (array)
            obs_hotkey_load(ids_[i], array);
        else
            obs_hotkey_load_bindings(ids_[i], nullptr, 0); // not in this collection: unassigned
    }
}

void Hotkeys::onHotkey(void* data, obs_hotkey_id, obs_hotkey_t*, bool pressed) {
    auto* entry = static_cast<Entry*>(data);
    Hotkeys* self = entry->owner.load();
    if (!self) return;
    const std::size_t index = entry->index;
    // libobs holds its hotkey mutex here: never run the command (it may open a modal window).
    QPointer<Hotkeys> weak(self);
    QMetaObject::invokeMethod(
        self,
        [weak, index, pressed]() {
            if (weak) weak->trigger(index, pressed);
        },
        Qt::QueuedConnection);
}

void Hotkeys::trigger(std::size_t index, bool pressed) {
    if (QThread::currentThread() != thread()) { // safety net: should never happen
        QPointer<Hotkeys> weak(this);
        QMetaObject::invokeMethod(
            this,
            [weak, index, pressed]() {
                if (weak) weak->trigger(index, pressed);
            },
            Qt::QueuedConnection);
        return;
    }
    const sb::HotkeySpec& spec = sb::hotkeyCatalog()[index];
    if (!pressed) {
        if (repeating_ == index) stopRepeat();
        return;
    }
    fire(index);
    if (spec.repeats) startRepeat(index);
}

void Hotkeys::fire(std::size_t index) {
    if (!driver_ || !dispatch_) return;
    const sb::HotkeySpec& spec = sb::hotkeyCatalog()[index];
    const sb::Command command = sb::resolveHotkey(spec, driver_->engine());
    if (!sb::isHotkeyApplicable(command, sb::panelLayout(driver_->engine().settings()))) return;
    dispatch_(command);
}

void Hotkeys::startRepeat(std::size_t index) {
    repeating_ = index;
    repeat_.start(kRepeatDelayMs);
}

void Hotkeys::stopRepeat() {
    repeating_ = kNone;
    repeat_.stop();
}
