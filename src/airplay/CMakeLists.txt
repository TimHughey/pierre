#
# airplay
#

add_subdirectory(ple)

set(__target  airplay)

set(HEADER_PATH                           "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL                     "${HEADER_PATH}/${__target}")
set(HEADER_PATH_EMBED                     "${HEADER_PATH_LOCAL}/embed")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS   "${HEADER_PATH_LOCAL}/**/*.h*")

find_library(libplist     NAMES plist      REQUIRED)

add_library(${__target} SHARED
  # public API
  airplay.cpp

  # apple property lists
  aplist/aplist.cpp

  # message cipher support
  aes_ctx.cpp

  # rtsp message support
  content.cpp
  headers.cpp
  resp_code.cpp

  # embedded plists (via ld text file to binary, see ple/CMakeLists.txt)
  embed/embed.cpp

  # reply
  reply/factory.cpp
  reply/fairplay.cpp
  reply/flush.cpp
  reply/info.cpp
  reply/reply.cpp
  reply/set_anchor.cpp
  reply/set_peers.cpp 
  reply/set_peersx.cpp
  reply/setup.cpp 
  reply/teardown.cpp

  # rtsp session context, message / worker servers and message session
  rtsp.cpp 
  rtsp/session.cpp 
  rtsp/audio.cpp 
  rtsp/ctx.cpp

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
  pair
  ple
  base config frame io mdns stats
)

target_link_libraries(${__target} PUBLIC 
  mdns
)
