cmake_minimum_required(VERSION 3.25.0)

project(web_testing_server LANGUAGES CXX)

file(GLOB_RECURSE EXTERNAL_INCLUDE_DIRECTORIES_FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../external/*/include/*)

file(GLOB_RECURSE BOOST_EXTERNAL_INCLUDE_DIRECTORIES_FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../external/boost/*/include/*)

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. build_ipc)

find_package(OpenSSL REQUIRED)

if(OPENSSL_FOUND)
    message(STATUS "OpenSSL found")
    include_directories(${OPENSSL_INCLUDE_DIR})
else()
    message(FATAL_ERROR "OpenSSL not found")
endif()

set(EXTERNAL_INCLUDE_DIRECTORIES)
# Print the paths up to and including "include"

foreach(item ${EXTERNAL_INCLUDE_DIRECTORIES_FULL_PATH})
    string(FIND ${item} "include" index REVERSE)
    if(NOT ${index} EQUAL -1)
        string(SUBSTRING ${item} 0 ${index} item)
    endif()
    list(APPEND EXTERNAL_INCLUDE_DIRECTORIES ${item}include/)
endforeach()

foreach(item ${BOOST_EXTERNAL_INCLUDE_DIRECTORIES_FULL_PATH})
    string(FIND ${item} "include" index REVERSE)
    if(NOT ${index} EQUAL -1)
        string(SUBSTRING ${item} 0 ${index} item)
    endif()
    list(APPEND EXTERNAL_INCLUDE_DIRECTORIES ${item}include/)
endforeach()

# Remove duplicates from the list
list(REMOVE_DUPLICATES EXTERNAL_INCLUDE_DIRECTORIES)

message("Include Directories boost:")
foreach(item ${EXTERNAL_INCLUDE_DIRECTORIES})
    message("-- ${item}")
endforeach()

target_include_directories(${PROJECT_NAME} PRIVATE ../../src PRIVATE ${EXTERNAL_INCLUDE_DIRECTORIES})
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
    Boost::atomic
    Boost::context
    Boost::typeof
    Boost::algorithm
    Boost::function
    Boost::conversion
    Boost::variant2
    Boost::concept_check
    Boost::coroutine
    Boost::lexical_cast
    Boost::function_types
    Boost::container
    Boost::pool
    Boost::array
    Boost::range
    Boost::exception
    Boost::detail
    Boost::intrusive
    Boost::tokenizer
    Boost::optional
    Boost::iterator
    Boost::tuple
    Boost::unordered
    Boost::fusion
    IPC
    ws2_32
    wsock32
)