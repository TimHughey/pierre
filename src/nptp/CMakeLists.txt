#
# nptp
#

set(THREADS_PREFER_PTHREAD_FLAG true)

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE
  HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH}/nptp/*.h*"
)

add_library(nptp
  nptp.cpp clock_info.cpp
  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without usinf the full path
target_include_directories(nptp PRIVATE ${HEADER_PATH}/nptp)

# external users of this library include headers relative to
# the based include directory
target_include_directories(nptp PUBLIC ${HEADER_PATH})

target_link_libraries(nptp PUBLIC
  core
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
