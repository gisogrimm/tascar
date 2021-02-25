
# Read version
file(READ "${CMAKE_CURRENT_LIST_DIR}/config.mk" config_file)
string(REGEX MATCH "VERSION=([0-9]*).([0-9]*).([0-9]*)" _ "${config_file}")
set(PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(PROJECT_VERSION_MINOR ${CMAKE_MATCH_2})
set(PROJECT_VERSION_PATCH ${CMAKE_MATCH_3})

# Now read git related
execute_process(COMMAND git log --pretty=format:'%h' -n 1
                OUTPUT_VARIABLE GIT_REV
                ERROR_QUIET)

# Check whether we got any revision (which isn't
# always the case, e.g. when someone downloaded a zip
# file from Github instead of a checkout)
if ("${GIT_REV}" STREQUAL "")
    set(GIT_REV "N/A")
    set(GIT_DIFF "")
    set(GIT_TAG "N/A")
    set(GIT_BRANCH "N/A")
    set(GIT_NUM_COMMITS "0")
else()
    execute_process(
        COMMAND bash -c "git diff --quiet --exit-code || echo modified"
        OUTPUT_VARIABLE GIT_DIFF)
    execute_process(
        COMMAND git describe --exact-match --tags
        OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH)

    if(WIN32)
        execute_process(COMMAND bash -c "git log --pretty='format:%h' origin/master.. | find /c /v ''"
            OUTPUT_VARIABLE GIT_NUM_COMMITS)
    else()
        execute_process(COMMAND bash -c "git log --pretty='format:%h' origin/master.. | wc -w"
            OUTPUT_VARIABLE GIT_NUM_COMMITS)
    endif()

    string(STRIP "${GIT_REV}" GIT_REV)
    string(SUBSTRING "${GIT_REV}" 1 7 GIT_REV)
    string(STRIP "${GIT_DIFF}" GIT_DIFF)
    string(STRIP "${GIT_TAG}" GIT_TAG)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
    string(STRIP "${GIT_NUM_COMMITS}" GIT_NUM_COMMITS)
endif()

set(VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(FULLVERSION "${VERSION}.${GIT_NUM_COMMITS}-${GIT_REV}${$GIT_DIFF}")
message("Version determined as ${FULLVERSION}")
