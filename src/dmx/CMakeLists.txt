#
# DMX
#

include(FetchContent)
FetchContent_Declare(ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG        v6.19.3
)
FetchContent_MakeAvailable(ArduinoJson)

set(HEADER_PATH "${pierre_SOURCE_DIR}/include/")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH}/dmx/*.h*")

set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
find_package(Boost 1.78.0 COMPONENTS system)

add_library(DMX net.cpp packet.cpp render.cpp ${HEADER_LIST})

# NOTE: libary compile and external usage must include
# the path dmx in #includes [FUTURE:  fix this at some point]
target_include_directories(DMX PUBLIC ${HEADER_PATH})

# don't expose boost beyond this library
target_include_directories(DMX PRIVATE ${Boost_INCLUDE_DIRS} )

target_link_libraries(DMX PUBLIC
  ArduinoJson
  ${Boost_LIBRARIES}
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
