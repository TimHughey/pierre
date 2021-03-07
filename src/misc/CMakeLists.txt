#
# Misc Library
#
# <project>/src/misc

# Make an automatic library - will be static or dynamic based on user setting
add_library(pierre_misc FFT.cpp random.cpp)

if(Boost_FOUND)
  # include_directories(${Boost_INCLUDE_DIRS})
  target_link_libraries(pierre_misc PUBLIC ${Boost_LIBRARIES})
endif()

# We need this directory, and users of our library will need it too
target_include_directories(pierre_misc PUBLIC
  ${pierre_SOURCE_DIR}/include/pierre)

target_compile_options(pierre_misc PRIVATE ${CXX_FLAGS})

# All users of this library will need at least C++14
target_compile_features(pierre_misc PUBLIC cxx_std_17)
