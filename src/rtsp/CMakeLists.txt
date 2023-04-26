#
# rtsp
#

set(__target  rtsp)

set(HEADER_PATH                           "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL                     "${HEADER_PATH}/${__target}")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS   "${HEADER_PATH_LOCAL}/**/*.h*")

find_library(libplist     NAMES plist      REQUIRED)

add_library(${__target} 
  # apple property lists
  aplist.cpp

  # message cipher support
  aes.cpp

  # rtsp message support
  headers.cpp
  resp_code.cpp

  # rtsp session context and message server/workers
  audio.cpp 
  ctx.cpp
  reply.cpp
  request.cpp
  rtsp.cpp

  # rtsp replies (complex, requires .cpp)
  replies/info.cpp
  replies/fairplay.cpp
  replies/setup.cpp
  replies/set_anchor.cpp

  # utilities / debug
  saver.cpp

  ${HEADER_LIST}
)

# all includes are relative to base includes
target_include_directories(${__target} PRIVATE
  ${HEADER_PATH_LOCAL}
  ${HEADER_PATH}
)

# all includes are relative to base includes
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  ${Boost_LIBRARIES}
  ${libplist}
  base
  desk
  frame
  mdns
  pair
)

