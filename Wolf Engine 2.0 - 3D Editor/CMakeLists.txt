cmake_minimum_required(VERSION 3.31)
project(WolfEngine_3DEditor)

set(CMAKE_CXX_STANDARD 23)

file(GLOB SRC
        "*.cpp"
)

# Includes Wolf libs
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/Common")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/GraphicAPIBroker/Public")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/Wolf Engine 2.0")

# Includes third parties
include_directories("./../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/xxh64")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/glm")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/GLFW/include")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/tiny_obj")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/FrustumCull")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/stb_image")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/UltraLight/include")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/vulkan/Include")
include_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/Tracy/tracy")
include_directories("../GifEncoder/egif")

link_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/x64/Debug/lib")
link_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/vulkan/Lib")
link_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/GLFW/lib-vc2019")
link_directories("../../Wolf Engine 2.0/Wolf Engine 2.0/ThirdParty/UltraLight/lib")
link_directories("../x64/Debug")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(WolfEngine_3DEditor ${SRC})

target_compile_definitions(WolfEngine_3DEditor PUBLIC WOLF_VULKAN)
target_compile_definitions(WolfEngine_3DEditor PUBLIC MATERIAL_DEBUG)
target_compile_definitions(WolfEngine_3DEditor PUBLIC TRACY_ENABLE)

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
        RUNTIME_OUTPUT_DIRECTORY "../../x64/Debug/exe")

#add_dependencies(WolfEngine_3DEditor
#        BrowseToFile
#        Engine Core
#        GifEncoder
#        "Graphic API Broker"
#)