cmake_minimum_required(VERSION 3.31)
include(FetchContent)
project(WolfEngine_3DEditor)

set(CMAKE_CXX_STANDARD 23)

file(GLOB SRC
        "Wolf Engine 2.0 - 3D Editor/*.cpp"
)

set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
FetchContent_Declare(WolfEngineContent
        GIT_REPOSITORY https://github.com/arthur-monteiro/WolfEngine-2.0
        GIT_TAG d732eb79ab100c245e162aaabbd971a7f326040c
        SOURCE_SUBDIR "__skip_subdirectory__")
FetchContent_MakeAvailable(WolfEngineContent)

# Includes Wolf libs
include_directories("dependencies/wolfenginecontent-src/Common")
include_directories("dependencies/wolfenginecontent-src/GraphicAPIBroker/Public")
include_directories("dependencies/wolfenginecontent-src/Wolf-Engine-2.0")

# Includes third parties
include_directories("dependencies/wolfenginecontent-src/ThirdParty/xxh64")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/glm")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/GLFW/include")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/tiny_obj")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/FrustumCull")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/stb_image")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/UltraLight/include")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/vulkan/Include")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/Tracy/tracy")
include_directories("GifEncoder/egif")

link_directories("dependencies/wolfenginecontent-src/x64/Debug/lib")
link_directories("dependencies/wolfenginecontent-src/ThirdParty/vulkan/Lib")
link_directories("dependencies/wolfenginecontent-src/ThirdParty/GLFW/lib-vc2019")
link_directories("dependencies/wolfenginecontent-src/ThirdParty/UltraLight/lib")
link_directories("dependencies/wolfenginecontent-src/ThirdParty/meshoptimizer/lib")
link_directories("x64/Debug/lib")

add_subdirectory("GifEncoder")
add_subdirectory("BrowseToFile")
add_subdirectory("dependencies/wolfenginecontent-src")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(WolfEngine_3DEditor ${SRC})

target_compile_definitions(WolfEngine_3DEditor PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(WolfEngine_3DEditor PUBLIC GLM_FORCE_RADIANS)
target_compile_definitions(WolfEngine_3DEditor PUBLIC GLM_ENABLE_EXPERIMENTAL)
target_compile_definitions(WolfEngine_3DEditor PUBLIC WOLF_VULKAN)
target_compile_definitions(WolfEngine_3DEditor PUBLIC MATERIAL_DEBUG)
target_compile_definitions(WolfEngine_3DEditor PUBLIC TRACY_ENABLE)
target_compile_definitions(WolfEngine_3DEditor PUBLIC TRACY_ON_DEMAND)

if (RESOURCE_TRACKING OR RESOURCE_DEBUG)
    target_compile_definitions(
            WolfEngine_3DEditor
            PRIVATE
            $<$<BOOL:RESOURCE_TRACKING>:RESOURCE_TRACKING>
            $<$<BOOL:RESOURCE_DEBUG>:RESOURCE_DEBUG>
    )
endif()

target_link_libraries(WolfEngine_3DEditor vulkan-1.lib)
target_link_libraries(WolfEngine_3DEditor glfw3.lib)
target_link_libraries(WolfEngine_3DEditor Ultralight.lib)
target_link_libraries(WolfEngine_3DEditor UltralightCore.lib)
target_link_libraries(WolfEngine_3DEditor WebCore.lib)
target_link_libraries(WolfEngine_3DEditor AppCore.lib)
target_link_libraries(WolfEngine_3DEditor meshoptimizer.lib)
target_link_libraries(WolfEngine_3DEditor Common.lib)
target_link_libraries(WolfEngine_3DEditor GraphicAPIBroker.lib)
target_link_libraries(WolfEngine_3DEditor WolfEngine.lib)
target_link_libraries(WolfEngine_3DEditor GifEncoder.lib)

add_dependencies(WolfEngine_3DEditor GifEncoder)
add_dependencies(WolfEngine_3DEditor Common)
add_dependencies(WolfEngine_3DEditor GraphicAPIBroker)
add_dependencies(WolfEngine_3DEditor WolfEngine)

set_target_properties(WolfEngine_3DEditor
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/x64/${CMAKE_BUILD_TYPE}/exe"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug/exe"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/exe")