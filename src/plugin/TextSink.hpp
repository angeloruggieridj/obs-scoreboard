// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <obs.hpp>

#include <optional>
#include <string>

// Writes one field's text straight into one OBS Text source (GDI+ or FreeType 2). UI thread
// only: FieldSinks marshals the source signals here.
class TextSink {
public:
    // What identifies the target across restarts: the uuid, with the name as a fallback.
    struct Binding {
        std::string uuid;
        std::string name;
    };
    enum class Problem { None, Removed, WrongType, NotFound };

    // Binds if `source` is a Text source; otherwise ends up unbound with the reason.
    bool bind(obs_source_t* source);
    // Looks the source up by uuid, then by name (adopting its uuid); type-checked like bind().
    bool bindTo(const Binding& binding);
    // Keeps binding() as the last target, so the dock can name what was lost.
    void unbind(Problem why = Problem::None);

    bool bound() const { return bound_; }
    const Binding& binding() const { return binding_; }
    Problem problem() const { return problem_; }

    // Sends `text` unless it is what this source last received. True when it sent.
    bool push(const std::string& text);

    void sourceGone(const std::string& uuid);
    void sourceRenamed(const std::string& uuid, const std::string& newName);

    static bool isTextSource(obs_source_t* source);

private:
    OBSWeakSourceAutoRelease weak_;
    Binding binding_;
    std::optional<std::string> lastSent_;
    bool bound_ = false;
    Problem problem_ = Problem::None;
};
