// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "Fields.hpp"
#include "TextSink.hpp"

// One TextSink per field. Pushes every published FieldValues (each sink sends only what
// changed) and turns OBS's global source signals, raised on any thread, into sink updates on
// the UI thread.
class FieldSinks : public QObject {
    Q_OBJECT
public:
    // Called after a text was actually sent: the field, the text, os_gettime_ns() at sending.
    using PushObserver = std::function<void(sb::FieldId, const std::string&, std::uint64_t)>;

    explicit FieldSinks(QObject* parent = nullptr);
    ~FieldSinks() override;

    bool bind(sb::FieldId field, obs_source_t* source);
    bool bindTo(sb::FieldId field, const TextSink::Binding& binding);
    void unbind(sb::FieldId field);
    void unbindAll();
    const TextSink& sink(sb::FieldId field) const;

    void setPushObserver(PushObserver observer);

public slots:
    void publish(const sb::FieldValues& fields);

signals:
    void bindingChanged(sb::FieldId field);

private:
    static void onSourceGone(void* data, calldata_t* cd);
    static void onSourceRenamed(void* data, calldata_t* cd);
    void pushOne(sb::FieldId field);

    std::array<TextSink, sb::kFieldCount> sinks_;
    sb::FieldValues latest_;
    PushObserver observer_;
};
