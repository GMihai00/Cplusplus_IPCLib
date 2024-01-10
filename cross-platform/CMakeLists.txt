cmake_minimum_required(VERSION 3.25.0)


# project(IPC LANGUAGES CXX)

# Add all .cpp files to lib
file(GLOB_RECURSE LIB_SOURCES ../src/*.cpp)

# Add all .hpp files to lib
file(GLOB_RECURSE LIB_HEADERS ../src/*.hpp)

message(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE EXTERNAL_INCLUDE_DIRECTORIES_FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../external/*/include/*)

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

message("Include Directories:")
foreach(item ${EXTERNAL_INCLUDE_DIRECTORIES})
    message("-- ${item}")
endforeach()

add_library(${PROJECT_NAME} ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE ../src PRIVATE ${EXTERNAL_INCLUDE_DIRECTORIES})
# target_linking_directories(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME})