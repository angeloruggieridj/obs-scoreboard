// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <obs.h> // obs-hotkey.h alone is not self-contained
#include <obs.hpp>

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

#include "MatchEngine.hpp"

class ClockDriver;

// Registers the fixed hotkey set (sb::hotkeyCatalog()) with OBS and turns each press into a
// command for the dock. libobs calls the callbacks with its hotkey mutex held, possibly from its
// own thread, so a press is only queued here and runs later on the UI thread.
class Hotkeys : public QObject {
    Q_OBJECT
public:
    using Dispatcher = std::function<void(const sb::Command&)>;

    Hotkeys(ClockDriver* driver, Dispatcher dispatch, QObject* parent = nullptr);
    ~Hotkeys() override;

    obs_hotkey_id idOf(std::string_view name) const;

    // Bindings are stored per scene collection, under "obs-scoreboard-hotkeys". Loading a
    // collection without an entry for a hotkey leaves that hotkey unassigned.
    void save(obs_data_t* data) const;
    void load(obs_data_t* data);

private:
    struct Entry;
    static void onHotkey(void* data, obs_hotkey_id id, obs_hotkey_t* hotkey, bool pressed);
    void trigger(std::size_t index, bool pressed);
    void fire(std::size_t index);
    void startRepeat(std::size_t index);
    void stopRepeat();

    QPointer<ClockDriver> driver_;
    Dispatcher dispatch_;
    std::vector<obs_hotkey_id> ids_; // parallel to sb::hotkeyCatalog()
    std::vector<Entry*> entries_;
    QTimer repeat_;
    std::size_t repeating_ = static_cast<std::size_t>(-1);
};
