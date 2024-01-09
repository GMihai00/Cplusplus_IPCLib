cmake_minimum_required(VERSION 3.25.0)

project(IPC_CMAKE)

set(CMAKE_C_COMPILER "gcc" )
set(CMAKE_CXX_COMPILER "g++" )

add_executable(${PROJECT_NAME} main.cpp)

target_link_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ipc_lib/build)
target_link_libraries(${PROJECT_NAME} IPC_CMAKE_LIB)