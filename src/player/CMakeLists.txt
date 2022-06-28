#
# PLAYER
#

set(THREADS_PREFER_PTHREAD_FLAG true)

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE
  HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH}/player/*.h*"
)

find_package(fmt            REQUIRED)
find_library(m      NAMES m REQUIRED)
find_package(Sodium         REQUIRED)
find_package(ZLIB           REQUIRED)

add_library(Player
  # orchestration
  player.cpp

  # AirPlay 2 packet handling and queuing
  reel.cpp spooler.cpp frame.cpp

  # FFT and peak detection
  fft.cpp peaks.cpp

  # DMX
  dmx/render.cpp dmx/packet.cpp


  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(Player PRIVATE ${HEADER_PATH}/player)

# external users of this library include headers relative to
# the based include directory
target_include_directories(Player PUBLIC ${HEADER_PATH})

target_link_libraries(Player PUBLIC
  PkgConfig::LIBAV
  ArduinoJson
  ${sodium_LIBRARY_RELEASE}
  base
  core
  packet
  rtp_time
  fmt::fmt
)


