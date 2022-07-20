#
# Embedded Default Config
#

set(__target cfg_fallback)

# no headers, only a single file and binary file via ld
# must be separate library consisting of only the ld created binary file
# to prevent linker relocation errors

add_library(${__target} STATIC cfg_fallback.cpp)

# embed static plists
set(CFG_EMBED ${CMAKE_CURRENT_SOURCE_DIR})
set(CFG_FALLBACK_JSON cfg_fallback.json)
set(CFG_FALLBACK_JSON_BIN ${CMAKE_CURRENT_BINARY_DIR}/${CFG_FALLBACK_JSON}.bin)

add_custom_command(TARGET 
  ${__target} PRE_BUILD
  COMMAND ld -r -b binary -o  ${CFG_FALLBACK_JSON_BIN} ${CFG_FALLBACK_JSON}
  BYPRODUCTS ${CFG_FALLBACK_JSON_BIN}
  WORKING_DIRECTORY ${CFG_EMBED}
  COMMENT "Generating binary version of ${CFG_FALLBACK_JSON}"
  VERBATIM
)

target_link_libraries(${__target} PUBLIC ${CFG_FALLBACK_JSON_BIN})


