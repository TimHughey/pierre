#
# mDNS
#

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL ${HEADER_PATH}/mdns)
SET(HEADER_PATH_PAIR ${HEADER_PATH_LOCAL}/pair)
file(GLOB
  HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.hpp"
  "${HEADER_PATH_LOCAL}/service/*.hpp"
  "${HEADER_PATH_PAIR}/pair/*.h"
  )

set(THREADS_PREFER_PTHREAD_FLAG true)

find_package(Avahi    REQUIRED)
find_package(GCRYPT   REQUIRED)
find_package(Sodium   REQUIRED)
find_package(OpenSSL  REQUIRED)
find_package(fmt      REQUIRED)

find_library(libplist NAMES plist REQUIRED)

add_library(mDNS 
  mdns.cpp callbacks.cpp
  service.cpp service/airplay.cpp service/raop.cpp
  pair/pair_fruit.c pair/pair_homekit.c pair/pair-tlv.c pair/pair.c
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
  sodium ssl gcrypt
  ${AVAHI_LIBRARIES} ${libplist}
#  ${AVAHI_LIBRARIES} ${GCRYPT_LIBRARIES} ${Sodium_LIBRARIES} ${libplist}
  fmt::fmt
)

target_compile_definitions(mDNS PUBLIC ${AVAHI_DEFINITIONS})