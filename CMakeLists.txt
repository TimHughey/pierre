#
# Pierre
#
# Top-level Cmake configuration
#

cmake_minimum_required(VERSION 3.13.4)


# set the project name and version
project(pierre
  VERSION 1.0
  DESCRIPTION "Audio stream processing for Ruth"
  LANGUAGES CXX)

# Only do these if this is the main project, and not if it is included through add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  # Let's ensure -std=c++xx instead of -std=g++xx
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED True)

  set(COMPILER_FLAGS)
  list(APPEND CXX_FLAGS PUBLIC
  "-Wall" "-Wextra" "-pedantic" "-O3" "-mcpu=cortex-a72")

    #  "-mfloat-abi=hard" "-mfpu=neon-fp-armv8")

  # Let's nicely support folders in IDEs
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

add_subdirectory(src)
add_subdirectory(apps)
