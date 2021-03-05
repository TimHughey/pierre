#
# Core Application
#

add_executable(pierre main.cpp)

add_compile_options(pierre -Wall -Wextra -pedantic -Werror -O3)
target_compile_features(pierre PRIVATE cxx_std_14)
target_link_libraries(pierre PRIVATE pierre_lib ALSA::ALSA)
