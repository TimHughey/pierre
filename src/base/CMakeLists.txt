#
# base
#

set(__target base)

set(HEADER_PATH                         ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL                   ${HEADER_PATH}/${__target})
set(HEADER_PATH_PAIR                    ${HEADER_PATH_LOCAL}/pair)
set(HEADER_PATH_AES                     ${HEADER_PATH_LOCAL}/aes)
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/**/*.h*")

find_package(Boost 1.79   QUIET COMPONENTS REQUIRED system)
find_package(fmt                           REQUIRED)
find_package(GCRYPT                        REQUIRED)
find_package(Sodium                        REQUIRED)
find_package(Threads                       REQUIRED)

add_subdirectory(pair)

add_library(${__target} SHARED
  # io (asio) helpers
  io.cpp

  # logging (threaded)
  logger.cpp

  # common thread typedefs and helpers
  threads.cpp

  # aes
  aes/ctx.cpp

  # host info
  host.cpp

  # airplay feature bits
  features.cpp

  # generic vector of uint8_t (as a base class or standalone)
  uint8v.cpp

  # RTSP message support
  content.cpp headers.cpp resp_code.cpp

  # colors (used by multiple libraries)
  color.cpp

  # time / frame / rtp time keeping anchor and clock info
  anchor_data.cpp clock_info.cpp pet.cpp render.cpp
  
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
  Threads::Threads 
  ${Boost_LIBRARIES}
  libpairing
  sodium
  gcrypt
  fmt::fmt
)
