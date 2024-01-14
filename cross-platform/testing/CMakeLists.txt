cmake_minimum_required(VERSION 3.16.3)

if(WIN32)
    message("Building TESTING_SERVER for Windows")
elseif(UNIX)
    message("Building TESTING_SERVER for UNIX")
endif()

project(web_testing_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(EXE_NAME web_testing_server)

option(LOG_ALL "Log everything" ON)

file(GLOB_RECURSE EXTERNAL_INCLUDE_DIRECTORIES_FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../external/*/include/*)

file(GLOB_RECURSE BOOST_EXTERNAL_INCLUDE_DIRECTORIES_FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../external/boost/*/include/*)

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. build_ipc)

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

if (LOG_ALL)
    message("Include Directories boost:")
    foreach(item ${EXTERNAL_INCLUDE_DIRECTORIES})
        message("-- ${item}")
    endforeach()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ../../src PRIVATE ${EXTERNAL_INCLUDE_DIRECTORIES})

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/build/build_ipc)


set(BOOST_LIBRARIES boost_atomic boost_chrono boost_container boost_context boost_date_time boost_exception boost_thread)
foreach(BOOST_LIB ${BOOST_LIBRARIES})
    add_custom_command(TARGET ${EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/build/build_ipc/build_${BOOST_LIB}/
        $<TARGET_FILE_DIR:${EXE_NAME}>)
endforeach()

add_custom_command(TARGET ${EXE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/build/build_ipc/build_zlib/
    $<TARGET_FILE_DIR:${EXE_NAME}>)
    
# if openssl already on system
find_package(OpenSSL REQUIRED)

if(OPENSSL_FOUND)
    message("OpenSSL found")
    include_directories(${OPENSSL_INCLUDE_DIR})
    link_directories(${OPENSSL_LIBRARY_DIR})
else()
    message(FATAL_ERROR "OpenSSL not found")
endif()

#WIP
# add_custom_command(TARGET ${EXE_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#     ${CMAKE_CURRENT_SOURCE_DIR}/build/build_ipc/build_openssl/
#     $<TARGET_FILE_DIR:${EXE_NAME}>)
    
add_custom_command(TARGET ${EXE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/build/build_ipc/libIPC.a
    $<TARGET_FILE_DIR:${EXE_NAME}>)


if(WIN32)
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
elseif(UNIX)
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
        pthread
    )
else()
    message(FATAL_ERROR "Unsupported operating system")
endif()