// SPDX-License-Identifier: GPL-2.0-or-later
#include "WebRemote.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <system_error>

#include <nlohmann/json.hpp>

#include "PanelLayout.hpp"
#include "VendorProtocol.hpp"

namespace sb {

namespace {

// Every request of the vendor protocol that changes the match (docs/remote-control.md).
constexpr std::array<std::string_view, 18> kCommands = {"ClockToggle",   "ClockStart",
                                                        "ClockStop",     "ClockReset",
                                                        "ClockAdjust",   "ClockSet",
                                                        "PeriodNext",    "PeriodPrev",
                                                        "AddStat",       "SetTeamName",
                                                        "PenaltyAdd",    "PenaltyEdit",
                                                        "PenaltyCancel", "PenaltyCancelActive",
                                                        "StoppageSet",   "StoppageAdd",
                                                        "EndMatch",      "NewMatch"};

bool isCommand(std::string_view name) {
    return std::find(kCommands.begin(), kCommands.end(), name) != kCommands.end();
}

std::string lower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string percentDecode(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+') {
            out += ' ';
        } else if (text[i] == '%' && i + 2 < text.size() && hexValue(text[i + 1]) >= 0 &&
                   hexValue(text[i + 2]) >= 0) {
            out += static_cast<char>(hexValue(text[i + 1]) * 16 + hexValue(text[i + 2]));
            i += 2;
        } else {
            out += text[i];
        }
    }
    return out;
}

const char* reason(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 408:
        return "Request Timeout";
    case 413:
        return "Payload Too Large";
    case 503:
        return "Service Unavailable";
    default:
        return "Error";
    }
}

} // namespace

std::string keyFromBytes(const std::array<unsigned char, 12>& bytes) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (unsigned char b : bytes) {
        out += digits[b >> 4];
        out += digits[b & 15];
    }
    return out;
}

bool constantTimeEqual(std::string_view a, std::string_view b) {
    unsigned char diff = a.size() == b.size() ? 0 : 1;
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

WebAccess authorizeKey(RemoteMode mode, std::string_view viewKey, std::string_view controlKey,
                       std::string_view provided) {
    if (mode == RemoteMode::Off || provided.empty()) return WebAccess::None;
    // Both keys are always compared, so the time taken does not tell which one was close.
    const bool control = !controlKey.empty() && constantTimeEqual(controlKey, provided);
    const bool view = !viewKey.empty() && constantTimeEqual(viewKey, provided);
    if (control && mode == RemoteMode::Control) return WebAccess::Control;
    if (view) return WebAccess::View;
    return WebAccess::None;
}

ParseStatus parseHttpRequest(std::string_view data, HttpRequest& out) {
    const std::size_t headEnd = data.find("\r\n\r\n");
    if (headEnd == std::string_view::npos)
        return data.size() > kMaxRequestHeadBytes ? ParseStatus::TooLarge : ParseStatus::Incomplete;
    if (headEnd > kMaxRequestHeadBytes) return ParseStatus::TooLarge;

    const std::string_view head = data.substr(0, headEnd);
    const std::size_t lineEnd = head.find("\r\n");
    const std::string_view requestLine = head.substr(0, lineEnd);
    const std::size_t sp1 = requestLine.find(' ');
    const std::size_t sp2 = requestLine.rfind(' ');
    if (sp1 == std::string_view::npos || sp2 == sp1) return ParseStatus::Bad;
    HttpRequest request;
    request.method = std::string(requestLine.substr(0, sp1));
    const std::string_view target = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);
    const std::string_view version = requestLine.substr(sp2 + 1);
    if (version.substr(0, 7) != "HTTP/1." || target.empty() || target.front() != '/')
        return ParseStatus::Bad;
    if (request.method.empty() ||
        !std::all_of(request.method.begin(), request.method.end(),
                     [](unsigned char c) { return std::isupper(c) != 0; }))
        return ParseStatus::Bad;
    const std::size_t q = target.find('?');
    request.path = std::string(target.substr(0, q));
    if (q != std::string_view::npos) request.query = std::string(target.substr(q + 1));

    if (lineEnd != std::string_view::npos) {
        std::string_view rest = head.substr(lineEnd + 2);
        while (!rest.empty()) {
            const std::size_t end = rest.find("\r\n");
            const std::string_view line = rest.substr(0, end);
            rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 2);
            const std::size_t colon = line.find(':');
            if (colon == std::string_view::npos || colon == 0) return ParseStatus::Bad;
            request.headers[lower(trim(line.substr(0, colon)))] =
                std::string(trim(line.substr(colon + 1)));
        }
    }
    if (request.headers.count("transfer-encoding")) return ParseStatus::Bad;

    std::size_t length = 0;
    const auto it = request.headers.find("content-length");
    if (it != request.headers.end()) {
        const std::string& value = it->second;
        const auto [end, error] =
            std::from_chars(value.data(), value.data() + value.size(), length);
        if (error != std::errc() || end != value.data() + value.size()) return ParseStatus::Bad;
        if (length > kMaxRequestBodyBytes) return ParseStatus::TooLarge;
    }
    const std::size_t bodyStart = headEnd + 4;
    if (data.size() < bodyStart + length) return ParseStatus::Incomplete;
    request.body = std::string(data.substr(bodyStart, length));
    out = std::move(request);
    return ParseStatus::Ready;
}

std::optional<std::string> queryParam(std::string_view query, std::string_view name) {
    while (!query.empty()) {
        const std::size_t amp = query.find('&');
        const std::string_view pair = query.substr(0, amp);
        query = amp == std::string_view::npos ? std::string_view() : query.substr(amp + 1);
        const std::size_t eq = pair.find('=');
        const std::string_view key = pair.substr(0, eq);
        if (percentDecode(key) != name) continue;
        return eq == std::string_view::npos ? std::string() : percentDecode(pair.substr(eq + 1));
    }
    return std::nullopt;
}

WebRoute routeWebRequest(const HttpRequest& request, WebAccess access) {
    using Kind = WebRoute::Kind;
    WebRoute route;
    if (request.path == "/favicon.ico") {
        route.kind = Kind::Favicon;
        return route;
    }
    if (access == WebAccess::None) {
        route.kind = Kind::Unauthorized;
        return route;
    }
    const bool get = request.method == "GET";
    const bool post = request.method == "POST";
    if (request.path == "/" || request.path == "/api/state") {
        if (!get) {
            route.kind = Kind::MethodNotAllowed;
            return route;
        }
        route.kind = request.path == "/" ? Kind::Page : Kind::State;
        route.canControl = access == WebAccess::Control;
        return route;
    }
    constexpr std::string_view prefix = "/api/";
    if (request.path.compare(0, prefix.size(), prefix) == 0) {
        const std::string name = request.path.substr(prefix.size());
        if (!isCommand(name)) {
            route.kind = Kind::NotFound;
            return route;
        }
        if (!post) {
            route.kind = Kind::MethodNotAllowed;
            return route;
        }
        if (access != WebAccess::Control) {
            route.kind = Kind::Forbidden;
            return route;
        }
        route.kind = Kind::Command;
        route.requestType = name;
        route.canControl = true;
        return route;
    }
    route.kind = Kind::NotFound;
    return route;
}

std::string webStateJson(const MatchEngine& engine, bool canControl) {
    using json = nlohmann::json;
    json doc = json::parse(vendorStateJson(engine));
    json& state = doc["state"];
    const PanelLayout layout = panelLayout(engine.settings());
    state["layout"] = {{"shots", layout.shots},
                       {"fouls", layout.fouls},
                       {"fouls2", layout.fouls2},
                       {"penalties", layout.penalties},
                       {"strength", layout.strength},
                       {"stoppage", layout.stoppage},
                       {"scoreValues", layout.scoreValues},
                       {"foulsLabel", layout.foulsLabel},
                       {"fouls2Label", layout.fouls2Label},
                       {"scoreLabel", layout.scoreLabel},
                       {"segmentLabel", layout.segmentLabel}};
    state["control"] = canControl;
    return doc.dump(-1, ' ', false, json::error_handler_t::replace);
}

std::string httpResponse(int status, std::string_view contentType, std::string_view body) {
    std::string out = "HTTP/1.1 " + std::to_string(status) + " " + reason(status) + "\r\n";
    if (status != 204) {
        out += "Content-Type: " + std::string(contentType) + "\r\n";
        out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    out += "Cache-Control: no-store\r\n";
    out += "X-Content-Type-Options: nosniff\r\n";
    out += "Referrer-Policy: no-referrer\r\n";
    // The page is one inline script and style; it talks only to the server it came from.
    out += "Content-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; "
           "style-src 'unsafe-inline'; connect-src 'self'\r\n";
    out += "Connection: close\r\n\r\n";
    if (status != 204) out += std::string(body);
    return out;
}

} // namespace sb
