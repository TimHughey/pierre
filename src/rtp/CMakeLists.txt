#
# RTP
#

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/rtp")
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.h*"
)

message(STATUS "header list ${HEADER_LIST}")

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)

find_package(Boost 1.78 COMPONENTS system REQUIRED)
find_package(fmt                          REQUIRED)
find_library(rt       NAMES rt            REQUIRED)
find_package(OpenSSL                      REQUIRED)

add_library(RTP
  # Base
    rtp.cpp

  # Supporting data structures
    conn_info.cpp servers.cpp anchor_info.cpp stream_info.cpp

  # Keeper of server instances
    servers.cpp

  # TCP audo server and supprt
    tcp/audio/entry.cpp
    tcp/audio/server.cpp
    tcp/audio/session.cpp

  # TCP event server and support

    tcp/event/server.cpp
    tcp/event/session.cpp

  # UDP servers
    udp/control/packet.cpp
    udp/control/server.cpp
  
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
  PCM
  ssl
  nPTP RTSP PACKET
  fmt::fmt
  rt
)
