cmake_minimum_required(VERSION 3.10)

# GLOBAL
##project(tascar LANGUAGES CXX)

if(WIN32)
else()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../version.cmake)
set(PROJECT_VERSION ${VERSION})
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../cmake/modules)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
message("Building libtascar version ${VERSION}")
message("Building components with version ${FULLVERSION}")
configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/tascarver.h.in"
        "${CMAKE_CURRENT_LIST_DIR}/include/tascarver.h"
)
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

# Source file management
file(GLOB LIB_HEADER ${CMAKE_CURRENT_LIST_DIR}/include/*.h)
set(LIB_SOURCES
        src/licensehandler.cc
        src/audiostates.cc
        src/coordinates.cc
        src/audiochunks.cc
        src/xmlconfig.cc
        src/dynamicobjects.cc
        src/sourcemod.cc
        src/receivermod.cc
        src/filterclass.cc
        src/osc_helper.cc
        src/pluginprocessor.cc
        src/acousticmodel.cc
        src/scene.cc
        src/render.cc
        src/session_reader.cc
        src/session.cc
        src/jackclient.cc
        src/delayline.cc
        src/async_file.cc
        src/errorhandling.cc
        src/osc_scene.cc
        src/ringbuffer.cc
        src/viewport.cc
        src/sampler.cc
        src/jackiowav.cc
        src/jackclient.cc
        src/cli.cc
        src/irrender.cc
        src/jackrender.cc
        src/audioplugin.cc
        src/levelmeter.cc
        src/serviceclass.cc
        src/speakerarray.cc
        src/spectrum.cc
        src/fft.cc
        src/stft.cc
        src/ola.cc
        src/vbap3d.cc
        src/hoa.cc
		src/windows_port.cc
        )
if (Linux)
    list(APPEND LIB_HEADER
            ${CMAKE_CURRENT_LIST_DIR}/include/alsamidicc.h
            )
    list(APPEND LIB_SOURCES
            ${CMAKE_CURRENT_LIST_DIR}/src/alsamidicc.cc
            )
endif ()
set(DMX_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/termsetbaud.cc
        ${CMAKE_CURRENT_LIST_DIR}/src/serialport.cc
        ${CMAKE_CURRENT_LIST_DIR}/src/dmxdriver.cc
        )
set(UI_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/gui_elements.cc
        ${CMAKE_CURRENT_LIST_DIR}/src/pdfexport.cc
        )


# REQUIREMENTS
find_package(Boost REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(Threads REQUIRED)
find_package(pthread REQUIRED)
find_package(Jack REQUIRED) 
find_package(LibSndFile REQUIRED) 
find_package(LibSampleRate REQUIRED) 
find_package(FFTW REQUIRED) 
find_package(Eigen3 REQUIRED)
find_package(GSL REQUIRED)
find_package(Glib REQUIRED)
find_package(Glibmm REQUIRED)
find_package(liblo REQUIRED)
#find_package(GTK3 COMPONENTS gtkmm)
find_package(LibXml++ REQUIRED)
find_package(dlfcn-win32 REQUIRED)

if(WIN32)
set(CMAKE_DL_LIBS dlfcn-win32::dl)
find_package(PCRE)
endif()

if (JACK_FOUND AND LIBSNDFILE_FOUND AND LIBXML++_FOUND AND FFTW_FOUND AND LIBLO_FOUND AND LIBSAMPLERATE_FOUND AND GSL_FOUND AND EIGEN3_FOUND)
    message("All dependencies ok!")
else ()
    message("Missing dependencies.")
endif ()

# Cross platform path manipulation
add_subdirectory(third_party/cwalk)

# ADDITIONAL FLAGS
set(PLUGINPREFIX "" CACHE STRING "Prefix for plugins")
if (NOT PLUGINPREFIX MATCHES "")
    message("Using plugin prefix ${PLUGINPREFIX}")
    add_compile_definitions(PLUGINPREFIX=${PLUGINPREFIX})
endif ()
add_compile_definitions(CMAKE)
add_definitions(-D_USE_MATH_DEFINES) # For M_PI
# CMake package config


# TARGET
# libtascar
add_library(tascar ${LIB_SOURCES})
target_include_directories(tascar
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${LIBXML++_INCLUDE_DIRS}
        ${LIBLO_INCLUDE_DIRS}
        ${LIBSNDFILE_INCLUDE_DIRS}
        ${LIBSAMPLERATE_INCLUDE_DIRS}
        ${FFTW_INCLUDE_DIRS}
        ${JACK_INCLUDE_DIRS}
        ${EIGEN3_INCLUDE_DIR}
        ${GSL_INCLUDE_DIR}
		${CMAKE_CURRENT_LIST_DIR}/../../../third_party/unistd
		${CMAKE_CURRENT_LIST_DIR}/../../../third_party/getopt-for-windows
        )
target_link_libraries(tascar        
        Threads::Threads
		cwalk        
		${PTHREAD_LIBRARIES}
		${LIBXML2_LIBRARIES}
		${GLIB_LIBRARIES}
		${GLIBMM_LIBRARIES}
        ${LIBXML++_LIBRARIES}
        ${LIBLO_LIBRARIES}
        ${LIBSNDFILE_LIBRARIES}
        ${LIBSAMPLERATE_LIBRARIES}
        ${FFTW_LIBRARIES}
        ${JACK_LIBRARIES}
        ${GSL_LIBRARIES}
        ${CMAKE_DL_LIBS}
		${PCRE_LIBRARY}
        )
set_target_properties(tascar PROPERTIES PUBLIC_HEADER "${LIB_HEADER}")
set(TARGETS_EXPORT_NAME tascarTarget)
set(INCLUDE_INSTALL_DIR include/tascar CACHE PATH "Install path for include files")
configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/tascarConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tascar"
        PATH_VARS INCLUDE_INSTALL_DIR
)
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/tascarConfigVersion.cmake"
        VERSION ${VERSION}
        COMPATIBILITY SameMajorVersion
)
if (CREATE_INSTALL)
install(
        TARGETS tascar
        EXPORT "${TARGETS_EXPORT_NAME}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tascar"
)
install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/tascarConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/tascarConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tascar"
)
install(
        EXPORT "${TARGETS_EXPORT_NAME}"
        NAMESPACE "tascar::"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tascar"
)
endif()

# libtascargui
if (GTK3_FOUND)
    add_library(tascargui SHARED ${UI_SOURCES})
    target_compile_definitions(tascargui PRIVATE -DGTKMM30)
    target_include_directories(tascargui
            PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            PUBLIC
            #${LIBXML++_INCLUDE_DIRS}
            #${JACK_INCLUDE_DIRS}
            ${GTK3_INCLUDE_DIRS}
            )
    target_link_libraries(tascargui
            PRIVATE
            Threads::Threads			
            PUBLIC
            tascar
            #${LIBXML++_LIBRARIES}
            #${JACK_LIBRARIES}
            ${GTK3_LIBRARIES}
            )
    install(
            TARGETS tascargui
            EXPORT "${TARGETS_EXPORT_NAME}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tascar"
    )
endif ()

if (Linux)
    # libtascardmx
    add_library(tascardmx SHARED ${DMX_SOURCES})
    target_include_directories(tascardmx
            PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            )
    target_link_libraries(tascardmx
            PUBLIC
            tascar
            )

    install(
            TARGETS tascardmx
            EXPORT "${TARGETS_EXPORT_NAME}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tascar"
    )
    install(
            FILES "${generated_dir}/tascarDmxConfig.cmake" "${generated_dir}/tascarDmxConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tascarDmx"
    )
    install(
            EXPORT "${TARGETS_EXPORT_NAME}"
            NAMESPACE "tascar::"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tascar"
    )
endif ()

if (LINUX)
    if (TARGET tascargui)
        export(TARGETS tascar tascargui tascardmx NAMESPACE tascar:: FILE tascarTarget.cmake)
    elseif ()
        export(TARGETS tascar tascardmx NAMESPACE tascar:: FILE tascarTarget.cmake)

    endif ()
else ()
    if (TARGET tascargui)
        export(TARGETS tascar tascargui NAMESPACE tascar:: FILE tascarTarget.cmake)
    elseif ()
        export(TARGETS tascar NAMESPACE tascar:: FILE tascarTarget.cmake)
    endif ()
endif ()
