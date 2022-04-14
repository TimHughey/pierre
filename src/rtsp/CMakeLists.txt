#
# RTSP
#

include(${pierre_SOURCE_DIR}/cmake/FileEmbed.cmake)
FileEmbedSetup()

set(THREADS_PREFER_PTHREAD_FLAG true)

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH}/rtsp/*.h*"
)

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
find_package(Boost 1.78.0 COMPONENTS context thread coroutine system REQUIRED)

find_package(fmt REQUIRED)
find_library(plist NAMES plist REQUIRED)

add_library(RTSP
  rtsp.cpp headers.cpp aes_ctx.cpp
  request.cpp reply.cpp
    reply/factory.cpp reply/fairplay.cpp reply/options.cpp
    reply/info.cpp reply/xml.cpp reply/pairing.cpp
  ${HEADER_LIST}
)

FileEmbedAdd(${CMAKE_CURRENT_LIST_DIR}/plist_get_info_resp.xml)

# source files compiled for the library can include headers
# without usinf the full path
target_include_directories(RTSP PRIVATE
  ${HEADER_PATH}/rtsp
  ${CMAKE_BINARY_DIR}/file_embed
)

# external users of this library include headers relative to
# the based include directory
target_include_directories(RTSP PUBLIC ${HEADER_PATH})

target_link_libraries(RTSP PUBLIC
  ${Boost_LIBRARIES}
  ${plist}
  Core
  mDNS
  fmt::fmt
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
