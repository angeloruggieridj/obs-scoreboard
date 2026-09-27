# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

# project(VERSION) does not accept "1.0.0-beta1": the numbers live in buildspec's `version`,
# the suffix in `prerelease`, and they are joined here for plugin-support.c.in. The release
# workflow refuses a tag that disagrees with this string.
string(JSON _prerelease ERROR_VARIABLE _prerelease_err GET ${buildspec} prerelease)
if(_prerelease_err OR _prerelease STREQUAL "NOTFOUND" OR _prerelease STREQUAL "")
  set(PLUGIN_VERSION_FULL "${CMAKE_PROJECT_VERSION}")
else()
  set(PLUGIN_VERSION_FULL "${CMAKE_PROJECT_VERSION}-${_prerelease}")
endif()
message(STATUS "obs-scoreboard reports itself as ${PLUGIN_VERSION_FULL}")

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_subdirectory(src/core)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs scoreboard-core)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Gui REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)
endif()

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin")

# The template's cmake/windows/compilerconfig.cmake only applies a blanket /W3 to every
# target; unlike scoreboard-core, this module never asked for /W4, so our own code (as
# opposed to the OBS/Qt headers it includes) was never actually checked at the project's
# usual warning level. /external:anglebrackets + /external:W0 tells MSVC that headers
# included with <angle brackets> (obs-module.h, obs-frontend-api.h, every Qt header used
# here) are external and keeps them at warning level 0, so /W4 only judges our own
# "quoted" headers and .cpp files -- never lowered to accommodate a header we do not own.
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4 /permissive- /external:anglebrackets /external:W0>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin/plugin-main.cpp src/plugin/ScoreboardDock.cpp src/plugin/ScoreboardDock.hpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(OS_LINUX)
  install(
    CODE
      "set(SB_LIBDIR \"${CMAKE_INSTALL_LIBDIR}\")
set(SB_DATADIR \"${CMAKE_INSTALL_DATAROOTDIR}\")
set(SB_NAME \"${_name}\")"
  )
  install(SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux/install-obs-layouts.cmake")
endif()

option(ENABLE_TESTS "Build obs-scoreboard unit tests" ON)
if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
