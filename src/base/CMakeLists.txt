#
# base
#

set(__target base)

set(HEADER_PATH                         ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL                   ${HEADER_PATH}/${__target})
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/*.h*")

set(Boost_USE_DEBUG_LIBS             ON)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS           ON)  # only find release libs
set(Boost_USE_MULTITHREADED          ON)
set(Boost_THREADAPI             pthread)

find_package(Boost 1.79 COMPONENTS system REQUIRED)
find_library(rt         NAMES rt          REQUIRED)

find_package(fmt REQUIRED)

add_library(${__target} SHARED
  # logging
  log.cpp 

  # common thread typedefs and helpers
  threads.cpp
  
  # connection shared key
  shk.cpp

  # generic vector of uint8_t (as a base class or standalone)
  uint8v.cpp

  # RTSP message support
  content.cpp headers.cpp resp_code.cpp

  # colors (used by multiple libraries)
  color.cpp

  # time / frame / rtp time keeping anchor and clock info
  anchor_data.cpp clock_info.cpp pet.cpp
  
  # headers
  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE
  ${HEADER_PATH_LOCAL} ${HEADER_PATH_PAIR}
)

# external users of this library include headers relative to
# the based include directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PUBLIC 
  ${Boost_LIBRARIES}
  fmt::fmt
)
