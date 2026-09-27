# SPDX-License-Identifier: GPL-2.0-or-later
add_library(
  scoreboard-core
  STATIC
  Types.cpp
  Clock.cpp
  Format.cpp
  Periods.cpp
  Counters.cpp
  PenaltyBox.cpp
  Strength.cpp
)
target_include_directories(scoreboard-core PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(
  scoreboard-core
  SYSTEM
  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../third_party"
)
target_compile_features(scoreboard-core PUBLIC cxx_std_17)
set_target_properties(
  scoreboard-core
  PROPERTIES
    CXX_EXTENSIONS OFF
    # Linked into the plugin's shared module: position-independent on Linux.
    POSITION_INDEPENDENT_CODE ON
)

option(SB_WARNINGS_AS_ERRORS "Treat warnings in the core as errors" OFF)
option(SB_COVERAGE "Instrument the core for line coverage (GCC/Clang)" OFF)

if(MSVC)
  target_compile_options(scoreboard-core PRIVATE /W4 /permissive- /utf-8)
  if(SB_WARNINGS_AS_ERRORS)
    target_compile_options(scoreboard-core PRIVATE /WX)
  endif()
else()
  target_compile_options(scoreboard-core PRIVATE -Wall -Wextra -Wpedantic)
  if(SB_WARNINGS_AS_ERRORS)
    target_compile_options(scoreboard-core PRIVATE -Werror)
  endif()
endif()

if(SB_COVERAGE)
  target_compile_options(scoreboard-core PUBLIC --coverage -O0)
  target_link_options(scoreboard-core PUBLIC --coverage)
endif()
