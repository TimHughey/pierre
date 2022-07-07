#
# Digital Signal Processing
#

set(__target dsp)

set(THREADS_PREFER_PTHREAD_FLAG true)

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE
  HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH}/${__target}/*.h*"
)

add_library(${__target} SHARED
  fft.cpp peaks.cpp ${HEADER_LIST}
)
 
# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE ${HEADER_PATH}/player)

# external users of this library include headers relative to
# the basedinclude directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PUBLIC
  base
  rtp_time
)


