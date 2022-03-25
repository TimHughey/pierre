#
# Pierre
#
# Top-level Cmake configuration
#

cmake_minimum_required(VERSION 3.22.0)


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

find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

# set(Boost_USE_STATIC_LIBS        ON)  # only find static libs
set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
find_package(Boost 1.67.0 COMPONENTS
  system regex date_time timer program_options
)

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif()

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

add_subdirectory(src)

set(AUDIO src/audio)
set(CORE src/core)
set(DMX src/dmx)
SET(LIGHTDESK src/lightdesk)
SET(FADERS src/lightdesk/faders)
SET(HEADUNITS src/lightdesk/headunits)
SET(FX src/lightdesk/fx)
set(MISC src/misc)
set(CLI src/cli)
set(APPS apps)


add_executable(${_target}
  ${AUDIO}/dsp.cpp ${AUDIO}/pcm.cpp ${AUDIO}/fft.cpp ${AUDIO}/peaks.cpp
  ${CORE}/config.cpp ${CORE}/state.cpp
  ${DMX}/packet.cpp ${DMX}/net.cpp ${DMX}/render.cpp
  ${MISC}/random.cpp ${MISC}/elapsed.cpp
  ${LIGHTDESK}/lightdesk.cpp ${LIGHTDESK}/color.cpp
  ${FADERS}/fader.cpp ${FADERS}/easings.cpp
  ${HEADUNITS}/pinspot.cpp
  ${FX}/fx.cpp ${FX}/majorpeak.cpp ${FX}/leave.cpp
  ${CLI}/linenoise.c ${CLI}/cli.cpp ${CLI}/subcmds/dsp.cpp ${CLI}/subcmds/lightdesk.cpp
  ${APPS}/pierre.cpp ${APPS}/main.cpp
)

configure_file(cmake/version.h.in version.h @ONLY)

target_include_directories(${_target} PRIVATE
  include/pierre
  ${CMAKE_BINARY_DIR}
)

target_compile_options(${_target} PRIVATE ${CXX_FLAGS})

target_link_libraries(${_target}
  ${Boost_LIBRARIES}
  ALSA::ALSA
  Threads::Threads
  stdc++fs
)
