cmake_minimum_required(VERSION 3.17) # Check for the version of CMake available in your CLion installation
project(irc_server)

set(CMAKE_CXX_STANDARD 98) # Specify the C++ standard

add_executable(irc_server main.cpp) # Add executable for the project
