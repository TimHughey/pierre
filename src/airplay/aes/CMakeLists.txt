#
# AES Ctx
#



set(THREADS_PREFER_PTHREAD_FLAG true)

set(__target ap_aes)

set(HEADER_PATH       "${pierre_SOURCE_DIR}/include")
set(HEADER_PATH_LOCAL "${HEADER_PATH}/airplay/aes")
file(GLOB HEADER_LIST CONFIGURE_DEPENDS 
  "${HEADER_PATH_LOCAL}/*.h*"
)

# set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
# set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
# set(Boost_USE_MULTITHREADED      ON)

# find_package(Boost 1.78 COMPONENTS system REQUIRED)
find_package(fmt                          REQUIRED)
find_library(rt         NAMES rt          REQUIRED)

add_library(${__target}
    aes_ctx.cpp
  ${HEADER_LIST}
)

# all includes are relative to base includes
target_include_directories(${__target} PRIVATE ${HEADER_PATH_LOCAL})

# all includes are relative to base includes
target_include_directories(${__target} PUBLIC ${HEADER_PATH})

target_link_libraries(${__target} PUBLIC
  # ${Boost_LIBRARIES}
  fmt::fmt
  rt
  core
)
