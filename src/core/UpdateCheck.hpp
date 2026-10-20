// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// The pure half of the update check: comparing plugin versions and picking the newest release out
// of what GitHub's "list releases" API answers. No network, no OBS, no Qt: the plugin fetches the
// text and hands it here.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sb {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    // Pre-release suffix without the dash ("beta4", "rc1"); empty for a final release.
    std::string pre;
};

// Parses "1.2.3", "v1.2.3" and "1.2.3-beta4". nullopt for anything else.
std::optional<Version> parseVersion(std::string_view text);

// <0, 0, >0. A final release is newer than any pre-release of the same numbers; pre-releases
// compare by their letters ("beta" < "rc") and then by their trailing number (beta2 < beta10).
int compareVersions(const Version& a, const Version& b);

struct ReleaseInfo {
    std::string version; // the tag, e.g. "1.0.1"
    std::string name;
    std::string url; // the release page (html_url)
    bool prerelease = false;
    std::string body; // the release notes (Markdown), as written on GitHub
};

// Picks the newest release of `releasesJson` (the array of GET /repos/{owner}/{repo}/releases)
// that is newer than `current`. Drafts and unparsable tags are ignored; pre-releases are
// considered only when `includePrereleases` is true. nullopt when nothing newer exists or the
// document is not a releases array.
std::optional<ReleaseInfo> newestRelease(std::string_view releasesJson, const Version& current,
                                         bool includePrereleases);

// Every release newer than `current` (same filters as newestRelease), newest first, each with its
// notes. Empty when nothing is newer or the document is not a releases array.
std::vector<ReleaseInfo> newerReleases(std::string_view releasesJson, const Version& current,
                                       bool includePrereleases);

} // namespace sb
