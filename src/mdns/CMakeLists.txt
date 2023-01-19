#
# mdns
#

set(__target mdns)
set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL ${HEADER_PATH}/${__target})

file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/*.hpp")

find_package(Avahi                          REQUIRED)
find_library(libUUID      NAMES uuid        REQUIRED)

add_library(${__target} SHARED 
  # features bits (for mdns advert and airplay rtsp)
  features.cpp

  # mdns functionality specific to this app
  mdns.cpp 

  # mdns ctx and client provider
  mdns_ctx.cpp

  # service for airplay
  service.cpp

  # consolidated view of an available service
  zservice.cpp

  # this libraries headers
  ${HEADER_LIST}
)

# library specific files need Avahi privately
target_include_directories(${__target} PRIVATE 
  ${HEADER_PATH_LOCAL} ${AVAHI_INCLUDE_DIR}
)

# external users of this library include headers relative to
# the based include directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE 
  ${AVAHI_LIBRARIES}
  ${libUUID}
  io
  lcs
  pair
)

target_compile_definitions(${__target} PRIVATE ${AVAHI_DEFINITIONS})