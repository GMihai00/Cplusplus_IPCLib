cmake_minimum_required(VERSION 3.25.0)


project(IPC LANGUAGES CXX)

# Add all .cpp files to lib
file(GLOB_RECURSE LIB_SOURCES ../src/*.cpp)

# Add all .hpp files to lib
file(GLOB_RECURSE LIB_HEADERS ../src/*.hpp)

file(GLOB_RECURSE BOOST_EXTERNAL_INCLUDE_DIRECTORIES_FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../external/boost/*/include/*)

# Print the cpp files
message("Source Files:")
foreach(source_file ${LIB_SOURCES})
    message("-- ${source_file}")
endforeach()

# Print the header files
message("Header Files:")
foreach(header_file ${LIB_HEADERS})
    message("-- ${header_file}")
endforeach()

set(EXTERNAL_INCLUDE_DIRECTORIES)
# Print the paths up to and including "include"

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

find_package(OpenSSL REQUIRED)

if(OPENSSL_FOUND)
    message(STATUS "OpenSSL found")
    include_directories(${OPENSSL_INCLUDE_DIR})
else()
    message(FATAL_ERROR "OpenSSL not found")
endif()


add_subdirectory(../external/zlib-cmake build_zlib)
add_subdirectory(../external/boost/align build_boost_align)
add_subdirectory(../external/boost/asio build_boost_asio)
add_subdirectory(../external/boost/assert build_boost_assert)
add_subdirectory(../external/boost/bind build_boost_bind)
add_subdirectory(../external/boost/chrono build_boost_chrono)
add_subdirectory(../external/boost/config build_boost_config)
add_subdirectory(../external/boost/container_hash build_boost_container_hash)
add_subdirectory(../external/boost/core build_boost_core)
add_subdirectory(../external/boost/date_time build_boost_date_time)
add_subdirectory(../external/boost/describe build_boost_describe)
add_subdirectory(../external/boost/functional build_boost_functional)
add_subdirectory(../external/boost/integer build_boost_integer)
add_subdirectory(../external/boost/io build_boost_io)
add_subdirectory(../external/boost/move build_boost_move)
add_subdirectory(../external/boost/mp11 build_boost_mp11)
add_subdirectory(../external/boost/mpl build_boost_mpl)
add_subdirectory(../external/boost/numeric_conversion   build_boost_numeric_conversion)
add_subdirectory(../external/boost/predef build_boost_predef)
add_subdirectory(../external/boost/preprocessor build_boost_preprocessor)
add_subdirectory(../external/boost/ratio build_boost_ratio)
add_subdirectory(../external/boost/regex build_boost_regex)
add_subdirectory(../external/boost/smart_ptr build_boost_smart_ptr)
add_subdirectory(../external/boost/static_assert build_boost_static_assert)
add_subdirectory(../external/boost/system build_boost_system)
add_subdirectory(../external/boost/thread build_boost_thread)
add_subdirectory(../external/boost/throw_exception build_boost_throw_exception)
add_subdirectory(../external/boost/type_traits build_boost_type_traits)
add_subdirectory(../external/boost/utility build_boost_utility)
add_subdirectory(../external/boost/winapi build_boost_winapi)
add_subdirectory(../external/boost/atomic build_boost_atomic)

add_library(${PROJECT_NAME} ${LIB_SOURCES} ${LIB_HEADERS})

# target_include_directories(${PROJECT_NAME} PRIVATE ../src)
target_include_directories(${PROJECT_NAME} PRIVATE ../src PRIVATE ${EXTERNAL_INCLUDE_DIRECTORIES})
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
)