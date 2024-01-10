cmake_minimum_required(VERSION 3.25.0)

project(IPC_CMAKE LANGUAGES CXX)

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(ipc_lib)

target_include_directories(${PROJECT_NAME} PUBLIC ipc_lib)
target_link_libraries(${PROJECT_NAME} IPC_CMAKE_LIB)