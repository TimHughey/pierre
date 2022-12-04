#
# Desk
#

set(__target desk)

set(HEADER_PATH                                   ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL                             ${HEADER_PATH}/${__target})
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS   "${HEADER_PATH_LOCAL}/*.h*")

add_library(${__target} SHARED
  # public API
  desk.cpp 

  # color
  color.cpp

  # desk DMX control session
  ctrl.cpp

  # headunits
  unit/names.cpp
  unit/pinspot.cpp

  # FX
  fx/fx.cpp
  fx/leave.cpp
  fx/majorpeak.cpp
  fx/silence.cpp

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
  base config frame io fader mdns stats
)

target_link_libraries(${__target} PUBLIC
  ArduinoJson
)
