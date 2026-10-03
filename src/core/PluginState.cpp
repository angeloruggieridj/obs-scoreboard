// SPDX-License-Identifier: GPL-2.0-or-later
#include "PluginState.hpp"

#include <set>

#include <nlohmann/json.hpp>

namespace sb {

namespace {

using nlohmann::json;

std::optional<json> parseObject(std::string_view text, int version, std::string* error) {
    const auto fail = [error](const std::string& why) -> std::optional<json> {
        if (error) *error = why;
        return std::nullopt;
    };
    json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded()) return fail("not valid JSON");
    if (!j.is_object()) return fail("not a JSON object");
    const auto v = j.find("version");
    if (v == j.end() || !v->is_number_integer()) return fail("missing or non-integer version");
    if (v->get<long long>() != version) return fail("unsupported version " + v->dump());
    return j;
}

} // namespace

std::string bindingsToJson(const std::vector<FieldBinding>& bindings) {
    json list = json::array();
    for (const FieldBinding& b : bindings)
        list.push_back(
            {{"field", std::string(fieldKey(b.field))}, {"uuid", b.uuid}, {"name", b.name}});
    return json{{"version", kBindingsJsonVersion}, {"bindings", list}}.dump(2);
}

std::optional<std::vector<FieldBinding>> bindingsFromJson(std::string_view text,
                                                          std::string* error) {
    const auto j = parseObject(text, kBindingsJsonVersion, error);
    if (!j) return std::nullopt;
    const auto list = j->find("bindings");
    if (list == j->end() || !list->is_array()) {
        if (error) *error = "missing or non-array bindings";
        return std::nullopt;
    }
    std::vector<FieldBinding> out;
    std::set<FieldId> seen;
    for (const json& entry : *list) {
        if (!entry.is_object()) continue;
        const auto field = entry.find("field");
        const auto uuid = entry.find("uuid");
        const auto name = entry.find("name");
        if (field == entry.end() || !field->is_string()) continue;
        if (uuid == entry.end() || !uuid->is_string() || name == entry.end() || !name->is_string())
            continue;
        const auto id = fieldFromKey(field->get<std::string>());
        if (!id || seen.count(*id)) continue;
        FieldBinding b{*id, uuid->get<std::string>(), name->get<std::string>()};
        if (b.uuid.empty() && b.name.empty()) continue;
        seen.insert(*id);
        out.push_back(std::move(b));
    }
    return out;
}

std::string prefsToJson(const GlobalPrefs& prefs) {
    return json{{"version", kPrefsJsonVersion}, {"lastSport", prefs.lastSport}}.dump(2);
}

std::optional<GlobalPrefs> prefsFromJson(std::string_view text, std::string* error) {
    const auto j = parseObject(text, kPrefsJsonVersion, error);
    if (!j) return std::nullopt;
    GlobalPrefs prefs;
    const auto sport = j->find("lastSport");
    if (sport != j->end()) {
        if (!sport->is_string()) {
            if (error) *error = "lastSport is not a string";
            return std::nullopt;
        }
        if (!sport->get<std::string>().empty()) prefs.lastSport = sport->get<std::string>();
    }
    return prefs;
}

} // namespace sb
