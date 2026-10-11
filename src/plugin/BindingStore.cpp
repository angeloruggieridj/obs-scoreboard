// SPDX-License-Identifier: GPL-2.0-or-later
#include "BindingStore.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/base.h>

#include <string>
#include <vector>

#include "FieldSinks.hpp"
#include "Hotkeys.hpp"
#include "PluginState.hpp"
#include "plugin-support.h"

namespace {
constexpr const char* kKey = "obs-scoreboard";
}

BindingStore::BindingStore(FieldSinks* sinks, Hotkeys* hotkeys) : sinks_(sinks), hotkeys_(hotkeys) {
    obs_frontend_add_save_callback(&BindingStore::onSave, this);
}

BindingStore::~BindingStore() {
    obs_frontend_remove_save_callback(&BindingStore::onSave, this);
}

void BindingStore::onSave(obs_data_t* data, bool saving, void* param) {
    auto* self = static_cast<BindingStore*>(param);
    if (saving)
        self->save(data);
    else
        self->load(data);
}

void BindingStore::save(obs_data_t* data) const {
    if (hotkeys_) hotkeys_->save(data);
    if (!sinks_) return;
    sinks_->refreshAll(); // uuids change when a collection is duplicated
    std::vector<sb::FieldBinding> bindings;
    for (std::size_t i = 0; i < sb::kFieldCount; ++i) {
        const auto field = static_cast<sb::FieldId>(i);
        const TextSink& sink = sinks_->sink(field);
        if (!sink.bound()) continue;
        bindings.push_back({field, sink.binding().uuid, sink.binding().name});
    }
    OBSDataAutoRelease obj = obs_data_create_from_json(sb::bindingsToJson(bindings).c_str());
    obs_data_set_obj(data, kKey, obj);
}

void BindingStore::load(obs_data_t* data) {
    if (hotkeys_) hotkeys_->load(data); // first: the early returns below must not skip it
    if (!sinks_) return;
    sinks_->unbindAll();
    OBSDataAutoRelease obj = obs_data_get_obj(data, kKey);
    if (!obj) return; // a collection that never had our bindings
    std::string error;
    const auto bindings = sb::bindingsFromJson(obs_data_get_json(obj), &error);
    if (!bindings) {
        obs_log(LOG_WARNING, "ignoring the scene collection's bindings: %s", error.c_str());
        return;
    }
    for (const sb::FieldBinding& b : *bindings) {
        if (!sinks_->bindTo(b.field, {b.uuid, b.name}))
            obs_log(LOG_INFO,
                    "binding of %s not restored: source '%s' is missing or not a Text source",
                    std::string(sb::fieldKey(b.field)).c_str(), b.name.c_str());
    }
}
