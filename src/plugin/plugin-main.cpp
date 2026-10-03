// SPDX-License-Identifier: GPL-2.0-or-later
#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QMainWindow>
#include <QPointer>

#include <filesystem>
#include <memory>
#include <system_error>

#include "AtomicFile.hpp"
#include "BindingStore.hpp"
#include "ClockDriver.hpp"
#include "FieldSinks.hpp"
#include "GlobalSettings.hpp"
#include "MatchStateSaver.hpp"
#include "ScoreboardDock.hpp"
#include "SelfTest.hpp"
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
QPointer<MatchStateSaver> g_saver;
QPointer<ScoreboardDock> g_dock;
GlobalSettings g_settings;
std::unique_ptr<BindingStore> g_bindings;
// After EXIT OBS has already cleared the frontend callbacks; removing ours would log an error.
bool g_exited = false;

void tearDown() {
    selftest::stop();      // the video callback must go before the driver and sinks
    delete g_saver.data(); // before the driver it listens to
    if (g_driver) g_driver->shutdown();
    g_bindings.reset(); // unregisters its save callback before the sinks go
    delete g_sinks.data();
    delete g_driver.data();
}

void onFrontendEvent(enum obs_frontend_event event, void*) {
    switch (event) {
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
        // After the left collection was saved (CHANGING comes before the save); the next
        // collection's bindings are restored by BindingStore on load.
        if (g_sinks) g_sinks->unbindAll();
        break;
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        selftest::maybeStart(g_driver, g_sinks);
        break;
    case OBS_FRONTEND_EVENT_EXIT:
        g_exited = true;
        if (g_saver) g_saver->flush();
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

// The previous session's match becomes previous-match.json before the saver can write: "Resume"
// then keeps working after the new session has already started the clock.
static void rotateLastMatch() {
    const std::string last = moduleConfigPath("last-match.json");
    const std::string previous = moduleConfigPath("previous-match.json");
    if (last.empty() || previous.empty()) return;
    std::error_code ec;
    const auto lastPath = std::filesystem::u8path(last);
    if (!std::filesystem::exists(lastPath, ec)) return;
    std::filesystem::rename(lastPath, std::filesystem::u8path(previous), ec);
    if (ec) obs_log(LOG_WARNING, "could not keep the previous match: %s", ec.message().c_str());
}

bool obs_module_load(void) {
    rotateLastMatch();
    g_settings.load();
    const sb::SportPreset* sport = sb::findSport(g_settings.prefs().lastSport);
    if (!sport) {
        obs_log(LOG_WARNING, "saved sport '%s' is unknown, using '%s'",
                g_settings.prefs().lastSport.c_str(), kDefaultSport);
        sport = sb::findSport(kDefaultSport);
    }
    if (!sport) {
        obs_log(LOG_ERROR, "built-in sport '%s' is missing", kDefaultSport);
        return false;
    }
    g_driver = new ClockDriver(sb::MatchSettings::forSport(*sport));
    g_sinks = new FieldSinks();
    g_saver = new MatchStateSaver(g_driver, moduleConfigPath("last-match.json"));
    g_bindings = std::make_unique<BindingStore>(g_sinks);
    QObject::connect(g_driver, &ClockDriver::fieldsChanged, g_sinks, &FieldSinks::publish);
    g_sinks->publish(g_driver->fields());

    auto* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    auto* dock = new ScoreboardDock(g_driver, g_sinks, &g_settings, g_saver, mainWindow);
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
    if (!g_exited) obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
    tearDown(); // no-op when EXIT already ran
    obs_log(LOG_INFO, "unloaded");
}
