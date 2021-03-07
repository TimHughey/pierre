#
# Base Source Directory
#

find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

set(Boost_USE_STATIC_LIBS        ON)  # only find static libs
set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
find_package(Boost 1.67.0 COMPONENTS date_time timer)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  # target_link_libraries(foo ${Boost_LIBRARIES})
endif()


add_subdirectory(audio)
add_subdirectory(misc)
