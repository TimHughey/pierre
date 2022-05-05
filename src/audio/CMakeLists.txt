#
# Audio
#

set(HEADER_PATH "${pierre_SOURCE_DIR}/include/")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH}/audio/*.h*")


set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
find_package(Boost 1.78.0 REQUIRED COMPONENTS system)

add_library(Audio dsp.cpp fft.cpp net.cpp peaks.cpp ${HEADER_LIST})

# source files compiled for the library can include headers
# without usinf the full path
target_include_directories(Audio PRIVATE ${HEADER_PATH}/audio)

# external users of this library include headers relative to
# the based include directory
target_include_directories(Audio PUBLIC ${HEADER_PATH})

# don't expose boost beyond this library
target_include_directories(Audio PRIVATE ${Boost_INCLUDE_DIRS} )

target_link_libraries(Audio PUBLIC
  ${Boost_LIBRARIES}
  core
  Misc
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
