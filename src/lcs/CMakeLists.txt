#
# config
#

set(__target lcs)

find_package(Boost 1.80 QUIET REQUIRED COMPONENTS program_options)

message(VERBOSE "Resolving GIT Version")
set(_build_version_ "unknown")

find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe
    WORKING_DIRECTORY ${pierre_SOURCE_DIR}
    OUTPUT_VARIABLE _build_version_
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(STATUS "GIT hash: ${_build_version_}")
else()
  message(VERBOSE "GIT not found")
endif()

# string(TIMESTAMP _time_stamp "%Y.%m.%d %H%M")
string(TIMESTAMP _build_timestamp_)
message(STATUS "Build timestamp: ${_build_timestamp_}")

configure_file(
  ${pierre_SOURCE_DIR}/cmake/version.hpp.in 
  ${pierre_BINARY_DIR}/version.hpp @ONLY)


set(HEADER_PATH               ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL         ${HEADER_PATH}/${__target})
set(HEADER_PATH_BIN           ${pierre_BINARY_DIR})

file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/**/*.h*"
)

add_library(${__target} SHARED 
  ${HEADER_LIST}
  args.cpp
  config.cpp
  lcs.cpp
  logger.cpp
  stats.cpp
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE 
  ${HEADER_PATH_LOCAL} ${HEADER_PATH_BIN}
)

target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  ${Boost_LIBRARIES}
  base
  stdc++fs
)

target_link_libraries(${__target} PUBLIC 
  InfluxData::InfluxDB
  io
  tomlplusplus::tomlplusplus
)