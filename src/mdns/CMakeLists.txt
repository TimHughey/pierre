#
# mDNS
#

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL ${HEADER_PATH}/mdns)

file(GLOB
  HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.hpp"
  # "${HEADER_PATH_LOCAL}/service/*.hpp"
  "${HEADER_PATH_PAIR}/pair/*.h"
  )

set(THREADS_PREFER_PTHREAD_FLAG true)

find_package(Avahi    REQUIRED)
find_package(fmt      REQUIRED)



add_library(mDNS 
  mdns.cpp callbacks.cpp
  ${HEADER_LIST}
)

# library specific files need Avahi privately
target_include_directories(mDNS
  PRIVATE
  ${HEADER_PATH_LOCAL}
  ${HEADER_PATH_PAIR}
  ${AVAHI_INCLUDE_DIR})

# external users of this library include headers relative to
# the based include directory
target_include_directories(mDNS PUBLIC ${HEADER_PATH} ${AVAHI_INCLUDE_DIR})

target_link_libraries(mDNS PUBLIC
  Core
  ${AVAHI_LIBRARIES} 
  fmt::fmt
)

target_compile_definitions(mDNS PUBLIC ${AVAHI_DEFINITIONS})