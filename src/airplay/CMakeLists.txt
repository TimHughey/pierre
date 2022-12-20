#
# airplay
#

set(__target  airplay)

set(HEADER_PATH                           "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL                     "${HEADER_PATH}/${__target}")
set(HEADER_PATH_EMBED                     "${HEADER_PATH_LOCAL}/embed")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS   "${HEADER_PATH_LOCAL}/**/*.h*")

find_library(libplist     NAMES plist      REQUIRED)

add_library(${__target} SHARED
  # apple property lists
  aplist/aplist.cpp

  # message cipher support
  aes_ctx.cpp

  # rtsp message support
  content.cpp
  headers.cpp
  resp_code.cpp

  # rtsp session context and message server/workers
  rtsp.cpp 
  rtsp/audio.cpp 
  rtsp/ctx.cpp
  rtsp/reply.cpp
  rtsp/request.cpp
  rtsp/session.cpp

  # rtsp replies (complex, requires .cpp)
  rtsp/replies/info.cpp
  rtsp/replies/fairplay.cpp
  rtsp/replies/setup.cpp
  rtsp/replies/set_anchor.cpp

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
  base
  config
  frame
  io
  mdns
  pair
  stats
)

# target_link_libraries(${__target} PUBLIC 
#   mdns
# )
