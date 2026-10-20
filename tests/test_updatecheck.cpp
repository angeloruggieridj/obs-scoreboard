// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <string>

#include "UpdateCheck.hpp"

using namespace sb;

namespace {

Version v(const char* text) {
    const auto parsed = parseVersion(text);
    REQUIRE(parsed.has_value());
    return *parsed;
}

} // namespace

TEST_CASE("versions parse with and without a v prefix and a pre-release suffix") {
    const Version plain = v("1.2.3");
    CHECK(plain.major == 1);
    CHECK(plain.minor == 2);
    CHECK(plain.patch == 3);
    CHECK(plain.pre.empty());
    CHECK(v("v10.0.1").major == 10);
    CHECK(v("1.0.0-beta4").pre == "beta4");
}

TEST_CASE("anything that is not major.minor.patch does not parse") {
    CHECK_FALSE(parseVersion("").has_value());
    CHECK_FALSE(parseVersion("1.2").has_value());
    CHECK_FALSE(parseVersion("1.2.x").has_value());
    CHECK_FALSE(parseVersion("1.2.3-").has_value());
    CHECK_FALSE(parseVersion("latest").has_value());
    CHECK_FALSE(parseVersion("1..3").has_value());
}

TEST_CASE("versions compare by numbers first") {
    CHECK(compareVersions(v("1.0.0"), v("1.0.0")) == 0);
    CHECK(compareVersions(v("1.0.1"), v("1.0.0")) > 0);
    CHECK(compareVersions(v("1.2.0"), v("1.10.0")) < 0);
    CHECK(compareVersions(v("2.0.0"), v("1.99.99")) > 0);
}

TEST_CASE(
    "a final release outranks its pre-releases and pre-releases order by letters and number") {
    CHECK(compareVersions(v("1.0.0"), v("1.0.0-rc1")) > 0);
    CHECK(compareVersions(v("1.0.0-beta2"), v("1.0.0")) < 0);
    CHECK(compareVersions(v("1.0.0-beta2"), v("1.0.0-beta10")) < 0);
    CHECK(compareVersions(v("1.0.0-beta9"), v("1.0.0-rc1")) < 0);
    CHECK(compareVersions(v("1.0.0-rc1"), v("1.0.0-rc1")) == 0);
    CHECK(compareVersions(v("1.0.1-beta1"), v("1.0.0")) > 0);
}

namespace {

const char* kReleases = R"([
  {"tag_name":"1.1.0-beta1","name":"Beta","html_url":"https://example.org/r/1.1.0-beta1",
   "prerelease":true,"draft":false},
  {"tag_name":"1.0.1","name":"ScoreBoard 1.0.1","html_url":"https://example.org/r/1.0.1",
   "prerelease":false,"draft":false},
  {"tag_name":"2.0.0","name":"Draft","html_url":"https://example.org/r/2.0.0",
   "prerelease":false,"draft":true},
  {"tag_name":"1.0.0","name":"ScoreBoard 1.0.0","html_url":"https://example.org/r/1.0.0",
   "prerelease":false,"draft":false},
  {"tag_name":"nightly","html_url":"x","prerelease":false,"draft":false},
  42
])";

} // namespace

TEST_CASE("the newest stable release newer than the running build is offered") {
    const auto found = newestRelease(kReleases, v("1.0.0"), false);
    REQUIRE(found.has_value());
    CHECK(found->version == "1.0.1");
    CHECK(found->name == "ScoreBoard 1.0.1");
    CHECK(found->url == "https://example.org/r/1.0.1");
    CHECK_FALSE(found->prerelease);
}

TEST_CASE("drafts and unparsable tags are never offered") {
    const auto found = newestRelease(kReleases, v("1.0.1"), false);
    CHECK_FALSE(found.has_value()); // 2.0.0 is a draft, 1.1.0-beta1 a pre-release
}

TEST_CASE("pre-releases are offered only when asked for") {
    const auto found = newestRelease(kReleases, v("1.0.1"), true);
    REQUIRE(found.has_value());
    CHECK(found->version == "1.1.0-beta1");
    CHECK(found->prerelease);
}

TEST_CASE("nothing newer, or a document that is not a releases array, offers nothing") {
    CHECK_FALSE(newestRelease(kReleases, v("1.0.1"), false).has_value());
    CHECK_FALSE(newestRelease("{\"message\":\"Not Found\"}", v("1.0.0"), true).has_value());
    CHECK_FALSE(newestRelease("not json", v("1.0.0"), true).has_value());
    CHECK_FALSE(newestRelease("[]", v("1.0.0"), true).has_value());
}

TEST_CASE("a running pre-release is offered the final release of its own numbers") {
    const auto found = newestRelease(kReleases, v("1.0.1-beta3"), false);
    REQUIRE(found.has_value());
    CHECK(found->version == "1.0.1");
}

TEST_CASE("every newer release comes with its notes, newest first") {
    const std::string releases = R"([
      {"tag_name":"1.0.1","html_url":"u1","body":"Fixes","prerelease":false,"draft":false},
      {"tag_name":"1.2.0","html_url":"u3","body":"Big one","prerelease":false,"draft":false},
      {"tag_name":"1.1.0","html_url":"u2","prerelease":false,"draft":false},
      {"tag_name":"1.0.0","html_url":"u0","body":"Old","prerelease":false,"draft":false},
      {"tag_name":"1.3.0","html_url":"u4","body":"Draft","prerelease":false,"draft":true}
    ])";
    const auto list = newerReleases(releases, v("1.0.0"), false);
    REQUIRE(list.size() == 3);
    CHECK(list[0].version == "1.2.0");
    CHECK(list[0].body == "Big one");
    CHECK(list[1].version == "1.1.0");
    CHECK(list[1].body.empty()); // no notes written
    CHECK(list[2].version == "1.0.1");
    CHECK(list[2].body == "Fixes");
    CHECK(newerReleases("not json", v("1.0.0"), true).empty());
    CHECK(newerReleases(releases, v("1.2.0"), false).empty());
}
