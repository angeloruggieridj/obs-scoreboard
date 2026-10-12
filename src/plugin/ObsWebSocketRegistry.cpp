// SPDX-License-Identifier: GPL-2.0-or-later
#include "ObsWebSocketRegistry.hpp"

#include <obs.h>
#include <obs.hpp>

#include <utility>
#include <vector>

#include "plugin-support.h"

namespace {
// Same layout as obs_websocket_request_callback in obs-websocket-api.h (obs-websocket copies it).
struct RequestCallback {
    void (*callback)(obs_data_t*, obs_data_t*, void*);
    void* priv_data;
};

// obs-websocket calls this from its own thread; the handler (VendorApi) hops to the UI thread and
// waits for the response JSON. `state` is shared and outlives the registry.
void vendorRequestCallback(obs_data_t* request, obs_data_t* response, void* priv) {
    auto* state = static_cast<ObsWebSocketRegistry::State*>(priv);
    if (!state || !state->alive.load() || !response) return;
    const char* json = request ? obs_data_get_json(request) : nullptr;
    const std::string body = state->handler(state->type, json ? json : "{}");
    OBSDataAutoRelease parsed = obs_data_create_from_json(body.c_str());
    if (parsed) obs_data_apply(response, parsed);
}

// States retired by unregisterAll(): kept alive until process end because obs-websocket may still
// be running a callback it copied (PerformVendorRequest calls it outside the lock). Bounded by the
// number of requests, one set per module load.
std::vector<std::shared_ptr<ObsWebSocketRegistry::State>>& retiredStates() {
    static std::vector<std::shared_ptr<ObsWebSocketRegistry::State>> v;
    return v;
}
} // namespace

ObsWebSocketRegistry::ObsWebSocketRegistry() = default;

ObsWebSocketRegistry::~ObsWebSocketRegistry() {
    unregisterAll();
}

bool ObsWebSocketRegistry::available() const {
    return procHandler() != nullptr;
}

void* ObsWebSocketRegistry::procHandler() const {
    if (phChecked_) return ph_;
    phChecked_ = true;
    proc_handler_t* global = obs_get_proc_handler();
    if (!global) return nullptr;
    calldata_t cd = {};
    if (!proc_handler_call(global, "obs_websocket_api_get_ph", &cd)) {
        calldata_free(&cd);
        return nullptr;
    }
    ph_ = calldata_ptr(&cd, "ph");
    calldata_free(&cd);
    return ph_;
}

bool ObsWebSocketRegistry::registerVendor() {
    proc_handler_t* ph = static_cast<proc_handler_t*>(procHandler());
    if (!ph) return false;
    calldata_t cd = {};
    calldata_set_string(&cd, "name", kVendorName);
    proc_handler_call(ph, "vendor_register", &cd);
    vendor_ = calldata_ptr(&cd, "vendor");
    calldata_free(&cd);
    if (!vendor_) {
        obs_log(LOG_DEBUG, "obs-websocket refused to register the vendor '%s'", kVendorName);
        return false;
    }
    return true;
}

bool ObsWebSocketRegistry::registerRequest(std::string_view type, Handler handler) {
    proc_handler_t* ph = static_cast<proc_handler_t*>(procHandler());
    if (!ph || !vendor_) return false;
    auto state = std::make_shared<State>();
    state->type = std::string(type);
    state->handler = std::move(handler);

    RequestCallback cb = {&vendorRequestCallback, state.get()};
    calldata_t cd = {};
    calldata_set_ptr(&cd, "vendor", vendor_);
    calldata_set_string(&cd, "type", state->type.c_str());
    calldata_set_ptr(&cd, "callback", &cb);
    const bool ok = proc_handler_call(ph, "vendor_request_register", &cd);
    const bool success = ok && calldata_bool(&cd, "success");
    calldata_free(&cd);
    if (success) entries_[state->type] = std::move(state);
    return success;
}

void ObsWebSocketRegistry::unregisterAll() {
    proc_handler_t* ph = static_cast<proc_handler_t*>(procHandler());
    for (auto& entry : entries_) {
        entry.second->alive.store(false);
        if (!ph || !vendor_) continue;
        calldata_t cd = {};
        calldata_set_ptr(&cd, "vendor", vendor_);
        calldata_set_string(&cd, "type", entry.first.c_str());
        proc_handler_call(ph, "vendor_request_unregister", &cd);
        calldata_free(&cd);
    }
    // Keep each State alive: obs-websocket may still be running a callback it copied.
    for (auto& entry : entries_) retiredStates().push_back(std::move(entry.second));
    entries_.clear();
}

bool ObsWebSocketRegistry::emitEvent(std::string_view type, std::string_view eventJson) {
    proc_handler_t* ph = static_cast<proc_handler_t*>(procHandler());
    if (!ph || !vendor_) return false;
    OBSDataAutoRelease data = obs_data_create_from_json(std::string(eventJson).c_str());
    calldata_t cd = {};
    calldata_set_ptr(&cd, "vendor", vendor_);
    calldata_set_string(&cd, "type", std::string(type).c_str());
    calldata_set_ptr(&cd, "data", data);
    proc_handler_call(ph, "vendor_event_emit", &cd);
    const bool success = calldata_bool(&cd, "success");
    calldata_free(&cd);
    return success;
}
