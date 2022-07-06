#
# mdns
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


add_library(mdns SHARED
  mdns.cpp callbacks.cpp
  ${HEADER_LIST}
)

# library specific files need Avahi privately
target_include_directories(mdns
  PRIVATE
  ${HEADER_PATH_LOCAL}
  ${HEADER_PATH_PAIR}
  ${AVAHI_INCLUDE_DIR})

# external users of this library include headers relative to
# the based include directory
target_include_directories(mdns PUBLIC ${HEADER_PATH} ${AVAHI_INCLUDE_DIR})

target_link_libraries(mdns PRIVATE core ${AVAHI_LIBRARIES})

target_compile_definitions(mdns PRIVATE ${AVAHI_DEFINITIONS})