#
# Frame
#

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libswresample
    libswscale
    libavutil
)

set(__target frame)

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE
  HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH}/${__target}/*.h*"
)

find_library(m      NAMES m REQUIRED)
find_package(Sodium         REQUIRED)
find_package(ZLIB           REQUIRED)

add_library(${__target} SHARED
  # dsp
  fft.cpp peaks.cpp

  # the frame
  frame.cpp

  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE ${HEADER_PATH}/${__target})

# external users of this library include headers relative to
# the basedinclude directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  PkgConfig::LIBAV
  ${sodium_LIBRARY_RELEASE}
  base
  rtp_time
)



