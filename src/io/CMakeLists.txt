#
# asio wrapper
#

set(THREADS_PREFER_PTHREAD_FLAG true)

set(__target  io)

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/${__target}")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${HEADER_PATH_LOCAL}/*.h*")

set(Boost_USE_DEBUG_LIBS             ON)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS           ON)  # only find release libs
set(Boost_USE_MULTITHREADED          ON)
set(Boost_THREADAPI             pthread)

find_package(Boost 1.78 COMPONENTS system REQUIRED)
find_library(rt         NAMES rt          REQUIRED)

add_library(${__target} INTERFACE ${HEADER_LIST})

# all includes are relative to base includes
target_include_directories(${__target} PUBLIC INTERFACE ${HEADER_PATH})
target_link_libraries(${__target} INTERFACE 
  ${Boost_LIBRARIES}
  ArduinoJson
  rt
)
