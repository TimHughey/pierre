#
# fader
#

set(__target fader)

set(HEADER_PATH                         ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL                   ${HEADER_PATH}/${__target})
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/*.h*")

add_library(${__target} 
  easings.cpp
  fader.cpp
  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(
  ${__target} PRIVATE ${HEADER_PATH_LOCAL} ${HEADER_PATH}
)

# external users of this library include headers relative to
# the based include directory
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PUBLIC base)
