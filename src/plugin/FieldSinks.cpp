// SPDX-License-Identifier: GPL-2.0-or-later
#include "FieldSinks.hpp"

#include <QMetaObject>

#include <util/platform.h>

#include <utility>

namespace {

std::size_t index(sb::FieldId field) {
    return static_cast<std::size_t>(field);
}

// The uuid is copied inside the signal: by the time the UI thread runs, the source may be gone.
std::string uuidOf(calldata_t* cd) {
    auto* source = static_cast<obs_source_t*>(calldata_ptr(cd, "source"));
    const char* uuid = source ? obs_source_get_uuid(source) : nullptr;
    return uuid ? std::string(uuid) : std::string();
}

} // namespace

FieldSinks::FieldSinks(QObject* parent) : QObject(parent) {
    signal_handler_t* sh = obs_get_signal_handler();
    signal_handler_connect(sh, "source_destroy", &FieldSinks::onSourceGone, this);
    signal_handler_connect(sh, "source_remove", &FieldSinks::onSourceGone, this);
    signal_handler_connect(sh, "source_rename", &FieldSinks::onSourceRenamed, this);
}

FieldSinks::~FieldSinks() {
    signal_handler_t* sh = obs_get_signal_handler();
    signal_handler_disconnect(sh, "source_destroy", &FieldSinks::onSourceGone, this);
    signal_handler_disconnect(sh, "source_remove", &FieldSinks::onSourceGone, this);
    signal_handler_disconnect(sh, "source_rename", &FieldSinks::onSourceRenamed, this);
}

bool FieldSinks::bind(sb::FieldId field, obs_source_t* source) {
    const bool ok = sinks_[index(field)].bind(source);
    if (ok) pushOne(field);
    emit bindingChanged(field);
    return ok;
}

bool FieldSinks::bindTo(sb::FieldId field, const TextSink::Binding& binding) {
    const bool ok = sinks_[index(field)].bindTo(binding);
    if (ok) pushOne(field);
    emit bindingChanged(field);
    return ok;
}

void FieldSinks::unbind(sb::FieldId field) {
    sinks_[index(field)].unbind();
    emit bindingChanged(field);
}

void FieldSinks::unbindAll() {
    for (std::size_t i = 0; i < sinks_.size(); ++i) {
        if (!sinks_[i].bound()) continue;
        sinks_[i].unbind();
        emit bindingChanged(static_cast<sb::FieldId>(i));
    }
}

const TextSink& FieldSinks::sink(sb::FieldId field) const {
    return sinks_[index(field)];
}

void FieldSinks::setPushObserver(PushObserver observer) {
    observer_ = std::move(observer);
}

void FieldSinks::publish(const sb::FieldValues& fields) {
    latest_ = fields;
    for (std::size_t i = 0; i < sinks_.size(); ++i) pushOne(static_cast<sb::FieldId>(i));
}

void FieldSinks::pushOne(sb::FieldId field) {
    const std::string& text = latest_[index(field)];
    if (sinks_[index(field)].push(text) && observer_) observer_(field, text, os_gettime_ns());
}

void FieldSinks::onSourceGone(void* data, calldata_t* cd) {
    auto* self = static_cast<FieldSinks*>(data);
    std::string uuid = uuidOf(cd);
    if (uuid.empty()) return;
    QMetaObject::invokeMethod(
        self,
        [self, uuid]() {
            for (std::size_t i = 0; i < self->sinks_.size(); ++i) {
                const bool was = self->sinks_[i].bound();
                self->sinks_[i].sourceGone(uuid);
                if (was && !self->sinks_[i].bound())
                    emit self->bindingChanged(static_cast<sb::FieldId>(i));
            }
        },
        Qt::QueuedConnection);
}

void FieldSinks::onSourceRenamed(void* data, calldata_t* cd) {
    auto* self = static_cast<FieldSinks*>(data);
    std::string uuid = uuidOf(cd);
    const char* newName = calldata_string(cd, "new_name");
    if (uuid.empty() || !newName) return;
    std::string name = newName;
    QMetaObject::invokeMethod(
        self,
        [self, uuid, name]() {
            for (std::size_t i = 0; i < self->sinks_.size(); ++i) {
                if (self->sinks_[i].binding().uuid != uuid) continue;
                self->sinks_[i].sourceRenamed(uuid, name);
                emit self->bindingChanged(static_cast<sb::FieldId>(i));
            }
        },
        Qt::QueuedConnection);
}
