#
# Audio Library
#
# <project>/src/audio
#

# Note that headers are optional, and do not affect add_library, but they will not
# show up in IDEs unless they are listed in add_library.

set(HEADER_LIST "${pierre_SOURCE_DIR}/include/pierre/audio/audiotx.hpp")

# Make an automatic library - will be static or dynamic based on user setting
add_library(pierre_audio audiotx.cpp ${HEADER_LIST})

if(Boost_FOUND)
  # include_directories(${Boost_INCLUDE_DIRS})
  target_link_libraries(pierre_audio PUBLIC ${Boost_LIBRARIES})
endif()

# We need this directory, and users of our library will need it too
target_include_directories(pierre_audio PUBLIC ${pierre_SOURCE_DIR}/include/pierre)

# This depends on ALSA
target_link_libraries(pierre_audio PRIVATE ALSA::ALSA)
target_link_libraries(pierre_audio PRIVATE Threads::Threads)

target_compile_options(pierre_audio PRIVATE ${CXX_FLAGS})

# All users of this library will need at least C++17
target_compile_features(pierre_audio PUBLIC cxx_std_17)

# IDEs should put the headers in a nice place
source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
