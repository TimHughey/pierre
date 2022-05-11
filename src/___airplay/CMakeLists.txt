#
# Airplay
#

include(${pierre_SOURCE_DIR}/cmake/FileEmbed.cmake)
FileEmbedSetup()

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH}/___airplay/*.h*")
list(APPEND HEADER_LIST "${CMAKE_BINARY_DIR}/file_embed")

set(THREADS_PREFER_PTHREAD_FLAG  true)
find_package(Threads             REQUIRED)
find_package(GCRYPT              REQUIRED)
find_package(OpenSSL             REQUIRED)
find_package(Sodium              REQUIRED)
find_package(libconfig           REQUIRED)
pkg_check_modules(libdaemon      REQUIRED libdaemon>=0.14)

add_compile_definitions(CONFIG_GCRYPT SYSCONFDIR="${pierre_SOURCE_DIR}/config")

file(GLOB C_FILES *.c)
add_library(Airplay ${C_FILES} ${HEADER_LIST})

FileEmbedAdd(${CMAKE_CURRENT_LIST_DIR}/plist_get_info_response.xml)

# source files compiled for the library can include headers
# without usinf the full path
target_include_directories(Airplay PRIVATE 
  ${HEADER_PATH}/airplay
  ${CMAKE_BINARY_DIR}/file_embed)

# external users of this library include headers relative to
# the based include directory
target_include_directories(Airplay PUBLIC ${HEADER_PATH})

# This depends on (header only) boost
target_link_libraries(Airplay PRIVATE Sodium GCRYPT OpenSSL file_embed)

# IDEs should put the headers in a nice place
# source_group(
#   TREE "${PROJECT_SOURCE_DIR}/include/airplay"
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
