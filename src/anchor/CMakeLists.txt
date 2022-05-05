#
# Anchor
#

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/anchor")
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.h*"
)

message(STATUS "header list ${HEADER_LIST}")

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)

find_package(Boost 1.78 COMPONENTS system REQUIRED)
find_package(fmt                          REQUIRED)
find_library(rt       NAMES rt            REQUIRED)
# find_package(OpenSSL                      REQUIRED)

add_library(anchor
  anchor.cpp clock.cpp client.cpp
  time/net.cpp

  ${HEADER_LIST}
)

# all includes are relative to base includes
target_include_directories(anchor PRIVATE ${HEADER_PATH} ${HEADER_PATH_LOCAL})

# all includes are relative to base includes
target_include_directories(anchor PUBLIC ${HEADER_PATH})

target_link_libraries(anchor PUBLIC
  ${Boost_LIBRARIES}
  fmt::fmt
  core
  packet
)
