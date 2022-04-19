#
# RTSP
#

set(THREADS_PREFER_PTHREAD_FLAG true)

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE
  HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH}/rtsp/*.h*"
)

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
find_package(Boost 1.78.0 COMPONENTS context thread coroutine system REQUIRED)

find_package(fmt                REQUIRED)
find_library(plist  NAMES plist REQUIRED)
find_library(rt     NAMES rt    REQUIRED)

add_library(RTSP
  rtsp.cpp headers.cpp content.cpp aes_ctx.cpp nptp.cpp
  request.cpp  
  reply.cpp aplist.cpp
  reply/factory.cpp reply/fairplay.cpp reply/options.cpp
  reply/info.cpp reply/pairing.cpp reply/parameter.cpp
  reply/setup.cpp reply/record.cpp reply/set_peers.cpp
  reply/feedback.cpp
  "reply/command.cpp"
  ${HEADER_LIST}
)

# embed statis plists
set(PLIST_EMBED ${CMAKE_CURRENT_SOURCE_DIR}/plist_embed)
set(PLIST_GET_INFO get_info_resp.plist)
set(PLIST_GET_INFO_BIN ${CMAKE_CURRENT_BINARY_DIR}/${PLIST_GET_INFO}.bin)

add_custom_command(TARGET 
  RTSP PRE_BUILD
  COMMAND ld -r -b binary -o  ${PLIST_GET_INFO_BIN} ${PLIST_GET_INFO}
  BYPRODUCTS ${PLIST_GET_INFO_BIN}
  WORKING_DIRECTORY ${PLIST_EMBED}
  COMMENT "Generating binary version of ${PLIST_GET_INFO}"
  VERBATIM
)

# source files compiled for the library can include headers
# without usinf the full path
target_include_directories(RTSP PRIVATE ${HEADER_PATH}/rtsp)

# external users of this library include headers relative to
# the based include directory
target_include_directories(RTSP PUBLIC ${HEADER_PATH})

target_link_libraries(RTSP PUBLIC
  ${PLIST_GET_INFO_BIN}
  ${Boost_LIBRARIES}
  ${plist}
  Core
  mDNS
  RTP
  fmt::fmt
  rt
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
