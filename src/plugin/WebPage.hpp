// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>

// The HTML of the remote page. `i18nJson` is a JSON object of the texts the page shows, keyed by
// the plugin's locale keys; `canControl` decides whether the page offers buttons or only watches.
std::string webPageHtml(bool canControl, const std::string& i18nJson);
