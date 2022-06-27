#
# Lightdesk
#

set(HEADER_PATH "${pierre_SOURCE_DIR}/include/")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH}/lightdesk/*.h*")

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
find_package(Boost 1.78.0 COMPONENTS system)

add_library(Lightdesk 
  faders/easings.cpp faders/fader.cpp
  fx/fx.cpp fx/leave.cpp fx/majorpeak.cpp
  headunits/pinspot.cpp
  color.cpp
  lightdesk.cpp
  ${HEADER_LIST})

# NOTE: library compile and external usage must include
# the path Lightdesk in #includes [FUTURE:  fix this at some point]
target_include_directories(Lightdesk PUBLIC ${HEADER_PATH})

# don't expose boost beyond this library
target_include_directories(Lightdesk PRIVATE ${Boost_INCLUDE_DIRS} )

target_link_libraries(Lightdesk PUBLIC
  base core Audio DMX
  ${Boost_LIBRARIES}
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
