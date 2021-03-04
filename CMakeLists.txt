cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(pierre
  VERSION 1.0
  DESCRIPTION "Audio stream processing for Ruth"
  LANGUAGES CXX)

# Only do these if this is the main project, and not if it is included through add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  # Let's ensure -std=c++xx instead of -std=g++xx
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_STANDARD_REQUIRED True)

  # Let's nicely support folders in IDEs
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

# The compiled library code is here
add_subdirectory(src)

# The executable code is here
add_subdirectory(apps)
