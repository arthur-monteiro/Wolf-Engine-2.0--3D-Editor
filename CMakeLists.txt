cmake_minimum_required(VERSION 3.31)
project(WolfEngine_3DEditor)

set(CMAKE_CXX_STANDARD 23)

file(GLOB SRC
        "Wolf Engine 2.0 - 3D Editor/*.cpp"
)

# Includes Wolf libs
include_directories("dependencies/Wolf-Engine-2.0/Common")
include_directories("dependencies/Wolf-Engine-2.0/GraphicAPIBroker/Public")
include_directories("dependencies/Wolf-Engine-2.0/Wolf-Engine-2.0")

# Includes third parties
include_directories("dependencies/Wolf-Engine-2.0/ThirdParty/xxh64")
include_directories("dependencies/Wolf-Engine-2.0/ThirdParty/glm")
include_directories("dependencies/Wolf-Engine-2.0/ThirdParty/GLFW/include")
include_directories("dependencies/Wolf-Engine-2.0/ThirdParty/tiny_obj")
include_directories("dependencies/Wolf-Engine-2.0/ThirdParty/FrustumCull")
include_directories("dependencies/Wolf-Engine-2.0/ThirdParty/stb_image")
include_directories("dependencies/Wolf-Engine-2.0/ThirdParty/UltraLight/include")
include_directories("dependencies/Wolf-Engine-2.0/ThirdParty/vulkan/Include")
include_directories("dependencies/Wolf-Engine-2.0/ThirdParty/Tracy/tracy")
include_directories("GifEncoder/egif")

link_directories("dependencies/Wolf-Engine-2.0/x64/Debug/lib")
link_directories("dependencies/Wolf-Engine-2.0/ThirdParty/vulkan/Lib")
link_directories("dependencies/Wolf-Engine-2.0/ThirdParty/GLFW/lib-vc2019")
link_directories("dependencies/Wolf-Engine-2.0/ThirdParty/UltraLight/lib")
link_directories("x64/Debug")

add_subdirectory("dependencies/Wolf-Engine-2.0/Common")
add_subdirectory("dependencies/Wolf-Engine-2.0/GraphicAPIBroker")
add_subdirectory("dependencies/Wolf-Engine-2.0/Wolf-Engine-2.0")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(WolfEngine_3DEditor ${SRC})

target_link_libraries(WolfEngine_3DEditor Common)
target_link_libraries(WolfEngine_3DEditor GraphicAPIBroker)
target_link_libraries(WolfEngine_3DEditor WolfEngine)

target_compile_definitions(WolfEngine_3DEditor PUBLIC WOLF_VULKAN)
target_compile_definitions(WolfEngine_3DEditor PUBLIC MATERIAL_DEBUG)
target_compile_definitions(WolfEngine_3DEditor PUBLIC TRACY_ENABLE)
target_compile_definitions(WolfEngine_3DEditor PUBLIC TRACY_ON_DEMAND)

target_link_libraries(WolfEngine_3DEditor vulkan-1.lib)
target_link_libraries(WolfEngine_3DEditor glfw3.lib)
target_link_libraries(WolfEngine_3DEditor Ultralight.lib)
target_link_libraries(WolfEngine_3DEditor UltralightCore.lib)
target_link_libraries(WolfEngine_3DEditor WebCore.lib)
target_link_libraries(WolfEngine_3DEditor AppCore.lib)
target_link_libraries(WolfEngine_3DEditor Common.lib)
target_link_libraries(WolfEngine_3DEditor GraphicAPIBroker.lib)
target_link_libraries(WolfEngine_3DEditor WolfEngine.lib)
target_link_libraries(WolfEngine_3DEditor GifEncoder.lib)

set_target_properties(WolfEngine_3DEditor
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug/exe")