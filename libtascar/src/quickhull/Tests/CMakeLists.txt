cmake_minimum_required(VERSION 3.11)

set(SOURCE_FILES
  ${CMAKE_SOURCE_DIR}/QuickHullTests.cpp
  ${CMAKE_SOURCE_DIR}/main.cpp
  ${CMAKE_SOURCE_DIR}/../QuickHull.cpp
  )

add_definitions(-std=c++17)
add_executable(QuickHullTests ${SOURCE_FILES})
