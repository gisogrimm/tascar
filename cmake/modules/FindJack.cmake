# - Try to find JACK
# Once done, this will define
#
#  JACK_FOUND - system has JACK
#  JACK_INCLUDE_DIRS - the JACK include directories
#  JACK_LIBRARIES - link these to use JACK

# Use pkg-config to get hints about paths

if (JACK_LIBRARIES AND JACK_INCLUDE_DIRS)
    set(JACK_FOUND TRUE)
else ()
    find_package(PkgConfig QUIET)
    if (PKG_CONFIG_FOUND)
        pkg_check_modules(JACK_PKGCONF jack)
    endif (PKG_CONFIG_FOUND)

    # Include dir
    find_path(JACK_INCLUDE_DIR
            NAMES jack/jack.h
            PATH_SUFFIXES include includes
            PATHS ${JACK_PKGCONF_INCLUDE_DIRS} ${CURRENT_PROJECT_DIR}/third_party/vcpkg/installed
            )
	message("JACK_INCLUDE_DIR is ${JACK_INCLUDE_DIR}")

    # Library
    find_library(JACK_LIBRARY
            NAMES jack jack64 libjack libjack64 jack2
            PATH_SUFFIXES lib
            PATHS ${JACK_PKGCONF_LIBRARY_DIRS} ${CURRENT_PROJECT_DIR}/third_party/vcpkg/installed
            )
	message("JACK_LIBRARY is ${JACK_LIBRARY}")

    find_package(PackageHandleStandardArgs)
    find_package_handle_standard_args(Jack DEFAULT_MSG JACK_LIBRARY JACK_INCLUDE_DIR)

    if (JACK_FOUND)
        set(JACK_LIBRARIES ${JACK_LIBRARY})
        set(JACK_INCLUDE_DIRS ${JACK_INCLUDE_DIR})
    endif (JACK_FOUND)

    mark_as_advanced(JACK_LIBRARY JACK_INCLUDE_DIR)
endif ()
