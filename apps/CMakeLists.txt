add_executable(pierre main.cpp)

target_compile_features(pierre PRIVATE cxx_std_14)
target_link_libraries(pierre PRIVATE pierre_lib ALSA::ALSA)
