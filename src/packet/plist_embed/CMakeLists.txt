#
# Embedded plist
#

set(__target embed_plist)

add_library(${__target} embed.cpp)

# embed statis plists
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

target_link_libraries(${__target} PUBLIC ${PLIST_GET_INFO_BIN})


