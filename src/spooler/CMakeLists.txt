#
# SPOOLER
#

set(__target spooler)

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH}/${__target}/*.h*"
)

add_library(${__target} SHARED
  # spooler shared spooler object
  spooler.cpp

  # reels of frames maintained by spooler
  reel.cpp

  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE ${HEADER_PATH}/${__target})

# external users of this library include headers relative to
# the basedinclude directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  base
  frame
  io
  rtp_time
)



