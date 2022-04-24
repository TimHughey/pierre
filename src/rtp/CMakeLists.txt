#
# RTP
#

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/rtp")
file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.h*"
)

message(STATUS "header list ${HEADER_LIST}")

set(Boost_USE_DEBUG_LIBS         ON)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_DEBUG                  OFF)
set(Boost_THREADAPI              pthread)
find_package(Boost 1.78
  COMPONENTS
    context thread coroutine system
  REQUIRED
)

find_package(fmt                  REQUIRED)
# find_library(plist  NAMES plist REQUIRED)
find_library(rt       NAMES rt    REQUIRED)
find_package(OpenSSL              REQUIRED)

add_library(RTP
  conn_info.cpp rtp.cpp stream.cpp anchor_info.cpp
  event.cpp control.cpp buffered.cpp
  ${HEADER_LIST}
)

# all includes are relative to base includes
target_include_directories(RTP PUBLIC ${HEADER_PATH})

target_link_libraries(RTP PUBLIC
  ${Boost_LIBRARIES}
  # ${plist}
  # Core
  # mDNS
  ssl
  nPTP
  RTSP
  fmt::fmt
  rt
)


# FUNCTION(ADD_RESOURCES out_var)
#   SET(result)
#   FOREACH(in_f ${ARGN})
#     FILE(RELATIVE_PATH src_f ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${in_f})
#     SET(out_f "${PROJECT_BINARY_DIR}/${in_f}.o")
#     ADD_CUSTOM_COMMAND(OUTPUT ${out_f}
#       COMMAND ld -r -b binary -o ${out_f} ${src_f}
#       DEPENDS ${in_f}
#       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#       COMMENT "Building GLSL object ${out_f}"
#       VERBATIM
#       )
#     LIST(APPEND result ${out_f})
#   ENDFOREACH()
#   SET(${out_var} "${result}" PARENT_SCOPE)
# ENDFUNCTION()
# # A simple program that uses resources
# ADD_RESOURCES(someResources test.txt)
# ADD_EXECUTABLE(main.cpp ${someResources})

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
