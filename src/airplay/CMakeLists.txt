message(STATUS "diving into ${CMAKE_CURRENT_LIST_DIR}")

include(${pierre_SOURCE_DIR}/cmake/FileEmbed.cmake)

FileEmbedSetup()
FileEmbedAdd(${CMAKE_CURRENT_LIST_DIR}/plist_get_info_response.xml)

file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${pierre_SOURCE_DIR}/include/pierre/airplay/*.h*")
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
add_library(AirPlay ${C_FILES} ${HEADER_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(AirPlay PUBLIC 
  "${pierre_SOURCE_DIR}/include/pierre/airplay"
  "${CMAKE_BINARY_DIR}/file_embed"
)

# This depends on (header only) boost
target_link_libraries(AirPlay PRIVATE Sodium GCRYPT OpenSSL file_embed)

# All users of this library will need at least C++11
# target_compile_features(modern_library PUBLIC cxx_std_11)

# IDEs should put the headers in a nice place
source_group(
  TREE "${PROJECT_SOURCE_DIR}/include/pierre/airplay"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
