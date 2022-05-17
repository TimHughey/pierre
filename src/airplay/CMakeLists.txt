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

set(Boost_USE_DEBUG_LIBS             ON)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS           ON)  # only find release libs
set(Boost_USE_MULTITHREADED          ON)
set(Boost_THREADAPI             pthread)

find_package(Boost 1.78 COMPONENTS system REQUIRED)
find_package(fmt                          REQUIRED)
find_library(rt         NAMES rt          REQUIRED)

add_library(${__target}
  # public API and overall controller
  airplay.cpp controller.cpp

  # clock
  clock/info.cpp clock/clock.cpp 

  # common
  common/anchor.cpp common/conn_info.cpp common/net_time.cpp 
  common/stream_info.cpp common/stream.cpp

  # reply
  reply/cmd.cpp reply/factory.cpp reply/fairplay.cpp reply/feedback.cpp
  reply/flush.cpp reply/info.cpp reply/options.cpp reply/pairing.cpp
  reply/parameter.cpp reply/record.cpp reply/reply.cpp reply/set_anchor.cpp
  reply/set_peers.cpp reply/setup.cpp reply/teardown.cpp reply/unhandled.cpp

  # servers
  server/map.cpp server/audio.cpp server/control.cpp server/event.cpp server/rtsp.cpp

  # sessions
  session/audio.cpp session/event.cpp session/rtsp.cpp
  
  ${HEADER_LIST}
)

# all includes are relative to base includes
target_include_directories(${__target} PRIVATE ${HEADER_PATH} ${HEADER_PATH_LOCAL})

# all includes are relative to base includes
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PUBLIC
  ${Boost_LIBRARIES}
  fmt::fmt
  rt
  core mdns packet ap_aes
)
