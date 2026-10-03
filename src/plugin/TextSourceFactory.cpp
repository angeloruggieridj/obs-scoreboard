// SPDX-License-Identifier: GPL-2.0-or-later
#include "TextSourceFactory.hpp"

#include <obs-frontend-api.h>

namespace {

#ifdef _WIN32
constexpr const char* kTextId = "text_gdiplus";
#else
constexpr const char* kTextId = "text_ft2_source";
#endif

std::string freeName(const std::string& base) {
    for (int n = 1; n < 1000; ++n) {
        const std::string candidate = n == 1 ? base : base + " (" + std::to_string(n) + ")";
        OBSSourceAutoRelease existing = obs_get_source_by_name(candidate.c_str());
        if (!existing) return candidate;
    }
    return std::string();
}

} // namespace

OBSSourceAutoRelease createTextSource(const std::string& baseName, std::string* error) {
    OBSSourceAutoRelease sceneSource = obs_frontend_get_current_scene();
    obs_scene_t* scene = obs_scene_from_source(sceneSource);
    if (!scene) {
        if (error) *error = "no current scene";
        return nullptr;
    }
    const std::string name = freeName(baseName);
    if (name.empty()) {
        if (error) *error = "no free source name";
        return nullptr;
    }
    // docs/obs-api-notes.md section 1: versioned types are "<id>_v<N>"; ask for the current one.
    const char* type = obs_get_latest_input_type_id(kTextId);
    OBSSourceAutoRelease source =
        obs_source_create(type ? type : kTextId, name.c_str(), nullptr, nullptr);
    if (!source || !obs_scene_add(scene, source)) {
        if (error) *error = std::string("could not create a ") + kTextId + " source";
        return nullptr;
    }
    return source;
}
