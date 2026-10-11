# Require modern CMake version 3.20 or newer
cmake_minimum_required(VERSION 3.20)
# Define the project name and enable C++ language
project(RTSched LANGUAGES CXX)
# Specify C++20 standard for modern language features
set(CMAKE_CXX_STANDARD 20)
# Enforce that C++20 is strictly required
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Disable non-standard compiler extensions for portability
set(CMAKE_CXX_EXTENSIONS OFF)
# Locate threading library support for multi-threaded dispatching
find_package(Threads REQUIRED)
# Include FetchContent module to manage GoogleTest dependency
include(FetchContent)
# Prevent GoogleTest from installing headers and binaries
set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest install" FORCE)
# Use dynamic runtime matching host compilation mode
set(gtest_force_shared_crt ON CACHE BOOL "Use shared CRT" FORCE)
# Declare GoogleTest repository location and release tag
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Fetch and expose GoogleTest targets
FetchContent_MakeAvailable(googletest)
# Create RTSched core static library target
add_library(rtsched
  src/policy.cpp
  src/monitor.cpp
  src/watchdog.cpp
  src/scheduler.cpp
  src/gantt.cpp
)
# Export public include directory for RTSched library
target_include_directories(rtsched PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Link system threading library to RTSched
target_link_libraries(rtsched PUBLIC Threads::Threads)
# Configure strict compilation warnings for RTSched library
if(MSVC)
  # Enable warning level 4 and treat warnings as errors on MSVC
  target_compile_options(rtsched PRIVATE /W4 /WX)
# Otherwise configure strict warnings for GCC and Clang
else()
  # Enable all warnings and treat warnings as errors
  target_compile_options(rtsched PRIVATE -Wall -Wextra -Werror)
# End compiler check block for RTSched
endif()
# Create executable target for the demonstration CLI
add_executable(rtsched_demo src/main.cpp)
# Link core RTSched library to the demo executable
target_link_libraries(rtsched_demo PRIVATE rtsched)
# Configure strict compilation warnings for demo target
if(MSVC)
  # Enable warning level 4 and treat warnings as errors on MSVC
  target_compile_options(rtsched_demo PRIVATE /W4 /WX)
# Otherwise configure strict warnings for GCC and Clang
else()
  # Enable all warnings and treat warnings as errors
  target_compile_options(rtsched_demo PRIVATE -Wall -Wextra -Werror)
# End compiler check block for demo
endif()
# Enable testing integration with CTest
enable_testing()
# Create GoogleTest test executable target
add_executable(rtsched_tests tests/tests.cpp)
# Link core library and GoogleTest runner to test executable
target_link_libraries(rtsched_tests PRIVATE rtsched GTest::gtest_main)
# Configure strict compilation warnings for test target
if(MSVC)
  # Enable warning level 4 and treat warnings as errors on MSVC
  target_compile_options(rtsched_tests PRIVATE /W4 /WX)
# Otherwise configure strict warnings for GCC and Clang
else()
  # Enable all warnings and treat warnings as errors
  target_compile_options(rtsched_tests PRIVATE -Wall -Wextra -Werror)
# End compiler check block for tests
endif()
# Register test executable with CTest runner
add_test(NAME RTSchedTests COMMAND rtsched_tests)
