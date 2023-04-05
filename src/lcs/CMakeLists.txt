#
# config
#

set(__target lcs)

find_package(Boost 1.81 QUIET REQUIRED COMPONENTS program_options)

set(HEADER_PATH               ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL         ${HEADER_PATH}/${__target})

file(GLOB HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.h*"
)

add_library(${__target}  
  ${HEADER_LIST}
  config.cpp
  config_watch.cpp
  logger.cpp
  stats.cpp
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE ${HEADER_PATH_LOCAL})
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  base
  stdc++fs
)

target_link_libraries(${__target} PUBLIC 
  ${Boost_LIBRARIES}
  build_inject
  git_version
  InfluxData::InfluxDB
  tomlplusplus::tomlplusplus
)