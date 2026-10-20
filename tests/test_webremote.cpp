// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <string>

#include <nlohmann/json.hpp>

#include "PluginState.hpp"
#include "SportCatalog.hpp"
#include "WebRemote.hpp"
#include "WebState.hpp"

using namespace sb;

namespace {

HttpRequest parsed(const std::string& text) {
    HttpRequest request;
    REQUIRE(parseHttpRequest(text, request) == ParseStatus::Ready);
    return request;
}

} // namespace

TEST_CASE("keys are 24 hex digits and compare exactly") {
    std::array<unsigned char, 12> bytes{};
    bytes[0] = 0xAB;
    bytes[11] = 0x05;
    const std::string key = keyFromBytes(bytes);
    CHECK(key == "ab0000000000000000000005");
    CHECK(constantTimeEqual(key, key));
    CHECK_FALSE(constantTimeEqual(key, "ab0000000000000000000006"));
    CHECK_FALSE(constantTimeEqual(key, "ab00"));
    CHECK(constantTimeEqual("", ""));
}

TEST_CASE("a key gives the access of the link it belongs to") {
    CHECK(authorizeKey(RemoteMode::Off, "v", "c", "v") == WebAccess::None);
    CHECK(authorizeKey(RemoteMode::View, "v", "c", "v") == WebAccess::View);
    CHECK(authorizeKey(RemoteMode::View, "v", "c", "c") == WebAccess::None); // control link is off
    CHECK(authorizeKey(RemoteMode::Control, "v", "c", "v") == WebAccess::View);
    CHECK(authorizeKey(RemoteMode::Control, "v", "c", "c") == WebAccess::Control);
    CHECK(authorizeKey(RemoteMode::Control, "v", "c", "x") == WebAccess::None);
    CHECK(authorizeKey(RemoteMode::Control, "v", "c", "") == WebAccess::None);
    CHECK(authorizeKey(RemoteMode::Control, "", "", "") == WebAccess::None);
}

TEST_CASE("a request is read once it is complete") {
    HttpRequest request;
    CHECK(parseHttpRequest("GET / HTTP/1.1\r\nHost: x", request) == ParseStatus::Incomplete);
    request = parsed("GET /api/state?k=abc&x=1 HTTP/1.1\r\nHost: x\r\nUser-Agent:  t \r\n\r\n");
    CHECK(request.method == "GET");
    CHECK(request.path == "/api/state");
    CHECK(request.query == "k=abc&x=1");
    CHECK(request.headers.at("user-agent") == "t");
    CHECK(request.body.empty());
}

TEST_CASE("a body is waited for and cut at its length") {
    HttpRequest request;
    const std::string head = "POST /api/ClockToggle HTTP/1.1\r\nContent-Length: 4\r\n\r\n";
    CHECK(parseHttpRequest(head + "{}", request) == ParseStatus::Incomplete);
    request = parsed(head + "{}{}EXTRA");
    CHECK(request.body == "{}{}");
}

TEST_CASE("malformed and oversized requests are refused") {
    HttpRequest request;
    CHECK(parseHttpRequest("GARBAGE\r\n\r\n", request) == ParseStatus::Bad);
    CHECK(parseHttpRequest("GET / FTP/1.1\r\n\r\n", request) == ParseStatus::Bad);
    CHECK(parseHttpRequest("GET x HTTP/1.1\r\n\r\n", request) == ParseStatus::Bad);
    CHECK(parseHttpRequest("get / HTTP/1.1\r\n\r\n", request) == ParseStatus::Bad);
    CHECK(parseHttpRequest("GET / HTTP/1.1\r\nbroken\r\n\r\n", request) == ParseStatus::Bad);
    CHECK(parseHttpRequest("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n", request) ==
          ParseStatus::Bad);
    CHECK(parseHttpRequest("POST / HTTP/1.1\r\nContent-Length: x\r\n\r\n", request) ==
          ParseStatus::Bad);
    CHECK(parseHttpRequest("POST / HTTP/1.1\r\nContent-Length: 99999\r\n\r\n", request) ==
          ParseStatus::TooLarge);
    CHECK(parseHttpRequest(std::string(kMaxRequestHeadBytes + 10, 'a'), request) ==
          ParseStatus::TooLarge);
}

TEST_CASE("query values are decoded and found by name") {
    CHECK(queryParam("k=abc", "k") == "abc");
    CHECK(queryParam("a=1&k=x%2Fy+z", "k") == "x/y z");
    CHECK(queryParam("a=1&k", "k") == "");
    CHECK_FALSE(queryParam("a=1", "k").has_value());
    CHECK_FALSE(queryParam("", "k").has_value());
    CHECK(queryParam("k=%zz", "k") == "%zz");
    CHECK(queryParam("k=%4", "k") == "%4");
}

TEST_CASE("routes follow the access of the key") {
    using Kind = WebRoute::Kind;
    const auto route = [](const char* method, const char* path, WebAccess access) {
        HttpRequest request;
        request.method = method;
        request.path = path;
        return routeWebRequest(request, access);
    };
    CHECK(route("GET", "/", WebAccess::None).kind == Kind::Unauthorized);
    CHECK(route("GET", "/api/state", WebAccess::None).kind == Kind::Unauthorized);
    CHECK(route("POST", "/api/ClockToggle", WebAccess::None).kind == Kind::Unauthorized);
    CHECK(route("GET", "/favicon.ico", WebAccess::None).kind == Kind::Favicon);

    const WebRoute page = route("GET", "/", WebAccess::View);
    CHECK(page.kind == Kind::Page);
    CHECK_FALSE(page.canControl);
    CHECK(route("GET", "/", WebAccess::Control).canControl);
    CHECK(route("GET", "/api/state", WebAccess::View).kind == Kind::State);
    CHECK(route("POST", "/", WebAccess::View).kind == Kind::MethodNotAllowed);

    CHECK(route("POST", "/api/ClockToggle", WebAccess::View).kind == Kind::Forbidden);
    const WebRoute command = route("POST", "/api/AddStat", WebAccess::Control);
    CHECK(command.kind == Kind::Command);
    CHECK(command.requestType == "AddStat");
    CHECK(route("GET", "/api/AddStat", WebAccess::Control).kind == Kind::MethodNotAllowed);
    CHECK(route("POST", "/api/Nope", WebAccess::Control).kind == Kind::NotFound);
    CHECK(route("POST", "/api/GetState", WebAccess::Control).kind == Kind::NotFound);
    CHECK(route("GET", "/other", WebAccess::Control).kind == Kind::NotFound);
}

TEST_CASE("every change of the match can be asked for, and nothing else") {
    for (const char* name : {"ClockToggle", "ClockStart", "ClockStop", "ClockReset", "ClockAdjust",
                             "ClockSet", "PeriodNext", "PeriodPrev", "AddStat", "SetTeamName",
                             "PenaltyAdd", "PenaltyEdit", "PenaltyCancel", "PenaltyCancelActive",
                             "StoppageSet", "StoppageAdd", "EndMatch", "NewMatch"}) {
        HttpRequest request;
        request.method = "POST";
        request.path = std::string("/api/") + name;
        CHECK(routeWebRequest(request, WebAccess::Control).kind == WebRoute::Kind::Command);
    }
}

TEST_CASE("an answer carries its length and the safety headers") {
    const std::string answer = httpResponse(200, "text/plain", "hello");
    CHECK(answer.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
    CHECK(answer.find("Content-Length: 5\r\n") != std::string::npos);
    CHECK(answer.find("Connection: close\r\n") != std::string::npos);
    CHECK(answer.find("Cache-Control: no-store") != std::string::npos);
    CHECK(answer.find("Referrer-Policy: no-referrer") != std::string::npos);
    CHECK(answer.substr(answer.size() - 5) == "hello");
    const std::string empty = httpResponse(204, "", "");
    CHECK(empty.find("Content-Length") == std::string::npos);
    CHECK(httpResponse(401, "text/plain", "x").rfind("HTTP/1.1 401 Unauthorized", 0) == 0);
}

TEST_CASE("the remote settings survive saving and a damaged file falls back to off") {
    GlobalPrefs prefs;
    CHECK(prefs.remoteMode == RemoteMode::Off);
    prefs.remoteMode = RemoteMode::Control;
    prefs.remotePort = 9000;
    prefs.remoteViewKey = "view";
    prefs.remoteControlKey = "control";
    const auto back = prefsFromJson(prefsToJson(prefs));
    REQUIRE(back.has_value());
    CHECK(back->remoteMode == RemoteMode::Control);
    CHECK(back->remotePort == 9000);
    CHECK(back->remoteViewKey == "view");
    CHECK(back->remoteControlKey == "control");

    prefs.remoteMode = RemoteMode::View;
    CHECK(prefsFromJson(prefsToJson(prefs))->remoteMode == RemoteMode::View);

    const auto odd =
        prefsFromJson("{\"version\":3,\"remoteMode\":7,\"remotePort\":80,\"remoteViewKey\":1}");
    REQUIRE(odd.has_value());
    CHECK(odd->remoteMode == RemoteMode::Off);
    CHECK(odd->remotePort == kDefaultRemotePort); // below 1024: ignored
    CHECK(odd->remoteViewKey.empty());
}

TEST_CASE("typed keys are limited to what fits in a link") {
    CHECK(validKey("match-2026_A"));
    CHECK(validKey("abcdef"));
    CHECK_FALSE(validKey("short"));
    CHECK_FALSE(validKey("has space"));
    CHECK_FALSE(validKey("slash/inside"));
    CHECK_FALSE(validKey("a?b=c&d"));
    CHECK_FALSE(validKey(std::string(65, 'a')));
    CHECK(validKey(std::string(64, 'a')));
}

TEST_CASE("the font route is answered for any method rule of the page") {
    HttpRequest request;
    request.method = "GET";
    request.path = "/font";
    CHECK(routeWebRequest(request, WebAccess::View).kind == WebRoute::Kind::Font);
    request.method = "POST";
    CHECK(routeWebRequest(request, WebAccess::View).kind == WebRoute::Kind::MethodNotAllowed);
}

TEST_CASE("the web state carries the layout, the look and the right to command") {
    const SportPreset* hockey = findSport("ice_hockey");
    REQUIRE(hockey != nullptr);
    const MatchEngine engine(MatchSettings::forSport(*hockey));
    GlobalPrefs prefs;
    prefs.homeColor = "#2E9E6B";
    prefs.clockColor.mode = ClockColorMode::Single;
    prefs.clockColor.base = "#FF0000";
    const auto view = nlohmann::json::parse(webStateJson(engine, false, prefs));
    CHECK(view["result"] == "ok");
    CHECK(view["state"]["control"] == false);
    CHECK(view["state"]["layout"]["shots"] == true);
    CHECK(view["state"]["layout"]["penalties"] == true);
    CHECK(view["state"]["layout"]["fouls"] == false);
    CHECK(view["state"]["layout"]["segmentLabel"] == "Period");
    CHECK(view["state"]["sport"] == "ice_hockey");
    CHECK(view["state"]["look"]["periodBar"] == "remaining");
    CHECK(view["state"]["look"]["homeColor"] == "#2E9E6B");
    CHECK(view["state"]["look"]["awayColor"] == "");
    CHECK(view["state"]["look"]["scoreColor"] == "#F2C94C"); // the default is spelled out
    CHECK(view["state"]["look"]["clockColor"] == "#FF0000");
    CHECK(view["state"]["look"]["share"].is_number());
    CHECK(view["state"]["clockModel"]["down"] == true);
    CHECK(view["state"]["clockModel"]["running"] == false);
    CHECK(view["state"]["clockModel"]["duration"].is_number());
    CHECK(view["state"]["clockModel"]["twoDigit"].is_boolean());
    CHECK(nlohmann::json::parse(webStateJson(engine, true, prefs))["state"]["control"] == true);
}
