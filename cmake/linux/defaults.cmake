# CMake Linux defaults module

# cmake-format: off
# cmake-lint: disable=C0103
# cmake-lint: disable=C0111
# cmake-format: on

include_guard(GLOBAL)

include(GNUInstallDirs)

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_C_LIBRARY_ARCHITECTURE}")

# Detect Fedora (or other RPM-based distros) and switch the CPack generator
set(_IS_RPM_DISTRO OFF)
if(EXISTS "/etc/fedora-release"
   OR EXISTS "/etc/redhat-release"
   OR EXISTS "/etc/rocky-release"
   OR EXISTS "/etc/almalinux-release")
  set(_IS_RPM_DISTRO ON)
endif()

if(_IS_RPM_DISTRO)
  set(CPACK_GENERATOR "RPM")
  set(CPACK_RPM_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-x86_64")
  set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
  set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0")
  set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
  set(CPACK_RPM_PACKAGE_REQUIRES "obs-studio")
  set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25.0 OR NOT CMAKE_CROSSCOMPILING)
    set(CPACK_RPM_DEBUGINFO_PACKAGE ON)
  endif()
else()
  set(CPACK_GENERATOR "DEB")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PLUGIN_EMAIL}")
  set(CPACK_SET_DESTDIR ON)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25.0 OR NOT CMAKE_CROSSCOMPILING)
    set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
  endif()
endif()

set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/release")

set(CPACK_SOURCE_GENERATOR "TXZ")
set(CPACK_SOURCE_IGNORE_FILES
    # cmake-format: sortable
    ".*~$"
    \\.git/
    \\.github/
    \\.gitignore
    build_.*
    cmake/\\.CMakeBuildNumber
    release/)

set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-source")
set(CPACK_ARCHIVE_THREADS 0)

include(CPack)

find_package(libobs QUIET)

if(NOT TARGET OBS::libobs)
  find_package(LibObs REQUIRED)
  add_library(OBS::libobs ALIAS libobs)

  if(ENABLE_FRONTEND_API)
    find_path(
      obs-frontend-api_INCLUDE_DIR
      NAMES obs-frontend-api.h
      PATHS /usr/include /usr/local/include
      PATH_SUFFIXES obs)

    find_library(
      obs-frontend-api_LIBRARY
      NAMES obs-frontend-api
      PATHS /usr/lib /usr/local/lib)

    if(obs-frontend-api_LIBRARY)
      if(NOT TARGET OBS::obs-frontend-api)
        if(IS_ABSOLUTE "${obs-frontend-api_LIBRARY}")
          add_library(OBS::obs-frontend-api UNKNOWN IMPORTED)
          set_property(TARGET OBS::obs-frontend-api PROPERTY IMPORTED_LOCATION "${obs-frontend-api_LIBRARY}")
        else()
          add_library(OBS::obs-frontend-api INTERFACE IMPORTED)
          set_property(TARGET OBS::obs-frontend-api PROPERTY IMPORTED_LIBNAME "${obs-frontend-api_LIBRARY}")
        endif()

        set_target_properties(OBS::obs-frontend-api PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                               "${obs-frontend-api_INCLUDE_DIR}")
      endif()
    endif()
  endif()

  macro(find_package)
    if(NOT "${ARGV0}" STREQUAL libobs AND NOT "${ARGV0}" STREQUAL obs-frontend-api)
      _find_package(${ARGV})
    endif()
  endmacro()
endif()