cmake_minimum_required(VERSION 3.25)

project(CHAINSIGHT
    VERSION 0.1.0
    DESCRIPTION "Standalone industrial operations world for governed planning and verification"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHAINSIGHT_BUILD_TESTS "Build CHAINSIGHT tests" ON)

add_library(chainsight_core
    src/iag.cpp
    src/operations.cpp
)
target_include_directories(chainsight_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(chainsight_core PUBLIC cxx_std_23)

if(MSVC)
    target_compile_options(chainsight_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(chainsight_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(iag_scenario_runner
    apps/iag_scenario_runner/main.cpp
)
target_link_libraries(iag_scenario_runner PRIVATE chainsight_core)

if(CHAINSIGHT_BUILD_TESTS)
    enable_testing()
    add_executable(iag_tests
        tests/test_foundation.cpp
    )
    target_link_libraries(iag_tests PRIVATE chainsight_core)
    add_test(NAME iag_tests COMMAND iag_tests)
endif()
