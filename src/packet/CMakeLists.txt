#
# PACKET
#

set(THREADS_PREFER_PTHREAD_FLAG true)

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE
  HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH}/packet/*.h*"
)

find_package(fmt               REQUIRED)
find_library(plist NAMES plist REQUIRED)

add_library(PACKET
  headers.cpp content.cpp
  aplist.cpp resp_code.cpp
  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without usinf the full path
target_include_directories(PACKET PRIVATE ${HEADER_PATH}/packet)

# external users of this library include headers relative to
# the based include directory
target_include_directories(PACKET PUBLIC ${HEADER_PATH})

target_link_libraries(PACKET PUBLIC
  ${PLIST_GET_INFO_BIN}
  ${plist}
  fmt::fmt
)


