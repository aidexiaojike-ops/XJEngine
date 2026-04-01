# stb - single-file public domain libraries
# This is a header-only library interface target

cmake_minimum_required(VERSION 3.10)

project(stb LANGUAGES C)

# Create an interface library for stb
add_library(stb INTERFACE)

# Add the current directory to include path
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "stb header-only library configured")
