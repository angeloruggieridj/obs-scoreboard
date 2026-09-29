# Common build dependencies module

include_guard(GLOBAL)

# _check_deps_version: Checks for obs-deps VERSION file in prefix paths
function(_check_deps_version version)
  set(found FALSE)

  foreach(path IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${path}/share/obs-deps/VERSION")
      if(dependency STREQUAL qt6 AND NOT EXISTS "${path}/lib/cmake/Qt6/Qt6Config.cmake")
        set(found FALSE)
        continue()
      endif()

      file(READ "${path}/share/obs-deps/VERSION" _check_version)
      string(REPLACE "\n" "" _check_version "${_check_version}")
      string(REPLACE "-" "." _check_version "${_check_version}")
      string(REPLACE "-" "." version "${version}")

      if(_check_version VERSION_EQUAL version)
        set(found TRUE)
        break()
      elseif(_check_version VERSION_LESS version)
        message(
          AUTHOR_WARNING
          "Older ${label} version detected in ${path}: \n"
          "Found ${_check_version}, require ${version}"
        )
        list(REMOVE_ITEM CMAKE_PREFIX_PATH "${path}")
        list(APPEND CMAKE_PREFIX_PATH "${path}")
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
        continue()
      else()
        message(
          AUTHOR_WARNING
          "Newer ${label} version detected in ${path}: \n"
          "Found ${_check_version}, require ${version}"
        )
        set(found TRUE)
        break()
      endif()
    endif()
  endforeach()

  return(PROPAGATE found CMAKE_PREFIX_PATH)
endfunction()

# _setup_obs_studio: Create obs-studio build project, then build libobs and obs-frontend-api
function(_setup_obs_studio)
  if(NOT libobs_DIR)
    set(_is_fresh --fresh)
  endif()

  if(OS_WINDOWS)
    set(_cmake_generator "${CMAKE_GENERATOR}")
    # OBS 30.0's own buildspec.cmake reuses CMAKE_VS_PLATFORM_NAME as its deps architecture,
    # so the ",version=..." platform suffix breaks it; the SDK is pinned through
    # CMAKE_SYSTEM_VERSION below instead.
    set(_cmake_arch "-A ${arch}")
    set(_cmake_extra "-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION} -DCMAKE_ENABLE_SCRIPTING=OFF")
  elseif(OS_MACOS)
    set(_cmake_generator "Xcode")
    set(_cmake_arch "-DCMAKE_OSX_ARCHITECTURES:STRING='arm64;x86_64'")
    set(_cmake_extra "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    # OBS 30.0 reads the SDK version from the CMAKE_OSX_SYSROOT path (MacOSX<version>.sdk),
    # which CMake 4 no longer sets by default: hand it the versioned SDK path explicitly.
    execute_process(
      COMMAND xcrun --sdk macosx --show-sdk-path
      OUTPUT_VARIABLE _sdk_path
      OUTPUT_STRIP_TRAILING_WHITESPACE
      COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
      COMMAND xcrun --sdk macosx --show-sdk-version
      OUTPUT_VARIABLE _sdk_version
      OUTPUT_STRIP_TRAILING_WHITESPACE
      COMMAND_ERROR_IS_FATAL ANY
    )
    cmake_path(GET _sdk_path PARENT_PATH _sdk_dir)
    if(EXISTS "${_sdk_dir}/MacOSX${_sdk_version}.sdk")
      set(_sdk_path "${_sdk_dir}/MacOSX${_sdk_version}.sdk")
    endif()
    list(APPEND _cmake_extra "-DCMAKE_OSX_SYSROOT=${_sdk_path}")
    # OBS 30.0 turns warnings into errors by default; Xcode 16 postdates it and only libobs
    # and obs-frontend-api are built here, so its new warnings must not stop the build.
    list(APPEND _cmake_extra "-DCMAKE_COMPILE_WARNING_AS_ERROR:BOOL=OFF")
  endif()

  message(STATUS "Configure ${label} (${arch})")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -S "${dependencies_dir}/${_obs_destination}" -B
      "${dependencies_dir}/${_obs_destination}/build_${arch}" -G ${_cmake_generator}
      "${_cmake_arch}" -DOBS_CMAKE_VERSION:STRING=3.0.0 -DENABLE_PLUGINS:BOOL=OFF
      -DENABLE_FRONTEND:BOOL=OFF -DOBS_VERSION_OVERRIDE:STRING=${_obs_version}
      "-DCMAKE_PREFIX_PATH='${CMAKE_PREFIX_PATH}'" ${_is_fresh} ${_cmake_extra}
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Configure ${label} (${arch}) - done")

  message(STATUS "Build ${label} (Debug - ${arch})")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" --build build_${arch} --target obs-frontend-api --config Debug --parallel
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Build ${label} (Debug - ${arch}) - done")

  message(STATUS "Build ${label} (Release - ${arch})")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" --build build_${arch} --target obs-frontend-api --config Release --parallel
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Build ${label} (Reelase - ${arch}) - done")

  message(STATUS "Install ${label} (${arch})")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" --install build_${arch} --component Development --config Debug --prefix
      "${dependencies_dir}"
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" --install build_${arch} --component Development --config Release --prefix
      "${dependencies_dir}"
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Install ${label} (${arch}) - done")
endfunction()

# _check_dependencies: Fetch and extract pre-built OBS build dependencies
function(_check_dependencies)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" buildspec)

  string(JSON dependency_data GET ${buildspec} dependencies)

  foreach(dependency IN LISTS dependencies_list)
    string(JSON data GET ${dependency_data} ${dependency})
    string(JSON version GET ${data} version)
    string(JSON hash GET ${data} hashes ${platform})
    string(JSON url GET ${data} baseUrl)
    string(JSON label GET ${data} label)
    string(JSON revision ERROR_VARIABLE error GET ${data} revision ${platform})

    message(STATUS "Setting up ${label} (${arch})")

    set(file "${${dependency}_filename}")
    set(destination "${${dependency}_destination}")
    string(REPLACE "VERSION" "${version}" file "${file}")
    string(REPLACE "VERSION" "${version}" destination "${destination}")
    string(REPLACE "ARCH" "${arch}" file "${file}")
    string(REPLACE "ARCH" "${arch}" destination "${destination}")
    if(revision)
      string(REPLACE "_REVISION" "_v${revision}" file "${file}")
      string(REPLACE "-REVISION" "-v${revision}" file "${file}")
    else()
      string(REPLACE "_REVISION" "" file "${file}")
      string(REPLACE "-REVISION" "" file "${file}")
    endif()

    if(EXISTS "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256")
      file(
        READ
        "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256"
        OBS_DEPENDENCY_${dependency}_${arch}_HASH
      )
    endif()

    set(skip FALSE)
    if(dependency STREQUAL prebuilt OR dependency STREQUAL qt6)
      if(OBS_DEPENDENCY_${dependency}_${arch}_HASH STREQUAL ${hash})
        _check_deps_version(${version})

        if(found)
          set(skip TRUE)
        endif()
      endif()
    endif()

    if(skip)
      message(STATUS "Setting up ${label} (${arch}) - skipped")
      continue()
    endif()

    if(dependency STREQUAL obs-studio)
      set(url ${url}/${file})
    else()
      set(url ${url}/${version}/${file})
    endif()

    if(NOT EXISTS "${dependencies_dir}/${file}")
      # Retry transient CDN failures (e.g. curl 52 "Server returned nothing")
      # up to 3 times before giving up — GitHub release downloads flake.
      set(error_code 1)
      foreach(download_attempt RANGE 1 3)
        message(STATUS "Downloading ${url} (attempt ${download_attempt}/3)")
        file(
          DOWNLOAD "${url}" "${dependencies_dir}/${file}"
          STATUS download_status
          EXPECTED_HASH SHA256=${hash}
        )

        list(GET download_status 0 error_code)
        list(GET download_status 1 error_message)
        if(error_code GREATER 0)
          message(STATUS "Downloading ${url} - Failure (${error_message})")
          file(REMOVE "${dependencies_dir}/${file}")
          execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 5)
        else()
          message(STATUS "Downloading ${url} - done")
          break()
        endif()
      endforeach()

      if(error_code GREATER 0)
        message(
          FATAL_ERROR
          "Unable to download ${url} after 3 attempts, last error: ${error_message}"
        )
      endif()
    endif()

    if(NOT OBS_DEPENDENCY_${dependency}_${arch}_HASH STREQUAL ${hash})
      file(REMOVE_RECURSE "${dependencies_dir}/${destination}")
    endif()

    if(NOT EXISTS "${dependencies_dir}/${destination}")
      file(MAKE_DIRECTORY "${dependencies_dir}/${destination}")
      if(dependency STREQUAL obs-studio)
        file(ARCHIVE_EXTRACT INPUT "${dependencies_dir}/${file}" DESTINATION "${dependencies_dir}")
      else()
        file(
          ARCHIVE_EXTRACT
          INPUT "${dependencies_dir}/${file}"
          DESTINATION "${dependencies_dir}/${destination}"
        )
      endif()
    endif()

    file(WRITE "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256" "${hash}")

    if(dependency STREQUAL prebuilt)
      list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}/${destination}")
    elseif(dependency STREQUAL qt6)
      list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}/${destination}")
    elseif(dependency STREQUAL obs-studio)
      set(_obs_version ${version})
      set(_obs_destination "${destination}")
      list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}")
      if(OS_WINDOWS)
        # OBS 30.0 installs its package files as cmake/<target>/<target>Config.cmake, a layout
        # find_package does not search under a plain prefix; with cmake/ as a prefix they
        # match its <prefix>/<name>*/ rule.
        list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}/cmake")
      endif()
    endif()

    message(STATUS "Setting up ${label} (${arch}) - done")
  endforeach()

  list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)

  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} CACHE PATH "CMake prefix search path" FORCE)

  _setup_obs_studio()
endfunction()
