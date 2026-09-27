# SPDX-License-Identifier: GPL-2.0-or-later
#
# Unit tests for src/core, buildable WITHOUT OBS or Qt. Works two ways:
#   add_subdirectory(tests)                          from the plugin build
#   cmake -S tests -B build_tests && ctest ...       on its own (what CI runs)
cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(obs-scoreboard-tests LANGUAGES CXX)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../src/core" "${CMAKE_CURRENT_BINARY_DIR}/core")
endif()

enable_testing()

add_executable(
  scoreboard-tests
  test_main.cpp
  test_types.cpp
  test_clock.cpp
  test_format.cpp
  test_periods.cpp
  test_counters.cpp
  test_penaltybox.cpp
  test_strength.cpp
  test_sportcatalog.cpp
)
target_link_libraries(scoreboard-tests PRIVATE scoreboard-core)
# doctest's fallback forward declaration of std::basic_ostream breaks when stringifying a
# string_view under the VS 2026 MSVC STL, which defines that operator<< inline: force doctest to
# include the real <ostream>/<istream> headers instead.
target_compile_definitions(scoreboard-tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
target_include_directories(
  scoreboard-tests
  SYSTEM
  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../third_party"
)
set_target_properties(scoreboard-tests PROPERTIES CXX_EXTENSIONS OFF)
# /utf-8: tests contain non-ASCII literals ("Città") that must reach the JSON code as UTF-8.
if(MSVC)
  target_compile_options(scoreboard-tests PRIVATE /W4 /permissive- /utf-8)
  if(SB_WARNINGS_AS_ERRORS)
    target_compile_options(scoreboard-tests PRIVATE /WX)
  endif()
else()
  target_compile_options(scoreboard-tests PRIVATE -Wall -Wextra -Wpedantic)
  if(SB_WARNINGS_AS_ERRORS)
    target_compile_options(scoreboard-tests PRIVATE -Werror)
  endif()
endif()
add_test(NAME core COMMAND scoreboard-tests)
