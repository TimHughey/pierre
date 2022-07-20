#
# core
#

set(__target core)

set(HEADER_PATH               ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL         ${HEADER_PATH}/${__target})
set(HEADER_PATH_PAIR          ${HEADER_PATH_LOCAL}/pair)
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/*.h*")

# find_library(libConfig NAMES config++   REQUIRED)
find_library(libUUID   NAMES uuid       REQUIRED)
find_package(GCRYPT                     REQUIRED)
find_package(Sodium                     REQUIRED)

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
find_package(Boost 1.78 COMPONENTS program_options)

add_library(${__target} SHARED
  args.cpp host.cpp features.cpp
  status_flags.cpp service.cpp static.cpp
  pair/pair_fruit.c pair/pair_homekit.c pair/pair-tlv.c pair/pair.c
  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE 
  ${HEADER_PATH_LOCAL}
  ${HEADER_PATH_PAIR}
)

# external users of this library include headers relative to
# the based include directory
target_include_directories(${__target} PUBLIC 
  ${HEADER_PATH}
)

target_link_libraries(${__target} PRIVATE
  ${libUUID}
  ${Boost_LIBRARIES}
  sodium
  gcrypt
  base
  config
)
