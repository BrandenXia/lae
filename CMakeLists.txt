cmake_minimum_required(VERSION 3.20)

project(lae VERSION 0.10.0 LANGUAGES C CXX)

option(LAE_BUILD_CLI "Build the le-cli demonstration program" ON)
option(LAE_BUILD_TESTS "Build the test suite" ON)
set(LAE_DYNAMIC_PROVIDERS_DEFAULT OFF)
if(WIN32 OR UNIX OR APPLE)
    set(LAE_DYNAMIC_PROVIDERS_DEFAULT ON)
endif()
if(EMSCRIPTEN OR CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS)$")
    set(LAE_DYNAMIC_PROVIDERS_DEFAULT OFF)
endif()
option(LAE_ENABLE_DYNAMIC_PROVIDERS "Enable filesystem-backed provider modules"
       ${LAE_DYNAMIC_PROVIDERS_DEFAULT})

find_package(PkgConfig REQUIRED)
pkg_check_modules(UTF8PROC REQUIRED IMPORTED_TARGET libutf8proc)

add_library(le_runtime
    runtime/abi/api.cpp
    runtime/core/emphasis.cpp
    runtime/core/ir.cpp
    runtime/core/language_router.cpp
    runtime/core/pipeline.cpp
    runtime/core/reading.cpp
    runtime/core/text.cpp
    runtime/model/artifact.cpp
    runtime/plugin/provider_registry.cpp
    runtime/providers/chinese/chinese_provider.cpp
    runtime/providers/english/english_provider.cpp
    runtime/providers/generic/generic_provider.cpp
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
if(LAE_ENABLE_DYNAMIC_PROVIDERS)
    target_compile_definitions(le_runtime PRIVATE LE_DYNAMIC_PROVIDER_LOADING=1)
    target_link_libraries(le_runtime PRIVATE ${CMAKE_DL_LIBS})
endif()
set_target_properties(le_runtime PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

if(LAE_BUILD_CLI)
    add_executable(le-cli tools/cli/main.c)
    target_compile_features(le-cli PRIVATE c_std_11)
    target_link_libraries(le-cli PRIVATE le::runtime)

    add_executable(le-model tools/model/main.cpp runtime/model/artifact.cpp)
    target_compile_features(le-model PRIVATE cxx_std_20)
    target_include_directories(le-model PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    )
endif()

if(LAE_BUILD_TESTS)
    enable_testing()
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

    add_executable(le-runtime-tests tests/runtime_tests.cpp)
    target_compile_features(le-runtime-tests PRIVATE cxx_std_20)
    target_link_libraries(le-runtime-tests PRIVATE le::runtime)
    add_test(NAME runtime COMMAND le-runtime-tests)

    add_executable(le-core-text-tests tests/core_text_tests.cpp runtime/core/text.cpp)
    target_compile_features(le-core-text-tests PRIVATE cxx_std_20)
    target_include_directories(le-core-text-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
    target_link_libraries(le-core-text-tests PRIVATE PkgConfig::UTF8PROC)
    add_test(NAME core-text COMMAND le-core-text-tests)

    add_executable(le-core-ir-tests
        tests/core_ir_tests.cpp
        runtime/core/ir.cpp
        runtime/core/text.cpp
        runtime/providers/generic/generic_provider.cpp
    )
    target_compile_features(le-core-ir-tests PRIVATE cxx_std_20)
    target_include_directories(le-core-ir-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
    target_link_libraries(le-core-ir-tests PRIVATE PkgConfig::UTF8PROC)
    add_test(NAME core-ir COMMAND le-core-ir-tests)

    add_executable(le-core-policy-tests
        tests/core_policy_tests.cpp
        runtime/core/emphasis.cpp
        runtime/core/reading.cpp
        runtime/core/text.cpp
    )
    target_compile_features(le-core-policy-tests PRIVATE cxx_std_20)
    target_include_directories(le-core-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
    target_link_libraries(le-core-policy-tests PRIVATE PkgConfig::UTF8PROC)
    add_test(NAME core-policy COMMAND le-core-policy-tests)

    add_executable(le-core-english-tests
        tests/core_english_tests.cpp
        runtime/core/ir.cpp
        runtime/core/text.cpp
        runtime/providers/english/english_provider.cpp
    )
    target_compile_features(le-core-english-tests PRIVATE cxx_std_20)
    target_include_directories(le-core-english-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
    target_link_libraries(le-core-english-tests PRIVATE PkgConfig::UTF8PROC)
    add_test(NAME core-english COMMAND le-core-english-tests)

    add_executable(le-core-chinese-tests
        tests/core_chinese_tests.cpp
        runtime/core/ir.cpp
        runtime/core/text.cpp
        runtime/providers/chinese/chinese_provider.cpp
    )
    target_compile_features(le-core-chinese-tests PRIVATE cxx_std_20)
    target_include_directories(le-core-chinese-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
    target_link_libraries(le-core-chinese-tests PRIVATE PkgConfig::UTF8PROC)
    add_test(NAME core-chinese COMMAND le-core-chinese-tests)

    add_executable(le-model-artifact-tests
        tests/model_artifact_tests.cpp
        runtime/model/artifact.cpp
    )
    target_compile_features(le-model-artifact-tests PRIVATE cxx_std_20)
    target_include_directories(le-model-artifact-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    )
    add_test(NAME model-artifact COMMAND le-model-artifact-tests)

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

    if(LAE_ENABLE_DYNAMIC_PROVIDERS)
        add_library(le-test-provider MODULE tests/plugins/test_provider.c)
        target_compile_features(le-test-provider PRIVATE c_std_11)
        target_include_directories(le-test-provider PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(le-test-provider PROPERTIES
            C_VISIBILITY_PRESET hidden
            PREFIX ""
        )

        add_executable(le-plugin-loader-tests tests/plugin_loader_tests.cpp)
        target_compile_features(le-plugin-loader-tests PRIVATE cxx_std_20)
        target_link_libraries(le-plugin-loader-tests PRIVATE le::runtime)
        add_test(NAME plugin-dynamic
            COMMAND le-plugin-loader-tests $<TARGET_FILE:le-test-provider>)
    endif()

    add_test(NAME training-unit
        COMMAND ${CMAKE_COMMAND} -E env
                PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/training/src
                ${Python3_EXECUTABLE} -m unittest discover
                -s ${CMAKE_CURRENT_SOURCE_DIR}/training/tests -v
    )
    add_test(NAME training-plan-evaluation
        COMMAND ${CMAKE_COMMAND} -E env
                PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/training/src
                ${Python3_EXECUTABLE} -m lae_training summarize-plans
                ${CMAKE_CURRENT_SOURCE_DIR}/training/tests/fixtures/plans.jsonl
                --baseline plain
    )
    set_tests_properties(training-plan-evaluation PROPERTIES
        PASS_REGULAR_EXPRESSION "\"candidate\": \"traditional-prefix\""
    )
    add_test(NAME training-study-evaluation
        COMMAND ${CMAKE_COMMAND} -E env
                PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/training/src
                ${Python3_EXECUTABLE} -m lae_training summarize-study
                ${CMAKE_CURRENT_SOURCE_DIR}/training/tests/fixtures/study.jsonl
                --baseline plain
    )
    set_tests_properties(training-study-evaluation PROPERTIES
        PASS_REGULAR_EXPRESSION "\"mean_reading_speed_delta\""
    )

    if(BUILD_SHARED_LIBS)
        add_test(NAME example-terminal-bold
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/examples/terminal_bold.py
                    --library $<TARGET_FILE:le_runtime>
                    "Provider APIs can drive terminal emphasis."
        )
    endif()

    if(LAE_BUILD_CLI)
        add_test(NAME cli-analysis
            COMMAND le-cli --language zh-Hans --dump-analysis
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/mixed.txt)
        set_tests_properties(cli-analysis PROPERTIES
            PASS_REGULAR_EXPRESSION "\"kind\":\"unit\""
        )
        add_test(NAME cli-signals
            COMMAND le-cli --fixed 2 --dump-signals
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/mixed.txt)
        set_tests_properties(cli-signals PROPERTIES
            PASS_REGULAR_EXPRESSION "\"fixation_salience\":0.500"
        )
        add_test(NAME cli-variable-policy
            COMMAND le-cli --fixed 2 --policy variable --min-strength 0.2
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/mixed.txt)
        set_tests_properties(cli-variable-policy PROPERTIES
            PASS_REGULAR_EXPRESSION "\"strength\":0.600"
        )
        add_test(NAME cli-english-lexical-core
            COMMAND le-cli --language en --model lexical-core --dump-signals
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/english.txt)
        set_tests_properties(cli-english-lexical-core PROPERTIES
            PASS_REGULAR_EXPRESSION "\"lexical_salience\":1.000"
        )
        add_test(NAME cli-chinese-lexical-core
            COMMAND le-cli --language zh-Hans --model lexical-core --dump-signals
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/chinese.txt)
        set_tests_properties(cli-chinese-lexical-core PROPERTIES
            PASS_REGULAR_EXPRESSION "\"begin\":12,\"end\":21"
        )
        set(LE_TEST_MODEL ${CMAKE_CURRENT_BINARY_DIR}/test-prefix.lem)
        add_test(NAME model-cli-compile
            COMMAND le-model compile-prefix ${LE_TEST_MODEL} --fixed 2 --language en)
        add_test(NAME model-cli-inspect COMMAND le-model inspect ${LE_TEST_MODEL})
        set_tests_properties(model-cli-inspect PROPERTIES
            DEPENDS model-cli-compile
            PASS_REGULAR_EXPRESSION "\"type\":\"prefix\""
        )
        add_test(NAME model-cli-signals
            COMMAND le-cli --artifact ${LE_TEST_MODEL} --language en --dump-signals
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/english.txt)
        set_tests_properties(model-cli-signals PROPERTIES
            DEPENDS model-cli-compile
            PASS_REGULAR_EXPRESSION "\"fixation_salience\":0.500"
        )
        set(LE_TEST_LINEAR_MODEL ${CMAKE_CURRENT_BINARY_DIR}/test-linear.lem)
        add_test(NAME model-linear-compile
            COMMAND le-model compile-linear-salience ${LE_TEST_LINEAR_MODEL}
                    --bias 0 --weight 2:0.2 --language en
        )
        add_test(NAME model-linear-inspect COMMAND le-model inspect ${LE_TEST_LINEAR_MODEL})
        set_tests_properties(model-linear-inspect PROPERTIES
            DEPENDS model-linear-compile
            PASS_REGULAR_EXPRESSION "\"type\":\"linear-salience\""
        )
        add_test(NAME model-linear-runtime
            COMMAND le-cli --artifact ${LE_TEST_LINEAR_MODEL} --language en --dump-signals
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/english.txt
        )
        set_tests_properties(model-linear-runtime PROPERTIES
            DEPENDS model-linear-compile
            PASS_REGULAR_EXPRESSION "\"lexical_salience\":1.000"
        )
        set(LE_TEST_TRAINED_MODEL ${CMAKE_CURRENT_BINARY_DIR}/test-trained-prefix.lem)
        add_test(NAME training-fit-prefix
            COMMAND ${CMAKE_COMMAND} -E env
                    PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/training/src
                    ${Python3_EXECUTABLE} -m lae_training fit-prefix
                    ${CMAKE_CURRENT_SOURCE_DIR}/training/tests/fixtures/prefix-training.jsonl
                    ${LE_TEST_TRAINED_MODEL}
        )
        set_tests_properties(training-fit-prefix PROPERTIES
            PASS_REGULAR_EXPRESSION "\"strategy\": \"fixed\""
        )
        add_test(NAME training-model-inspect
            COMMAND le-model inspect ${LE_TEST_TRAINED_MODEL}
        )
        set_tests_properties(training-model-inspect PROPERTIES
            DEPENDS training-fit-prefix
            PASS_REGULAR_EXPRESSION "\"fixed_graphemes\":2"
        )
        add_test(NAME training-model-runtime
            COMMAND le-cli --artifact ${LE_TEST_TRAINED_MODEL} --language en --dump-signals
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/english.txt
        )
        set_tests_properties(training-model-runtime PROPERTIES
            DEPENDS training-fit-prefix
            PASS_REGULAR_EXPRESSION "\"fixation_salience\":0.500"
        )
        set(LE_TEST_TRAINED_LINEAR ${CMAKE_CURRENT_BINARY_DIR}/test-trained-linear.lem)
        add_test(NAME training-fit-linear
            COMMAND ${CMAKE_COMMAND} -E env
                    PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/training/src
                    ${Python3_EXECUTABLE} -m lae_training fit-linear-salience
                    ${CMAKE_CURRENT_SOURCE_DIR}/training/tests/fixtures/salience-training.jsonl
                    ${LE_TEST_TRAINED_LINEAR} --ridge 0
        )
        set_tests_properties(training-fit-linear PROPERTIES
            PASS_REGULAR_EXPRESSION "\"root_mean_square_error\""
        )
        add_test(NAME training-linear-inspect
            COMMAND le-model inspect ${LE_TEST_TRAINED_LINEAR}
        )
        set_tests_properties(training-linear-inspect PROPERTIES
            DEPENDS training-fit-linear
            PASS_REGULAR_EXPRESSION "\"type\":\"linear-salience\""
        )
        add_test(NAME training-linear-runtime
            COMMAND le-cli --artifact ${LE_TEST_TRAINED_LINEAR} --language en --dump-signals
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/english.txt
        )
        set_tests_properties(training-linear-runtime PROPERTIES
            DEPENDS training-fit-linear
            PASS_REGULAR_EXPRESSION "\"lexical_salience\":1.000"
        )
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS le_runtime
    EXPORT leTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(LAE_BUILD_CLI)
    if(APPLE)
        set_target_properties(le-cli PROPERTIES
            INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}"
        )
    elseif(UNIX)
        set_target_properties(le-cli PROPERTIES
            INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
        )
    endif()
    install(TARGETS le-cli le-model RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
