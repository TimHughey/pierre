#
# airplay
#

add_subdirectory(ple)

set(__target  airplay)

set(HEADER_PATH                           "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL                     "${HEADER_PATH}/${__target}")
set(HEADER_PATH_EMBED                     "${HEADER_PATH_LOCAL}/embed")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS  
    "${HEADER_PATH_LOCAL}/aplist/*.h*"
    "${HEADER_PATH_LOCAL}/common/*.h*"
    "${HEADER_PATH_LOCAL}/embed/*.h*"
    "${HEADER_PATH_LOCAL}/reply/*.h*"
    "${HEADER_PATH_LOCAL}/server/*.h*"
    "${HEADER_PATH_LOCAL}/session/*.h*"
)

find_library(libplist     NAMES plist      REQUIRED)

add_library(${__target} SHARED
  # public API
  airplay.cpp

  # apple property lists
  aplist/aplist.cpp

  # connection info
  conn_info/conn_info.cpp conn_info/stream.cpp conn_info/stream_info.cpp

  # embedded plists (via ld text file to binary, see ple/CMakeLists.txt)
  embed/embed.cpp

  # reply
  reply/cmd.cpp reply/factory.cpp reply/fairplay.cpp reply/feedback.cpp
  reply/flush.cpp reply/info.cpp reply/options.cpp reply/pairing.cpp
  reply/parameter.cpp reply/record.cpp reply/reply.cpp reply/set_anchor.cpp
  reply/set_peers.cpp reply/set_peersx.cpp reply/setup.cpp reply/teardown.cpp
  reply/unhandled.cpp

  # servers
  server/servers.cpp server/audio.cpp  server/rtsp.cpp

  # sessions
  session/audio.cpp session/rtsp.cpp

  ${HEADER_LIST}
)

# all includes are relative to base includes
target_include_directories(${__target} PRIVATE
  ${HEADER_PATH_EMBED} ${HEADER_PATH_LOCAL} ${HEADER_PATH}
)

# all includes are relative to base includes
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  ${Boost_LIBRARIES}
  ${libplist}
  ple
  base config frame io mdns
)

target_link_libraries(${__target} PUBLIC mdns)
