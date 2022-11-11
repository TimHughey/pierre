#
# Pierre
#
# Top-level Cmake configuration
#

cmake_minimum_required(VERSION 3.23.0)

set(FIND_LIBRARY_USE_LIB64_PATHS  true)
set(CMAKE_VERBOSE_MAKEFILE        ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

set(Boost_USE_DEBUG_LIBS        OFF) # ignore debug libs
set(Boost_USE_RELEASE_LIBS      ON)  # only find release libs
set(Boost_USE_MULTITHREADED     ON)
set(Boost_THREADAPI             pthread)
set(THREADS_PREFER_PTHREAD_FLAG true)

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
  set(CMAKE_CXX_EXTENSIONS            OFF)
  set(CMAKE_CXX_STANDARD              20)
  set(CMAKE_CXX_STANDARD_REQUIRED     TRUE)

  set(CMAKE_C_EXTENSIONS              ON)
  set(CMAKE_C_STANDARD                17)
  set(CMAKE_C_STANDARD_REQUIRED       TRUE)

  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

# find pthreads
set(THREADS_PREFER_PTHREAD_FLAG true)
find_package(Threads REQUIRED)

add_subdirectory(src)

add_executable(${_target} apps/pierre.cpp)

target_include_directories(${_target} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${_target} PUBLIC
  Threads::Threads 
  base config frame desk airplay mdns
)


