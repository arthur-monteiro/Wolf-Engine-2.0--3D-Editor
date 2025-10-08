cmake_minimum_required(VERSION 3.31)
project(BrowseToFile)

set(CMAKE_CXX_STANDARD 23)

file(GLOB SRC
        "*.cpp"
)

add_executable(BrowseToFile ${SRC})

set_target_properties(BrowseToFile
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Wolf Engine 2.0 - 3D Editor"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../Wolf Engine 2.0 - 3D Editor"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../Wolf Engine 2.0 - 3D Editor")