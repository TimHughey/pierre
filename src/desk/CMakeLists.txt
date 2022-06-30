#
# Desk
#

set(__target desk)

set(HEADER_PATH "${pierre_SOURCE_DIR}/include/")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH}/${__target}/**/*.h*")

# set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
# set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
# set(Boost_USE_MULTITHREADED      ON)
# find_package(Boost 1.78.0 COMPONENTS system)

add_library(${__target} 
  # public API for (light)desk 
  desk.cpp

  # FX
  fx/fx.cpp fx/majorpeak.cpp
  # fx/fx.cpp fx/leave.cpp fx/majorpeak.cpp
  
  # (head)units
  unit/pinspot.cpp
  ${HEADER_LIST})

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE ${HEADER_PATH}/${_target})

# external users of this library include headers relative to
# the basedinclude directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PUBLIC
  base fader packet
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
