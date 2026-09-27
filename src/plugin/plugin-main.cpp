// SPDX-License-Identifier: GPL-2.0-or-later
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QPointer>

#include "ScoreboardDock.hpp"
#include "plugin-support.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr const char* kDockId = "obs-scoreboard-dock";

// OBS destroys the dock before obs_module_unload (lesson learned, CLAUDE.md §7): every Qt
// object held globally is a QPointer, so a late access finds null instead of freed memory.
QPointer<ScoreboardDock> g_dock;

void onFrontendEvent(enum obs_frontend_event event, void*) {
    if (event == OBS_FRONTEND_EVENT_EXIT) {
        // Phase 3: stop the clock driver and save the match state here, while Qt objects live.
    }
}

} // namespace

MODULE_EXPORT const char* obs_module_name(void) { return "ScoreBoard for OBS"; }

MODULE_EXPORT const char* obs_module_description(void) {
    return obs_module_text("Plugin.Description");
}

bool obs_module_load(void) {
    auto* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    auto* dock = new ScoreboardDock(mainWindow);
    if (!obs_frontend_add_dock_by_id(kDockId, obs_module_text("Dock.Title"), dock)) {
        delete dock;
        obs_log(LOG_ERROR, "could not register the dock");
        return false;
    }
    g_dock = dock;
    obs_frontend_add_event_callback(onFrontendEvent, nullptr);
    obs_log(LOG_INFO, "loaded (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void) {
    obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
    obs_log(LOG_INFO, "unloaded");
}
