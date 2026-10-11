// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPointer>

#include <obs-data.h>

class FieldSinks;
class Hotkeys;

// Field bindings live in the scene collection (spec §9): written by the frontend save callback,
// restored when a collection loads (the load call comes after the collection's sources exist).
// The hotkey bindings travel in the same callback, under their own sibling key.
class BindingStore {
public:
    BindingStore(FieldSinks* sinks, Hotkeys* hotkeys);
    ~BindingStore();
    BindingStore(const BindingStore&) = delete;
    BindingStore& operator=(const BindingStore&) = delete;

private:
    static void onSave(obs_data_t* data, bool saving, void* param);
    void save(obs_data_t* data) const;
    void load(obs_data_t* data);

    QPointer<FieldSinks> sinks_;
    QPointer<Hotkeys> hotkeys_;
};
