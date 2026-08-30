if(NOT DEFINED LAE_SOURCE_DIR OR NOT DEFINED LAE_BUILD_DIR OR
   NOT DEFINED LAE_CTEST_COMMAND)
    message(FATAL_ERROR "package consumer test is missing required paths")
endif()

set(stage "${LAE_BUILD_DIR}/package-consumer-stage")
set(prefix "${LAE_BUILD_DIR}/package-consumer-prefix")
set(consumer_build "${LAE_BUILD_DIR}/package-consumer-build")
file(REMOVE_RECURSE "${stage}" "${prefix}" "${consumer_build}")

set(install_command "${CMAKE_COMMAND}" --install "${LAE_BUILD_DIR}" --prefix "${stage}")
set(build_command "${CMAKE_COMMAND}" --build "${consumer_build}")
set(test_command "${LAE_CTEST_COMMAND}" --test-dir "${consumer_build}" --output-on-failure)
if(LAE_CONFIG)
    list(APPEND install_command --config "${LAE_CONFIG}")
    list(APPEND build_command --config "${LAE_CONFIG}")
    list(APPEND test_command --build-config "${LAE_CONFIG}")
endif()

execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "runtime installation failed:\n${output}${error}")
endif()
file(RENAME "${stage}" "${prefix}")
if(NOT EXISTS "${prefix}/share/doc/lae/LICENSE")
    message(FATAL_ERROR "relocated runtime package does not contain the MIT license")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -S "${LAE_SOURCE_DIR}/examples/cmake-consumer"
            -B "${consumer_build}"
            "-DCMAKE_PREFIX_PATH=${prefix}"
            "-DCMAKE_BUILD_TYPE=${LAE_CONFIG}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "consumer configuration failed:\n${output}${error}")
endif()

execute_process(
    COMMAND ${build_command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "consumer build failed:\n${output}${error}")
endif()

execute_process(
    COMMAND ${test_command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "installed runtime test failed:\n${output}${error}")
endif()
