#
# RTP Time
#

set(THREADS_PREFER_PTHREAD_FLAG true)

set(__target  rtp_time)

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/${__target}")
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/*.h*")

# set(Boost_USE_DEBUG_LIBS             ON)  # ignore debug libs and
# set(Boost_USE_RELEASE_LIBS           ON)  # only find release libs
# set(Boost_USE_MULTITHREADED          ON)
# set(Boost_THREADAPI             pthread)

# find_package(Boost 1.78 COMPONENTS system REQUIRED)
# find_package(fmt                          REQUIRED)
# find_library(rt         NAMES rt          REQUIRED)

add_library(${__target} SHARED 
  # ptp master clock
  clock.cpp 

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
