cmake_minimum_required(VERSION 3.19)
project(GifEncoder)

set(CMAKE_CXX_STANDARD 11)

include_directories(egif)

file(GLOB_RECURSE EGIF_SOURCES "egif/*.h" "egif/*.cpp")

add_library(GifEncoder STATIC ${EGIF_SOURCES})

set_target_properties(GifEncoder
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../x64/${CMAKE_BUILD_TYPE}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../x64/${CMAKE_BUILD_TYPE}/lib"
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_LIST_DIR}/../x64/Debug/lib"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_LIST_DIR}/../x64/Release/lib")