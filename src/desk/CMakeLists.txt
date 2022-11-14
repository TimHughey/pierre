#
# Desk
#

FetchContent_MakeAvailable(ArduinoJson)

set(__target desk)

set(HEADER_PATH                                   ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL                             ${HEADER_PATH}/${__target})
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS   "${HEADER_PATH_LOCAL}/*.h*")

add_library(${__target} SHARED
  # public API for (light)desk and headunits
  desk.cpp units.cpp

  # desk sessions (control and data)
  session/ctrl.cpp session/data.cpp

  # headunits
  unit/pinspot.cpp

  # FX
  fx/fx.cpp fx/majorpeak.cpp fx/leave.cpp fx/colorbars.cpp fx/silence.cpp

  # stats
  stats/stats.cpp

  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE ${HEADER_PATH_LOCAL} ${HEADER_PATH})

# external users of this library include headers relative to
# the base include directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE
  InfluxData::InfluxDB
  base config frame io fader mdns
)

target_link_libraries(${__target} PUBLIC
  ArduinoJson
)
