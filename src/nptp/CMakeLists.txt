#
# RTSP
#

set(THREADS_PREFER_PTHREAD_FLAG true)

set(HEADER_PATH ${pierre_SOURCE_DIR}/include)
file(GLOB_RECURSE
  HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH}/nptp/*.h*"
)

add_library(nPTP
  nptp.cpp clock_info.cpp
  ${HEADER_LIST}
)

# # embed statis plists
# set(PLIST_EMBED ${CMAKE_CURRENT_SOURCE_DIR}/plist_embed)
# set(PLIST_GET_INFO get_info_resp.plist)
# set(PLIST_GET_INFO_BIN ${CMAKE_CURRENT_BINARY_DIR}/${PLIST_GET_INFO}.bin)

# add_custom_command(TARGET 
#   RTSP PRE_BUILD
#   COMMAND ld -r -b binary -o  ${PLIST_GET_INFO_BIN} ${PLIST_GET_INFO}
#   BYPRODUCTS ${PLIST_GET_INFO_BIN}
#   WORKING_DIRECTORY ${PLIST_EMBED}
#   COMMENT "Generating binary version of ${PLIST_GET_INFO}"
#   VERBATIM
# )

# source files compiled for the library can include headers
# without usinf the full path
target_include_directories(nPTP PRIVATE ${HEADER_PATH}/nptp)

# external users of this library include headers relative to
# the based include directory
target_include_directories(nPTP PUBLIC ${HEADER_PATH})

target_link_libraries(nPTP PUBLIC
  Core
)

# IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
