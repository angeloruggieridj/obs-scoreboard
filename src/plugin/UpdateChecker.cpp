// SPDX-License-Identifier: GPL-2.0-or-later
#include "UpdateChecker.hpp"

#include <curl/curl.h>
#include <obs-module.h>

#include <cstddef>
#include <mutex>

#include "UpdateCheck.hpp"
#include "plugin-support.h"

namespace {

constexpr const char* kReleasesUrl =
    "https://api.github.com/repos/angeloruggieridj/obs-scoreboard/releases?per_page=30";
constexpr long kConnectTimeoutSec = 10;
constexpr long kTimeoutSec = 20;
// A list of 30 releases is far below this; anything bigger is not what we asked for.
constexpr std::size_t kMaxBodyBytes = 2 * 1024 * 1024;

struct Transfer {
    std::string body;
    const std::atomic<bool>* abort = nullptr;
};

std::size_t appendBody(char* data, std::size_t size, std::size_t count, void* user) {
    auto* transfer = static_cast<Transfer*>(user);
    const std::size_t bytes = size * count;
    if (transfer->body.size() + bytes > kMaxBodyBytes) return 0; // makes curl fail the transfer
    transfer->body.append(data, bytes);
    return bytes;
}

// Returning non-zero aborts the transfer: how the destructor stops a request in flight.
int onProgress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    const auto* transfer = static_cast<const Transfer*>(user);
    return transfer->abort && transfer->abort->load() ? 1 : 0;
}

void ensureCurl() {
    // curl_easy_init() before curl_global_init() is undefined; OBS may or may not have done it.
    static std::once_flag once;
    std::call_once(once, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

} // namespace

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {}

UpdateChecker::~UpdateChecker() {
    abort_ = true;
    if (thread_.joinable()) thread_.join();
}

void UpdateChecker::start(const std::string& currentVersion, bool manual) {
    if (running_.exchange(true)) return;    // one check at a time
    if (thread_.joinable()) thread_.join(); // the previous one has finished
    thread_ = std::thread([this, currentVersion, manual]() {
        run(currentVersion, manual);
        running_ = false;
    });
}

void UpdateChecker::run(std::string currentVersion, bool manual) {
    const std::optional<sb::Version> current = sb::parseVersion(currentVersion);
    if (!current) {
        obs_log(LOG_INFO, "update check skipped: '%s' is not a release version",
                currentVersion.c_str());
        if (manual) emit failed();
        return;
    }

    ensureCurl();
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (manual) emit failed();
        return;
    }

    Transfer transfer;
    transfer.abort = &abort_;
    const std::string userAgent = std::string("obs-scoreboard/") + currentVersion;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");

    curl_easy_setopt(curl, CURLOPT_URL, kReleasesUrl);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, onProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &transfer);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (abort_) return;
    if (result != CURLE_OK || status != 200) {
        obs_log(LOG_INFO, "update check failed (curl %d, HTTP %ld)", static_cast<int>(result),
                status);
        if (manual) emit failed();
        return;
    }

    // A running pre-release is also offered newer pre-releases; a final build only finals.
    const std::optional<sb::ReleaseInfo> release =
        sb::newestRelease(transfer.body, *current, !current->pre.empty());
    if (!release) {
        obs_log(LOG_INFO, "update check: %s is the newest release", currentVersion.c_str());
        if (manual) emit noUpdate();
        return;
    }
    obs_log(LOG_INFO, "update check: %s is available", release->version.c_str());
    emit updateAvailable(QString::fromStdString(release->version),
                         QString::fromStdString(release->url));
}
