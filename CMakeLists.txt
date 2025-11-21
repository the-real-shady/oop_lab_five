cmake_minimum_required(VERSION 3.20)

project(oop_lab_five LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(PROJECT_WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
)

add_executable(oop_lab_five
    src/main.cpp
)

target_include_directories(oop_lab_five PRIVATE include)
target_compile_options(oop_lab_five PRIVATE ${PROJECT_WARNING_FLAGS})

if(BUILD_TESTING)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(oop_lab_five_tests
        tests/pmr_queue_tests.cpp
    )

    target_include_directories(oop_lab_five_tests PRIVATE include)
    target_link_libraries(oop_lab_five_tests PRIVATE GTest::gtest_main)
    target_compile_options(oop_lab_five_tests PRIVATE ${PROJECT_WARNING_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(oop_lab_five_tests)
endif()
