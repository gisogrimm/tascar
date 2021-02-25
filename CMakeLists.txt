cmake_minimum_required(VERSION 3.16)

# set the project name and version
#project(tascar LANGUAGES CXX)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
        # Check whether we are running under Rosetta on arm64 hardware.
        execute_process(COMMAND sysctl -q hw.optional.arm64
                OUTPUT_VARIABLE _sysctl_stdout
                ERROR_VARIABLE _sysctl_stderr
                RESULT_VARIABLE _sysctl_result
                )
        if (_sysctl_result EQUAL 0 AND _sysctl_stdout MATCHES "hw.optional.arm64: 1")
            #message("Compiling multi build for x84_64 and arm64")
            set(CMAKE_HOST_SYSTEM_PROCESSOR "arm64")
            set(QMAKE_APPLE_DEVICE_ARCHS arm64)
            set(CMAKE_OSX_ARCHITECTURES arm64)
        else ()
            set(CMAKE_OSX_ARCHITECTURES x86_64)
        endif ()
    endif ()
endif ()

set(PLUGINPREFIX "" CACHE STRING "Prefix for plugins")

# LIBTASCAR
add_subdirectory(libtascar)
add_subdirectory(plugins)

# CLI
add_executable(tascar_cli
        apps/src/tascar_cli.cc
        )
target_include_directories(tascar_cli
        PRIVATE
        ${PROJECT_SOURCE_DIR}/libtascar/include
        )

target_link_libraries(tascar_cli
        PRIVATE
        tascar
        )
install(TARGETS tascar_cli DESTINATION bin)
