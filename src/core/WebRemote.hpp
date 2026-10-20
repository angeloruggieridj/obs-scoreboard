// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// The pure half of the remote web page: reading one HTTP request, deciding who may do what, and
// writing the answer. No sockets, no Qt, no OBS: the plugin's server only moves bytes in and out.
// Nothing here reaches beyond a request line, a few headers and a small JSON body.

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "MatchEngine.hpp"

namespace sb {

// Who may open the remote page: nobody (the default), anyone with the view link, or anyone with
// the view link plus those with the control link.
enum class RemoteMode { Off, View, Control };

enum class WebAccess { None, View, Control };

constexpr int kDefaultRemotePort = 8765;
constexpr std::size_t kMaxRequestHeadBytes = 16 * 1024;
constexpr std::size_t kMaxRequestBodyBytes = 16 * 1024;

// A random key as 24 hex digits, from 12 bytes the caller drew from a secure source.
std::string keyFromBytes(const std::array<unsigned char, 12>& bytes);

// Compares without stopping at the first difference.
bool constantTimeEqual(std::string_view a, std::string_view b);

// The access a `provided` key gives. An empty key (configured or provided) never matches; the
// control key only counts in Control mode.
WebAccess authorizeKey(RemoteMode mode, std::string_view viewKey, std::string_view controlKey,
                       std::string_view provided);

struct HttpRequest {
    std::string method;
    std::string path;                           // without the query
    std::string query;                          // after the '?', undecoded
    std::map<std::string, std::string> headers; // names in lower case
    std::string body;
};

enum class ParseStatus { Incomplete, Ready, Bad, TooLarge };

// Reads one request from `data` (everything received so far). Ready fills `out`; Incomplete means
// wait for more bytes. Chunked bodies and anything that is not HTTP/1.x are Bad.
ParseStatus parseHttpRequest(std::string_view data, HttpRequest& out);

// The value of `name` in a query string ("a=1&k=xyz"), percent-decoded; nullopt when absent.
std::optional<std::string> queryParam(std::string_view query, std::string_view name);

struct WebRoute {
    enum class Kind {
        Page,         // GET /
        State,        // GET /api/state
        Command,      // POST /api/<requestType>
        Favicon,      // GET /favicon.ico (answered empty)
        Unauthorized, // no valid key
        Forbidden,    // a view key asked for a command
        NotFound,
        MethodNotAllowed,
    };
    Kind kind = Kind::NotFound;
    std::string requestType; // for Command: a request of the obs-scoreboard vendor protocol
    bool canControl = false; // for Page and State
};

WebRoute routeWebRequest(const HttpRequest& request, WebAccess access);

// What the web page draws: the vendor state of the match plus what the dock shows for the sport
// (which counters exist, the score buttons) and whether this viewer may command. JSON text.
std::string webStateJson(const MatchEngine& engine, bool canControl);

// A complete HTTP/1.1 answer that closes the connection.
std::string httpResponse(int status, std::string_view contentType, std::string_view body);

} // namespace sb
