// SPDX-License-Identifier: GPL-2.0-or-later
#include "PluginState.hpp"

#include <set>

#include <nlohmann/json.hpp>

#include "MatchJson.hpp"
#include "PanelModel.hpp"

namespace sb {

namespace {

using nlohmann::json;

std::optional<json> parseObject(std::string_view text, int minVersion, int maxVersion,
                                std::string* error) {
    const auto fail = [error](const std::string& why) -> std::optional<json> {
        if (error) *error = why;
        return std::nullopt;
    };
    json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded()) return fail("not valid JSON");
    if (!j.is_object()) return fail("not a JSON object");
    const auto v = j.find("version");
    if (v == j.end() || !v->is_number_integer()) return fail("missing or non-integer version");
    const long long version = v->get<long long>();
    if (version < minVersion || version > maxVersion)
        return fail("unsupported version " + v->dump());
    return j;
}

// A valid "#RRGGBB" in canonical form, or empty.
std::string colorOf(const json& object, const char* key) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) return {};
    const auto rgb = parseColor(it->get<std::string>());
    return rgb ? formatColor(*rgb) : std::string();
}

} // namespace

std::string bindingsToJson(const std::vector<FieldBinding>& bindings) {
    json list = json::array();
    for (const FieldBinding& b : bindings)
        list.push_back(
            {{"field", std::string(fieldKey(b.field))}, {"uuid", b.uuid}, {"name", b.name}});
    return json{{"version", kBindingsJsonVersion}, {"bindings", list}}.dump(
        2, ' ', false, json::error_handler_t::replace);
}

std::optional<std::vector<FieldBinding>> bindingsFromJson(std::string_view text,
                                                          std::string* error) {
    const auto j = parseObject(text, kBindingsJsonVersion, kBindingsJsonVersion, error);
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
    json stored = json::object();
    for (const auto& entry : prefs.sportSettings)
        stored[entry.first] = json::parse(settingsToJson(entry.second));
    return json{
        {"version", kPrefsJsonVersion},         {"lastSport", prefs.lastSport},
        {"scoreColor", prefs.scoreColor},       {"sportChosen", prefs.sportChosen},
        {"foulColor", prefs.foulColor},         {"fileOutputEnabled", prefs.fileOutputEnabled},
        {"fileOutputDir", prefs.fileOutputDir}, {"updateCheck", prefs.updateCheck},
        {"sportSettings", stored}}
        .dump(2, ' ', false, json::error_handler_t::replace);
}

std::optional<GlobalPrefs> prefsFromJson(std::string_view text, std::string* error) {
    const auto j = parseObject(text, 1, kPrefsJsonVersion, error);
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
    prefs.scoreColor = colorOf(*j, "scoreColor");
    prefs.foulColor = colorOf(*j, "foulColor");
    const auto chosen = j->find("sportChosen");
    prefs.sportChosen = chosen != j->end() && chosen->is_boolean() && chosen->get<bool>();
    const auto enabled = j->find("fileOutputEnabled");
    prefs.fileOutputEnabled = enabled != j->end() && enabled->is_boolean() && enabled->get<bool>();
    const auto dir = j->find("fileOutputDir");
    if (dir != j->end() && dir->is_string()) prefs.fileOutputDir = dir->get<std::string>();
    const auto updates = j->find("updateCheck");
    if (updates != j->end() && updates->is_boolean()) prefs.updateCheck = updates->get<bool>();
    const auto stored = j->find("sportSettings");
    if (stored != j->end() && stored->is_object()) {
        for (auto it = stored->begin(); it != stored->end(); ++it) {
            auto settings = settingsFromJson(it.value().dump(), nullptr);
            if (!settings || settings->sport.id != it.key()) continue;
            prefs.sportSettings[it.key()] = std::move(*settings);
        }
    }
    return prefs;
}

MatchSettings settingsForSport(const GlobalPrefs& prefs, const SportPreset& sport) {
    const auto it = prefs.sportSettings.find(sport.id);
    if (it != prefs.sportSettings.end()) return it->second;
    return MatchSettings::forSport(sport);
}

void rememberSettings(GlobalPrefs& prefs, const MatchSettings& settings) {
    MatchSettings stored = settings;
    const MatchSettings defaults;
    stored.homeName = defaults.homeName;
    stored.awayName = defaults.awayName;
    prefs.sportSettings[settings.sport.id] = std::move(stored);
    prefs.lastSport = settings.sport.id;
    prefs.sportChosen = true;
}

} // namespace sb
