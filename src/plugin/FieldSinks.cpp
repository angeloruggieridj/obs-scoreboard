// SPDX-License-Identifier: GPL-2.0-or-later
#include "FieldSinks.hpp"

#include <QMetaObject>

#include <util/base.h>
#include <util/platform.h>

#include <utility>

#include "plugin-support.h"

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

// One source, one field: two fields writing to the same source would show only the last one
// that changed, while the other sink believed its own text was still visible.
void FieldSinks::releaseFromOthers(sb::FieldId field) {
    const std::string uuid = sinks_[index(field)].binding().uuid;
    if (uuid.empty()) return;
    for (std::size_t j = 0; j < sinks_.size(); ++j) {
        if (j == index(field) || !sinks_[j].bound() || sinks_[j].binding().uuid != uuid) continue;
        sinks_[j].unbind();
        obs_log(LOG_INFO, "source moved from field %s to field %s",
                std::string(sb::fieldKey(static_cast<sb::FieldId>(j))).c_str(),
                std::string(sb::fieldKey(field)).c_str());
        emit bindingChanged(static_cast<int>(j));
    }
}

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
    if (ok) {
        releaseFromOthers(field);
        pushOne(field);
    }
    emit bindingChanged(static_cast<int>(field));
    return ok;
}

bool FieldSinks::bindTo(sb::FieldId field, const TextSink::Binding& binding) {
    const bool ok = sinks_[index(field)].bindTo(binding);
    if (ok) {
        releaseFromOthers(field);
        pushOne(field);
    }
    emit bindingChanged(static_cast<int>(field));
    return ok;
}

void FieldSinks::unbind(sb::FieldId field) {
    sinks_[index(field)].unbind();
    emit bindingChanged(static_cast<int>(field));
}

void FieldSinks::unbindAll() {
    for (std::size_t i = 0; i < sinks_.size(); ++i) {
        // A sink that lost its source still carries a problem: clear that too, or the link
        // indicator would stay yellow in the next collection.
        if (!sinks_[i].bound() && sinks_[i].problem() == TextSink::Problem::None) continue;
        sinks_[i].unbind(); // Problem::None
        emit bindingChanged(static_cast<int>(i));
    }
}

void FieldSinks::refreshAll() {
    for (std::size_t i = 0; i < sinks_.size(); ++i) {
        if (sinks_[i].refresh()) emit bindingChanged(static_cast<int>(i));
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
            // This signal is queued: a scene-collection switch can run in between and rebind the
            // fields to new sources that reuse the old uuids (a collection imported from a copy
            // of the current one). If a source with the uuid still exists and was not removed,
            // nothing was removed.
            OBSSourceAutoRelease live = obs_get_source_by_uuid(uuid.c_str());
            if (live && !obs_source_removed(live)) return;
            for (std::size_t i = 0; i < self->sinks_.size(); ++i) {
                const bool was = self->sinks_[i].bound();
                self->sinks_[i].sourceGone(uuid);
                if (was && !self->sinks_[i].bound()) emit self->bindingChanged(static_cast<int>(i));
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
                emit self->bindingChanged(static_cast<int>(i));
            }
        },
        Qt::QueuedConnection);
}
