# Note that headers are optional, and do not affect add_library, but they will not
# show up in IDEs unless they are listed in add_library.

set(HEADER_LIST "${pierre_SOURCE_DIR}/include/pierre/audiotx.hpp")

# Make an automatic library - will be static or dynamic based on user setting
add_library(pierre_lib audiotx.cpp ${HEADER_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(pierre_lib PUBLIC ../include)

# This depends on ALSA
target_link_libraries(pierre_lib PRIVATE ALSA::ALSA)
target_link_libraries(pierre_lib PRIVATE Threads::Threads)

# All users of this library will need at least C++14
target_compile_features(pierre_lib PUBLIC cxx_std_14)

# IDEs should put the headers in a nice place
source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
