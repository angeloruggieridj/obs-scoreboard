// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <obs.hpp>

#include <string>

// Creates a Text source of the platform's type in the current scene, under a free name derived
// from baseName ("X", then "X (2)", "X (3)", ...). Never renames or replaces an existing source.
OBSSourceAutoRelease createTextSource(const std::string& baseName, std::string* error);
