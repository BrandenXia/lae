cmake_minimum_required(VERSION 3.20)

project(le_package_consumer LANGUAGES C CXX)

find_package(le 0.14 CONFIG REQUIRED)

add_executable(le-package-consumer main.c)
target_compile_features(le-package-consumer PRIVATE c_std_11)
target_link_libraries(le-package-consumer PRIVATE le::runtime)

enable_testing()
add_test(NAME packaged-runtime COMMAND le-package-consumer)
