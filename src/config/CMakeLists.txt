#
# Fallback Config
#

add_subdirectory(fallback)

set(__target config)

set(HEADER_PATH               ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL         ${HEADER_PATH}/${__target})
set(HEADER_PATH_EMBED         ${HEADER_PATH_LOCAL}/embed)
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/**/*.h*"
)

add_library(${__target} SHARED 
  config.cpp 
  embed/embed.cpp
  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE 
  ${HEADER_PATH_LOCAL}
  ${CMAKE_BINARY_DIR}
)

# external users of this library include headers relative to
# the based include directory
target_include_directories(${__target} PRIVATE 
  ${HEADER_PATH_LOCAL} ${HEADER_PATH_EMBED}
)

target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE base stdc++fs fallback)
target_link_libraries(${__target} PUBLIC ArduinoJson)
