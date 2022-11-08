#
# base
#

set(__target libpairing)

set(HEADER_PATH                         ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL                   ${HEADER_PATH}/${__target})
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/*.h*")

find_library(libplist     NAMES plist       REQUIRED)
find_package(GCRYPT                         REQUIRED)
find_package(Sodium                         REQUIRED)

add_library(${__target} SHARED
  # logging (threaded)
  pair.c pair_fruit.c pair_homekit.c pair-tlv.c 
  
  # headers
  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE
  ${HEADER_PATH_LOCAL} ${HEADER_PATH_PAIR} ${HEADER_PATH_AES}
)

# external users of this library include headers relative to
# the based include directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  ${GCRYPT}
  ${libplist}
)
