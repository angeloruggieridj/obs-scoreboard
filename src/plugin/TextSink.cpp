// SPDX-License-Identifier: GPL-2.0-or-later
#include "TextSink.hpp"

#include <cstring>

namespace {

// docs/obs-api-notes.md §1–§2: unversioned ids, and each type's "read from file" key.
constexpr const char* kGdiPlusId = "text_gdiplus";
constexpr const char* kFreeType2Id = "text_ft2_source";

const char* fileKeyFor(obs_source_t* source) {
    const char* id = obs_source_get_unversioned_id(source);
    if (!id) return nullptr;
    if (std::strcmp(id, kGdiPlusId) == 0) return "read_from_file";
    if (std::strcmp(id, kFreeType2Id) == 0) return "from_file";
    return nullptr;
}

std::string orEmpty(const char* s) {
    return s ? std::string(s) : std::string();
}

} // namespace

bool TextSink::isTextSource(obs_source_t* source) {
    return source && fileKeyFor(source) != nullptr;
}

bool TextSink::bind(obs_source_t* source) {
    if (!source) {
        unbind(Problem::NotFound);
        return false;
    }
    binding_ = {orEmpty(obs_source_get_uuid(source)), orEmpty(obs_source_get_name(source))};
    if (!isTextSource(source)) {
        unbind(Problem::WrongType);
        return false;
    }
    weak_ = obs_source_get_weak_source(source);
    lastSent_.reset();
    bound_ = true;
    problem_ = Problem::None;
    return true;
}

bool TextSink::bindTo(const Binding& binding) {
    OBSSourceAutoRelease source;
    if (!binding.uuid.empty()) source = obs_get_source_by_uuid(binding.uuid.c_str());
    if (!source && !binding.name.empty()) source = obs_get_source_by_name(binding.name.c_str());
    if (!source) {
        binding_ = binding;
        unbind(Problem::NotFound);
        return false;
    }
    return bind(source);
}

void TextSink::unbind(Problem why) {
    weak_ = nullptr;
    lastSent_.reset();
    bound_ = false;
    problem_ = why;
}

bool TextSink::push(const std::string& text) {
    if (!bound_ || (lastSent_ && *lastSent_ == text)) return false;
    OBSSourceAutoRelease source = obs_weak_source_get_source(weak_);
    if (!source) return false; // being destroyed: sourceGone() unbinds when its signal lands
    const char* fileKey = fileKeyFor(source);
    if (!fileKey) {
        unbind(Problem::WrongType);
        return false;
    }
    OBSDataAutoRelease settings = obs_data_create();
    obs_data_set_string(settings, "text", text.c_str());
    obs_data_set_bool(settings, fileKey, false);
    obs_source_update(source, settings);
    lastSent_ = text;
    return true;
}

void TextSink::sourceGone(const std::string& uuid) {
    if (bound_ && uuid == binding_.uuid) unbind(Problem::Removed);
}

void TextSink::sourceRenamed(const std::string& uuid, const std::string& newName) {
    if (uuid == binding_.uuid) binding_.name = newName;
}
