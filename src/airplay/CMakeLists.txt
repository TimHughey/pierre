#
# airplay
#

add_subdirectory(aes)

set(THREADS_PREFER_PTHREAD_FLAG true)

set(__target  airplay)

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/${__target}")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.h*"
  "${HEADER_PATH_LOCAL}/common/*.h*"
  "${HEADER_PATH_LOCAL}/reply/*.h*"
  "${HEADER_PATH_LOCAL}/server/*.h*"
  "${HEADER_PATH_LOCAL}/session/*.h*"
)

add_library(${__target}
  # public API and overall controller
  airplay.cpp controller.cpp
  
  # connection info
  conn_info/conn_info.cpp conn_info/stream.cpp conn_info/stream_info.cpp

  # reply
  reply/cmd.cpp reply/factory.cpp reply/fairplay.cpp reply/feedback.cpp
  reply/flush.cpp reply/info.cpp reply/options.cpp reply/pairing.cpp
  reply/parameter.cpp reply/record.cpp reply/reply.cpp reply/set_anchor.cpp
  reply/set_peers.cpp reply/set_peersx.cpp reply/setup.cpp reply/teardown.cpp
  reply/unhandled.cpp

  # servers
  server/servers.cpp server/audio.cpp server/control.cpp server/event.cpp server/rtsp.cpp

  # sessions
  session/base.cpp session/audio.cpp session/event.cpp session/rtsp.cpp
  
  ${HEADER_LIST}
)

# all includes are relative to base includes
target_include_directories(${__target} PRIVATE ${HEADER_PATH} ${HEADER_PATH_LOCAL})

# all includes are relative to base includes
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PUBLIC
  ${Boost_LIBRARIES}
  # fmt::fmt rt
  base core io mdns packet ap_aes player
)
