# - Try to find libxml++
# Once done, this will define
#
#  LIBXML++_FOUND - system has libxml++
#  LIBXML++_INCLUDE_DIRS - the libxml++ include directories
#  LIBXML++_LIBRARIES - link these to use libxml++

# Use pkg-config to get hints about paths

if (LIBXML++_LIBRARIES AND LIBXML++_INCLUDE_DIRS)
    set(LIBXML++_FOUND TRUE)
else ()
    find_package(Glibmm REQUIRED)
    find_package(LibXml2 REQUIRED)
    find_package(PkgConfig QUIET)
    if (PKG_CONFIG_FOUND)
        pkg_check_modules(LIBXML++_PKGCONF libxml++-2.6)
    endif (PKG_CONFIG_FOUND)

    # Include dir
    find_path(LIBXML++_INCLUDE_DIR
            NAMES libxml++/libxml++.h
            PATHS ${LIBXML++_PKGCONF_INCLUDE_DIRS}
            )

    # Include dir
    find_path(LIBXML++_CONFIG_INCLUDE_DIR
            NAMES libxml++config.h
            PATHS ${LIBXML++_PKGCONF_INCLUDE_DIRS}
            )

    # Library
    find_library(LIBXML++_LIBRARY
            NAMES xml++ xml++-2.6
            PATHS ${LIBXML++_PKGCONF_LIBRARY_DIRS}
            )

    find_package(PackageHandleStandardArgs)
    find_package_handle_standard_args(LibXml++ DEFAULT_MSG LIBXML++_LIBRARY LIBXML++_INCLUDE_DIR LIBXML++_CONFIG_INCLUDE_DIR)

    if (LIBXML++_FOUND)	
        set(LIBXML++_LIBRARIES ${LIBXML++_LIBRARY} ${LIBXML_LIBRARIES} ${GLIBMM_LIBRARIES})
        set(LIBXML++_INCLUDE_DIRS ${LIBXML++_INCLUDE_DIR} ${LIBXML++_CONFIG_INCLUDE_DIR} ${GLIBMM_INCLUDE_DIRS})
		message("LibXml++ libraries are ${LIBXML++_LIBRARIES}")
    endif (LIBXML++_FOUND)

    # Mark the singular variables as advanced, to hide them of the GUI by default
    mark_as_advanced(LIBXML++_LIBRARY LIBXML++_INCLUDE_DIR)
endif ()


