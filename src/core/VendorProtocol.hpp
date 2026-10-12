// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// The obs-websocket vendor wire protocol of the plugin ("obs-scoreboard"), in pure C++: parse one
// request into a core Command (or a stable error code) and build the JSON of every response and
// event. No OBS, no Qt, no websocket: the plugin only moves strings in and out of obs_data_t.

#include <string>
#include <string_view>

#include "Fields.hpp"
#include "MatchEngine.hpp"

namespace sb {

struct VendorRequest {
    enum class Kind { Command, NewMatch, GetState, GetFields, Invalid };
    Kind kind = Kind::Invalid;
    Command command;        // meaningful only when kind == Kind::Command
    std::string sport;      // meaningful only when kind == Kind::NewMatch
    bool confirmed = false; // meaningful only when kind == Kind::NewMatch
    std::string errorCode;  // meaningful only when kind == Kind::Invalid
};

// `requestJson` is the requestData object as JSON text ("{}" when the client sent none).
// Error codes: "unknownRequest", "badRequest", "badField", "unknownSport".
VendorRequest parseVendorRequest(std::string_view type, std::string_view requestJson);

// Every response is one JSON object with a "result" of "ok", "rejected", "confirmation" or
// "error". "rejected"/"confirmation" carry the engine's stable "reason"; "error" carries a
// stable "error" code.
std::string vendorResultJson(const CommandResult& result);
std::string vendorErrorJson(std::string_view errorCode);

std::string vendorStateJson(const MatchEngine& engine);
std::string vendorFieldsJson(const FieldValues& fields);

// Events. StateChanged carries only the fields whose text differs between `previous` and
// `current`, plus the two booleans clients cannot derive from a field.
std::string vendorStateChangedJson(const FieldValues& previous, const FieldValues& current,
                                   bool clockRunning, bool matchEnded);
// The StateChanged payload of a brand new match: every one of the 28 fields, so a client that
// connected mid-match drops any stale value ("" included). Used after NewMatch/replaceMatch.
std::string vendorFullStateJson(const FieldValues& fields, bool clockRunning, bool matchEnded);
std::string vendorPeriodEndedJson(const MatchEvent& event);
std::string vendorMatchEndedJson(const MatchEvent& event);
std::string vendorScoreChangedJson(Team team, int score, int homeScore, int awayScore);

} // namespace sb
