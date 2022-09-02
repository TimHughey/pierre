#
# RTP Time
#

set(__target  rtp_time)

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/${__target}")
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/*.h*")

add_library(${__target} SHARED 
  # generic clock info
  clock_info.cpp

  # ptp master clock
  master_clock.cpp 

  # rtp anchor
  anchor.cpp anchor/data.cpp

  # headers
  ${HEADER_LIST}
)

# all includes are relative to base includes
target_include_directories(${__target} PRIVATE ${HEADER_PATH} ${HEADER_PATH_LOCAL})

# all includes are relative to base includes
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE core io)
