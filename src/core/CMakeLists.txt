#
# Core
#

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH}/core/*.h*")
list(APPEND HEADER_LIST ${CMAKE_BINARY_DIR}/version.h)

find_library(libConfig NAMES config++ REQUIRED)
find_library(libUUID NAMES uuid REQUIRED)

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
find_package(Boost 1.78.0 COMPONENTS program_options)

find_package(fmt)

add_library(Core args.cpp config.cpp state.cpp ${HEADER_LIST})

# source files compiled for the library can include headers
# without usinf the full path
target_include_directories(Core PRIVATE ${HEADER_PATH}/core ${CMAKE_BINARY_DIR})

# external users of this library include headers relative to
# the based include directory
target_include_directories(Core PUBLIC ${HEADER_PATH})

target_link_libraries(Core PUBLIC
  ${libConfig}
  ${libUUID}
  ${Boost_LIBRARIES}
  fmt::fmt
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
