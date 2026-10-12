// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <string>
#include <string_view>

// The transport boundary to obs-websocket, so VendorApi can be driven by a fake in the selftest.
// The real implementation (ObsWebSocketRegistry) talks to obs-websocket through the OBS proc
// handler; it is the only place that knows obs-websocket exists.
class VendorRegistry {
public:
    using Handler = std::function<std::string(std::string_view type, std::string_view requestJson)>;

    virtual ~VendorRegistry() = default;
    // Registers the vendor and one request type; false when obs-websocket is absent.
    virtual bool registerVendor() = 0;
    virtual bool registerRequest(std::string_view type, Handler handler) = 0;
    // Emits one vendor event; false when obs-websocket is absent.
    virtual bool emitEvent(std::string_view type, std::string_view eventJson) = 0;
    // Deregisters every registered request (obs-websocket has no vendor-unregister). Called from
    // the destructors so no callback survives the plugin unload (CLAUDE.md §7).
    virtual void unregisterAll() = 0;
};
