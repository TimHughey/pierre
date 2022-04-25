#
# RTP
#

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/rtp")
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.h*"
)

message(STATUS "header list ${HEADER_LIST}")

set(Boost_USE_DEBUG_LIBS         ON)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_DEBUG                  OFF)
set(Boost_THREADAPI              pthread)

find_package(Boost 1.78 COMPONENTS thread system REQUIRED)
find_package(fmt                                 REQUIRED)
find_library(rt       NAMES rt                   REQUIRED)
find_package(OpenSSL                             REQUIRED)

add_library(RTP
  conn_info.cpp rtp.cpp stream.cpp anchor_info.cpp
  audio/buffered/server.cpp audio/buffered/session.cpp
  control/datagram.cpp timing/datagram.cpp
  event/server.cpp event/session.cpp
  ${HEADER_LIST}
)

# all includes are relative to base includes
target_include_directories(RTP PRIVATE ${HEADER_PATH} ${HEADER_PATH_LOCAL})

# all includes are relative to base includes
target_include_directories(RTP PUBLIC ${HEADER_PATH})

target_link_libraries(RTP PUBLIC
  ${Boost_LIBRARIES}
  # ${plist}
  # Core
  # mDNS
  ssl
  nPTP RTSP PACKET
  fmt::fmt
  rt
)
