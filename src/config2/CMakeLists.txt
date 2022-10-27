#
# config2
#

set(__target config2)

set(HEADER_PATH               ${pierre_SOURCE_DIR}/include)
set(HEADER_PATH_LOCAL         ${HEADER_PATH}/${__target})

file(GLOB_RECURSE HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.h*"
)

add_library(${__target} SHARED 
  config2.cpp 
  ${HEADER_LIST}
)

# source files compiled for the library can include headers
# without using the full path
target_include_directories(${__target} PRIVATE 
  ${HEADER_PATH_LOCAL}
)

target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PRIVATE base stdc++fs)
target_link_libraries(${__target} PUBLIC 
  tomlplusplus::tomlplusplus base
)
