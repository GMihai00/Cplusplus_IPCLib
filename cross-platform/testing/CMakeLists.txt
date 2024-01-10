cmake_minimum_required(VERSION 3.25.0)

project(IPC_CMAKE LANGUAGES CXX)

# message(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. build_ipc)

find_package(OpenSSL REQUIRED)

if(OPENSSL_FOUND)
    message(STATUS "OpenSSL found")
    include_directories(${OPENSSL_INCLUDE_DIR})
else()
    message(FATAL_ERROR "OpenSSL not found")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ../../src)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    zlib 
    OpenSSL::SSL 
    OpenSSL::Crypto 
    Boost::align 
    Boost::asio 
    Boost::assert
    Boost::bind
    Boost::chrono
    Boost::config
    Boost::container_hash
    Boost::core
    Boost::date_time
    Boost::describe
    Boost::functional
    Boost::integer
    Boost::io
    Boost::move
    Boost::mp11
    Boost::mpl
    Boost::numeric_conversion
    Boost::predef
    Boost::preprocessor
    Boost::ratio
    Boost::regex
    Boost::smart_ptr
    Boost::static_assert
    Boost::system
    Boost::thread
    Boost::throw_exception
    Boost::type_traits
    Boost::utility
    Boost::winapi
    IPC
)