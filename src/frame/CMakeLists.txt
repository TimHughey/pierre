#
# Frame
#

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libavutil
)

set(__target frame)

set(HEADER_PATH                 ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL           ${HEADER_PATH}/${__target})
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS
  "${HEADER_PATH}/${__target}/**/.h*"
)

find_library(m      NAMES m REQUIRED)
find_package(Sodium         REQUIRED)
find_package(ZLIB           REQUIRED)

add_library(${__target} SHARED
  # deoder, dsp
  av.cpp dsp.cpp fft.cpp 
  
  # FFT results
  peaks.cpp peaks/peak.cpp peaks/peak_config.cpp

  # the frame (from raw to dsp complete)
  frame.cpp state.cpp

  # racks and reels of frames
  racked.cpp reel.cpp

  # master clock and anchor
  anchor.cpp master_clock.cpp

  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE 
  ${HEADER_PATH_LOCAL} ${HEADER_PATH})

# external users of this library include headers relative to
# the basedinclude directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  ${sodium_LIBRARY_RELEASE}
  PkgConfig::LIBAV
  base
  config
  io
)
