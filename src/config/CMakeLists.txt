#
# config
#

set(__target config)

message(VERBOSE "Resolving GIT Version")
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
  message(VERBOSE "GIT hash: ${_build_version}")
else()
  message(VERBOSE "GIT not found")
endif()

# string(TIMESTAMP _time_stamp "%Y.%m.%d %H%M")
string(TIMESTAMP _time_stamp)

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
find_package(Boost 1.79 COMPONENTS program_options)

configure_file(${pierre_SOURCE_DIR}/cmake/version.h.in ${pierre_BINARY_DIR}/version.h @ONLY)

set(HEADER_PATH               ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL         ${HEADER_PATH}/${__target} ${pierre_BINARY_DIR})

file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/**/*.h*"
)

add_library(${__target} SHARED 
  config.cpp args.cpp
  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE 
  ${HEADER_PATH_LOCAL}
)

target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  ${Boost_LIBRARIES}
  base
  stdc++fs
)

target_link_libraries(${__target} PUBLIC 
  tomlplusplus::tomlplusplus
)
