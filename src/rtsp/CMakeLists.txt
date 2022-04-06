#
# RTSP
#

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH}/rtsp/*.h*")

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
find_package(Boost 1.78.0 COMPONENTS context thread coroutine system REQUIRED)

find_package(fmt)

add_library(RTSP
  rtsp.cpp headers.cpp
  request.cpp
  reply.cpp reply/factory.cpp reply/fairplay.cpp reply/options.cpp
  ${HEADER_LIST}
)

message(STATUS ${HEADER_LIST})

# source files compiled for the library can include headers
# without usinf the full path
target_include_directories(RTSP PRIVATE ${HEADER_PATH}/rtsp)

# external users of this library include headers relative to
# the based include directory
target_include_directories(RTSP PUBLIC ${HEADER_PATH})

target_link_libraries(RTSP PUBLIC
  ${Boost_LIBRARIES}
  fmt::fmt
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
