
cmake_minimum_required(VERSION 3.24.3)

set(out_dir ${CMAKE_BINARY_DIR}/generated)
set(in_dir ${CMAKE_SOURCE_DIR}/cmake/build_inject_src)
set(header_file build_inject.hpp)
set(cpp_file build_inject.cpp)

if(NOT EXISTS ${out_dir})
  file(MAKE_DIRECTORY ${out_dir})
endif()

configure_file(${in_dir}/${header_file} ${out_dir}/${header_file} COPYONLY)
configure_file(${in_dir}/${cpp_file}.in ${out_dir}/${cpp_file} @ONLY)

add_library(build_inject ${CMAKE_BINARY_DIR}/generated/build_inject.cpp)
target_include_directories(build_inject PUBLIC ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(build_inject git_inject)
