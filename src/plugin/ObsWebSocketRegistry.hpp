// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "VendorRegistry.hpp"

// The only place that talks to obs-websocket. The vendor API is header-only in obs-websocket and
// not part of libobs, so it is reached at run time through the OBS proc handler; every call fails
// cleanly (false/nullptr) when obs-websocket is not loaded, and the plugin keeps working.
class ObsWebSocketRegistry : public VendorRegistry {
public:
    static constexpr const char* kVendorName = "obs-scoreboard";

    ObsWebSocketRegistry();
    ~ObsWebSocketRegistry() override;
    ObsWebSocketRegistry(const ObsWebSocketRegistry&) = delete;
    ObsWebSocketRegistry& operator=(const ObsWebSocketRegistry&) = delete;

    // True when obs-websocket is loaded and exposes the vendor API proc handler.
    bool available() const;

    bool registerVendor() override;
    bool registerRequest(std::string_view type, Handler handler) override;
    bool emitEvent(std::string_view type, std::string_view eventJson) override;
    void unregisterAll() override;

    // Per-request callback state. Shared (std::shared_ptr) so it outlives the registry:
    // obs-websocket copies the callback struct and may invoke it while tearDown() runs, so `alive`
    // is checked before touching the handler.
    struct State {
        std::atomic<bool> alive{true};
        std::string type;
        Handler handler;
    };

private:
    void* procHandler() const;

    mutable void* ph_ = nullptr;
    mutable bool phChecked_ = false;
    void* vendor_ = nullptr;
    // std::map keeps each State's address stable across insertions, so priv can point at it.
    std::map<std::string, std::shared_ptr<State>> entries_;
};
