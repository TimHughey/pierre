#
# Embedded plist
#

set(__target ple)

set(HEADER_PATH                         ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL                   ${HEADER_PATH}/airplay/${__target})
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/*.h*")

add_library(${__target} 
  embed.cpp
  ${HEADER_LIST}
)

# embed static plists
set(PLIST_EMBED ${CMAKE_CURRENT_SOURCE_DIR})
set(PLIST_GET_INFO get_info_resp.plist)
set(PLIST_GET_INFO_BIN ${CMAKE_CURRENT_BINARY_DIR}/${PLIST_GET_INFO}.bin)

add_custom_command(TARGET 
  ${__target} PRE_BUILD
  COMMAND ld -r -b binary -o  ${PLIST_GET_INFO_BIN} ${PLIST_GET_INFO}
  BYPRODUCTS ${PLIST_GET_INFO_BIN}
  WORKING_DIRECTORY ${PLIST_EMBED}
  COMMENT "Generating binary version of ${PLIST_GET_INFO}"
  VERBATIM
)

target_include_directories(${__target} PUBLIC ${HEADER_PATH_LOCAL})
target_link_libraries(${__target} PUBLIC ${PLIST_GET_INFO_BIN})


