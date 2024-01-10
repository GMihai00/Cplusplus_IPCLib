cmake_minimum_required(VERSION 3.25.0)

project(IPC_CMAKE LANGUAGES CXX)

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(..)

target_include_directories(${PROJECT_NAME} PRIVATE ..)
target_include_directories(${PROJECT_NAME} PRIVATE ..)
target_link_libraries(${PROJECT_NAME} PRIVATE ..)