cmake_minimum_required(VERSION 3.20)

project(lae VERSION 0.1.0 LANGUAGES C CXX)

option(LAE_BUILD_CLI "Build the le-cli demonstration program" ON)
option(LAE_BUILD_TESTS "Build the test suite" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(UTF8PROC REQUIRED IMPORTED_TARGET libutf8proc)

add_library(le_runtime
    runtime/abi/api.cpp
    runtime/core/pipeline.cpp
    runtime/core/text.cpp
)
add_library(le::runtime ALIAS le_runtime)

target_compile_features(le_runtime PRIVATE cxx_std_20)
target_compile_definitions(le_runtime PRIVATE LE_BUILDING_RUNTIME=1)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(le_runtime PUBLIC LE_SHARED=1)
endif()
target_include_directories(le_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
target_link_libraries(le_runtime PRIVATE PkgConfig::UTF8PROC)
set_target_properties(le_runtime PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

if(LAE_BUILD_CLI)
    add_executable(le-cli tools/cli/main.c)
    target_compile_features(le-cli PRIVATE c_std_11)
    target_link_libraries(le-cli PRIVATE le::runtime)
endif()

if(LAE_BUILD_TESTS)
    enable_testing()

    add_executable(le-runtime-tests tests/runtime_tests.cpp)
    target_compile_features(le-runtime-tests PRIVATE cxx_std_20)
    target_link_libraries(le-runtime-tests PRIVATE le::runtime)
    add_test(NAME runtime COMMAND le-runtime-tests)

    add_executable(le-core-text-tests tests/core_text_tests.cpp runtime/core/text.cpp)
    target_compile_features(le-core-text-tests PRIVATE cxx_std_20)
    target_include_directories(le-core-text-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
    target_link_libraries(le-core-text-tests PRIVATE PkgConfig::UTF8PROC)
    add_test(NAME core-text COMMAND le-core-text-tests)

    add_executable(le-c11-abi-test tests/c_abi_test.c)
    set_target_properties(le-c11-abi-test PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
    target_link_libraries(le-c11-abi-test PRIVATE le::runtime)
    add_test(NAME c11-abi COMMAND le-c11-abi-test)

    add_executable(le-c17-abi-test tests/c_abi_test.c)
    set_target_properties(le-c17-abi-test PROPERTIES C_STANDARD 17 C_EXTENSIONS OFF)
    target_link_libraries(le-c17-abi-test PRIVATE le::runtime)
    add_test(NAME c17-abi COMMAND le-c17-abi-test)

    add_executable(le-cpp-abi-test tests/cpp_abi_test.cpp)
    target_compile_features(le-cpp-abi-test PRIVATE cxx_std_20)
    target_link_libraries(le-cpp-abi-test PRIVATE le::runtime)
    add_test(NAME cpp-abi COMMAND le-cpp-abi-test)
endif()

include(GNUInstallDirs)
install(TARGETS le_runtime
    EXPORT leTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
