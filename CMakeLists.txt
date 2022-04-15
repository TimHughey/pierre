#
# Pierre
#
# Top-level Cmake configuration
#

cmake_minimum_required(VERSION 3.22.0)
set(FIND_LIBRARY_USE_LIB64_PATHS true)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

# set the project name and version
project(pierre
  VERSION 1.0
  DESCRIPTION "Custom light show for Wiss Landing"
  LANGUAGES CXX C
)

set(_target pierre)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# only do the following in the base directory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  # Let's ensure -std=c++xx instead of -std=g++xx
  set(CMAKE_CXX_EXTENSIONS ON)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED True)

  set(CMAKE_C_STANDARD 17)
  set(CMAKE_C_STANDARD_REQUIRED True)

  set(COMPILER_FLAGS)
  list(APPEND CXX_FLAGS
  "-Wall" "-Wextra" "-pedantic"
  "-Wno-parentheses"
  "-O3"
  "-march=native")

  # Let's nicely support folders in IDEs
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

find_package(Threads REQUIRED)

message(STATUS "Resolving GIT Version")

set(_build_version "unknown")

find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe
    WORKING_DIRECTORY ${pierre_SOURCE_DIR}
    OUTPUT_VARIABLE _build_version
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message( STATUS "GIT hash: ${_build_version}")
else()
  message(STATUS "GIT not found")
endif()

string(TIMESTAMP _time_stamp "%Y.%m.%d %H%M")

add_subdirectory(src libs)

add_executable(${_target}
  apps/pierre.cpp apps/main.cpp
)

configure_file(cmake/version.h.in version.h @ONLY)

target_include_directories(${_target} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}
)

target_link_libraries(${_target} PUBLIC
  Misc Core Audio DMX Lightdesk mDNS RTSP
  ${Boost_LIBRARIES}
  Threads::Threads
  stdc++fs
)
