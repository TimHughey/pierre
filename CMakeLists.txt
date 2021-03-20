#
# Pierre
#
# Top-level Cmake configuration
#

cmake_minimum_required(VERSION 3.13.4)


# set the project name and version
project(pierre
  VERSION 1.0
  DESCRIPTION "Custom light show for Wiss Landing"
  LANGUAGES CXX C
)

# only do the following in the base directory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  # Let's ensure -std=c++xx instead of -std=g++xx
  set(CMAKE_CXX_EXTENSIONS ON)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED True)

  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED True)

  set(COMPILER_FLAGS)
  list(APPEND CXX_FLAGS
  "-Wall" "-Wextra" "-pedantic"
  "-Wno-parentheses"
  "-O3"
  "-mcpu=cortex-a72" "-march=armv8-a+fp+simd")

  # Let's nicely support folders in IDEs
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

set(Boost_USE_STATIC_LIBS        ON)  # only find static libs
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

include_directories(AFTER
  include/pierre
)

set(AUDIO src/audio)
set(DMX src/dmx)
SET(LIGHTDESK src/lightdesk)
SET(HEADUNITS src/lightdesk/headunits)
SET(FX src/lightdesk/fx)
set(MISC src/misc)
set(CLI src/cli)
set(APPS apps)

add_executable(pierre
  ${AUDIO}/dsp ${AUDIO}/pcm ${AUDIO}/fft
  ${DMX}/packet ${DMX}/net ${DMX}/render
  ${MISC}/random
  ${LIGHTDESK}/lightdesk
  ${HEADUNITS}/pinspot
  ${FX}/majorpeak
  ${CLI}/linenoise ${CLI}/cli
  ${APPS}/pierre ${APPS}/main
)

target_compile_options(pierre PRIVATE ${CXX_FLAGS})

target_link_libraries(pierre
  ${Boost_LIBRARIES}
  ALSA::ALSA
  Threads::Threads
)
