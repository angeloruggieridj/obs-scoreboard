// SPDX-License-Identifier: GPL-2.0-or-later
#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QMainWindow>
#include <QPointer>

#include "ClockDriver.hpp"
#include "FieldSinks.hpp"
#include "ScoreboardDock.hpp"
#include "SportCatalog.hpp"
#include "plugin-support.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr const char* kDockId = "obs-scoreboard-dock";
// Temporary: until Phase 5 adds match settings, where the user picks the sport.
constexpr const char* kDefaultSport = "futsal";

// OBS destroys the dock before obs_module_unload: every Qt object held globally is a
// QPointer, so a late access finds null instead of freed memory.
QPointer<ClockDriver> g_driver;
QPointer<FieldSinks> g_sinks;
QPointer<ScoreboardDock> g_dock;

void tearDown() {
    if (g_driver) g_driver->shutdown();
    delete g_sinks.data();
    delete g_driver.data();
}

void onFrontendEvent(enum obs_frontend_event event, void*) {
    switch (event) {
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
        if (g_sinks) g_sinks->unbindAll(); // Phase 4 reloads the bindings of the new collection
        break;
    case OBS_FRONTEND_EVENT_EXIT:
        tearDown(); // while Qt and libobs are both still alive
        break;
    default:
        break;
    }
}

} // namespace

MODULE_EXPORT const char* obs_module_name(void) {
    return "ScoreBoard for OBS";
}

MODULE_EXPORT const char* obs_module_description(void) {
    return obs_module_text("Plugin.Description");
}

bool obs_module_load(void) {
    const sb::SportPreset* sport = sb::findSport(kDefaultSport);
    if (!sport) {
        obs_log(LOG_ERROR, "built-in sport '%s' is missing", kDefaultSport);
        return false;
    }
    g_driver = new ClockDriver(sb::MatchSettings::forSport(*sport));
    g_sinks = new FieldSinks();
    QObject::connect(g_driver, &ClockDriver::fieldsChanged, g_sinks, &FieldSinks::publish);
    g_sinks->publish(g_driver->fields());

    auto* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    auto* dock = new ScoreboardDock(g_driver, g_sinks, mainWindow);
    if (!obs_frontend_add_dock_by_id(kDockId, obs_module_text("Dock.Title"), dock)) {
        delete dock;
        tearDown();
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
    tearDown(); // no-op when EXIT already ran
    obs_log(LOG_INFO, "unloaded");
}
