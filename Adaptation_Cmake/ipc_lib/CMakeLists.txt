cmake_minimum_required(VERSION 3.25.0)

project(IPC_CMAKE_LIB)

set(CMAKE_C_COMPILER "gcc" )
set(CMAKE_CXX_COMPILER "g++" )

add_library(${PROJECT_NAME} lib.cpp)