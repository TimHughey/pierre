#
# asio wrapper
#

set(__target  io)

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/${__target}")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS 
  ${HEADER_PATH_LOCAL}/io.hpp
)

add_library(${__target}  
  ${HEADER_LIST}
  log_socket.cpp
)

# all includes are relative to base includes
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PUBLIC base)
