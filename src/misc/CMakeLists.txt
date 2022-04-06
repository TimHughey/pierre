#
# MISC
#

set(HEADER_PATH "${pierre_SOURCE_DIR}/include")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH}/misc/*.h*")

add_library(Misc elapsed.cpp random.cpp ${HEADER_LIST})

# source files compiled for the library can include headers
# without using the full path
target_include_directories(Misc PRIVATE "${HEADER_PATH}/misc")

# external users of this library include headers relative to
# the based include directory
target_include_directories(Misc PUBLIC ${HEADER_PATH})

# # IDEs should put the headers in a nice place
# source_group(
#   TREE ${HEADER_PATH}
#   PREFIX "Header Files"
#   FILES ${HEADER_LIST})
