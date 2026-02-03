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
        GIT_TAG dfd8710891575d77db2778876f95ba7812ca5031
        SOURCE_SUBDIR "__skip_subdirectory__")
FetchContent_MakeAvailable(WolfEngineContent)

# Includes Wolf libs
include_directories("dependencies/wolfenginecontent-src/Common")
include_directories("dependencies/wolfenginecontent-src/GraphicAPIBroker/Public")
include_directories("dependencies/wolfenginecontent-src/Wolf-Engine-2.0")

# Includes third parties
include_directories("dependencies/wolfenginecontent-src/ThirdParty/xxh64")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/glm")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/tiny_obj")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/FrustumCull")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/stb_image")
if(WIN32)
    include_directories(dependencies/wolfenginecontent-src/ThirdParty/UltraLight/windows/include)
    include_directories(dependencies/wolfenginecontent-src/ThirdParty/vulkan/Include)
    include_directories(dependencies/wolfenginecontent-src/ThirdParty/GLFW/include)
elseif(UNIX AND NOT APPLE)
    include_directories(dependencies/wolfenginecontent-src/ThirdParty/UltraLight/linux/include)
    find_package(Vulkan REQUIRED)
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(CAIRO REQUIRED cairo)
    pkg_check_modules(PANGO REQUIRED pango)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(BZIP2 REQUIRED bzip2)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
endif()
include_directories("dependencies/wolfenginecontent-src/ThirdParty/Tracy/tracy")
include_directories("dependencies/wolfenginecontent-src/ThirdParty/meshoptimizer/src")
include_directories("GifEncoder/egif")

link_directories("dependencies/wolfenginecontent-src/x64/Debug/lib")
link_directories("dependencies/wolfenginecontent-src/ThirdParty/vulkan/Lib")
link_directories("dependencies/wolfenginecontent-src/ThirdParty/GLFW/lib-vc2019")
link_directories("dependencies/wolfenginecontent-src/ThirdParty/UltraLight/windows/lib")
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

if(WIN32)
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
elseif(UNIX AND NOT APPLE)
    set(WOLF_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/x64/Debug/lib")
    set(ULTRALIGHT_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty/UltraLight/linux/bin")
    set(MESHOPTIMIZER_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty/meshoptimizer/lib")

    target_link_libraries(WolfEngine_3DEditor PRIVATE
            ${WOLF_LIB_PATH}/libWolfEngine.a
            ${WOLF_LIB_PATH}/libGraphicAPIBroker.a
            ${WOLF_LIB_PATH}/libCommon.a

            ${ULTRALIGHT_LIB_PATH}/libAppCore.so
            ${ULTRALIGHT_LIB_PATH}/libUltralight.so
            ${ULTRALIGHT_LIB_PATH}/libUltralightCore.so
            ${ULTRALIGHT_LIB_PATH}/libWebCore.so

            ${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug/lib/libGifEncoder.a
            ${MESHOPTIMIZER_LIB_PATH}/libmeshoptimizer.a

            # The system dependencies found by PkgConfig
            ${GTK3_LIBRARIES}
            ${CAIRO_LIBRARIES}
            ${PANGO_LIBRARIES}
            ${X11_LIBRARIES}
            ${BZIP2_LIBRARIES}
            ${FONTCONFIG_LIBRARIES}
            z

            Vulkan::Vulkan
            ${GLFW_LIBRARIES}
            Threads::Threads
            dl
    )
endif()

add_dependencies(WolfEngine_3DEditor GifEncoder)
add_dependencies(WolfEngine_3DEditor Common)
add_dependencies(WolfEngine_3DEditor GraphicAPIBroker)
add_dependencies(WolfEngine_3DEditor WolfEngine)

if(WIN32)
    set(GENERATE_CODE_FILE_HASHES_PATH "../dependencies/wolfenginecontent-src/x64/Debug/exe/GenerateCodeFileHashes.exe")
else()
    set(GENERATE_CODE_FILE_HASHES_PATH "./../dependencies/wolfenginecontent-src/x64/Debug/exe/GenerateCodeFileHashes")
endif()

add_custom_command(COMMAND  ${GENERATE_CODE_FILE_HASHES_PATH} "../Wolf Engine 2.0 - 3D Editor"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Wolf Engine 2.0 - 3D Editor"
        OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/Wolf Engine 2.0 - 3D Editor/CodeFileHashes.h"
        DEPENDS GenerateCodeFileHashes
        VERBATIM)
add_custom_target(
        GenerateEditorHashesTarget
        ALL
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Wolf Engine 2.0 - 3D Editor/CodeFileHashes.h"
)
add_dependencies(WolfEngine_3DEditor GenerateEditorHashesTarget)

set_target_properties(WolfEngine_3DEditor
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/x64/${CMAKE_BUILD_TYPE}/exe"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug/exe"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/exe")

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")