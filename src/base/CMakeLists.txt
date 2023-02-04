#
# base
#

set(__target base)

set(HEADER_PATH                         ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL                   ${HEADER_PATH}/${__target})
set(HEADER_PATH_PAIR                    ${HEADER_PATH_LOCAL}/pair)
set(HEADER_PATH_AES                     ${HEADER_PATH_LOCAL}/aes)
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/**/*.h*")

find_package(Boost 1.80   QUIET COMPONENTS REQUIRED system)
find_package(fmt                           REQUIRED)
find_package(Threads                       REQUIRED)
find_package(GCRYPT                        REQUIRED)
find_package(Sodium                        REQUIRED)

add_library(${__target} 
  # host info
  host.cpp

  # time utilities
  pet.cpp

  # thread util
  thread_util.cpp
  
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

target_link_libraries(${__target} PUBLIC 
  ${Boost_LIBRARIES}
  fmt::fmt
  gcrypt
  sodium
  Threads::Threads 
)
