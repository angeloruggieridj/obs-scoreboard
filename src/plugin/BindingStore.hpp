// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPointer>

#include <obs-data.h>

class FieldSinks;

// Field bindings live in the scene collection (spec §9): written by the frontend save callback,
// restored when a collection loads (the load call comes after the collection's sources exist).
class BindingStore {
public:
    explicit BindingStore(FieldSinks* sinks);
    ~BindingStore();
    BindingStore(const BindingStore&) = delete;
    BindingStore& operator=(const BindingStore&) = delete;

private:
    static void onSave(obs_data_t* data, bool saving, void* param);
    void save(obs_data_t* data) const;
    void load(obs_data_t* data);

    QPointer<FieldSinks> sinks_;
};
