#
# Config Fallback
#

set(__target            fallback)
add_library(${__target} STATIC fallback.cpp)

# embed static plists
set(FALLBACK_JSON fallback.json)
set(FALLBACK_JSON_BIN ${CMAKE_CURRENT_BINARY_DIR}/${FALLBACK_JSON}.bin)

add_custom_command(TARGET 
  ${__target} PRE_BUILD
  COMMAND ld -r -b binary -o  ${FALLBACK_JSON_BIN} ${FALLBACK_JSON}
  BYPRODUCTS ${FALLBACK_JSON_BIN}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating binary version of ${FALLBACK_JSON}"
  VERBATIM
)

target_link_libraries(${__target} PUBLIC ${FALLBACK_JSON_BIN})


