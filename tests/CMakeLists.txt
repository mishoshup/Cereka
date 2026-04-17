# tests/CMakeLists.txt — Cereka Engine Unit Tests
#
# Builds and runs GoogleTest-based unit tests.

include(FetchContent)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Test executable - use console subsystem on Windows for GoogleTest
if(WIN32)
    add_executable(cereka_test
        config_test.cpp
        save_data_test.cpp
        main.cpp
    )
    target_link_options(cereka_test PRIVATE "-Wl,--subsystem,console" "-municode")
else()
    add_executable(cereka_test
        config_test.cpp
        save_data_test.cpp
    )
endif()

target_include_directories(cereka_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(cereka_test PRIVATE
    Cereka
    gtest
    gmock
)

# Discover tests (optional - can also run directly)
include(GoogleTest)
# gtest_discover_tests(cereka_test) # Disabled due to DLL path issues
