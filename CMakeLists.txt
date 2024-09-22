cmake_minimum_required(VERSION 3.30)

# The toolchain file setup should ideally be in a CMakePresets.json file,
# but we'll keep it here for simplicity
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

project(ProjectKiwi VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find required packages
find_package(tinyspline CONFIG REQUIRED)
find_package(Bitsery CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-joltphysics CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glew CONFIG REQUIRED)
find_package(eventpp CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS container unordered pool circular_buffer heap geometry graph)

# Vorb static library
add_library(Vorb STATIC)
target_include_directories(Vorb 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor_src/imgui #imgui expects this
)

# Vorb sources
file(GLOB_RECURSE VORB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.inl"
)
target_sources(Vorb PRIVATE ${VORB_SOURCES})

# Vendor sources
file(GLOB_RECURSE VENDOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor_src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor_src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor_src/*.hpp"
)
# Remove 'examples' directories
list(FILTER VENDOR_SOURCES EXCLUDE REGEX ".*examples/.*")
# Remove imgui backends
list(FILTER VENDOR_SOURCES EXCLUDE REGEX ".*imgui/backends/.*")
# Append openGL backend
list(APPEND VENDOR_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/vendor_src/imgui/backends/imgui_impl_opengl3.cpp")
target_sources(Vorb PRIVATE ${VENDOR_SOURCES})

# Vorb specific defines
target_compile_definitions(Vorb PRIVATE VORB_IMPL_FONT_SDL)

# Link Vorb with required libraries
target_link_libraries(Vorb
    PUBLIC
    tinyspline::tinyspline
    Bitsery::bitsery
    zstd::libzstd
    spdlog::spdlog
    unofficial::joltphysics::Jolt
    glm::glm
    GLEW::GLEW
    eventpp::eventpp
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
    Boost::container
    Boost::unordered
    Boost::pool
    Boost::circular_buffer
    Boost::heap
    Boost::geometry
    Boost::graph
)

# Set configuration properties for Vorb
if(MSVC)
    target_compile_options(Vorb PRIVATE /W3
)
else()
    target_compile_options(Vorb PRIVATE -Wall -Wextra -Wpedantic) #Arbitrary.. we currently only use MSVC
endif()

# RapidYAML
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/rapidyaml)
set_target_properties(ryml PROPERTIES FOLDER "ryml")
set_target_properties(ryml-uninstall PROPERTIES FOLDER "ryml")

# Ozz-Animation
set(ozz_build_tests OFF CACHE BOOL "")
set(ozz_build_gltf OFF CACHE BOOL "")
set(ozz_build_samples OFF CACHE BOOL "")
set(ozz_build_howtos OFF CACHE BOOL "")
set(ozz_build_tests OFF CACHE BOOL "")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/ozz-animation "bin/ozz-animation/")

# GameClient executable
add_executable(GameClient)
# Set GameClient as the startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GameClient)

# GameClient sources and headers
file(GLOB_RECURSE GAMECLIENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/*.inl"
)
target_sources(GameClient PRIVATE ${GAMECLIENT_SOURCES})

# GameClient include dirs
target_include_directories(GameClient 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor_src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor_src/imgui
)

target_precompile_headers(GameClient PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/stdafx.h")

# Gameclient libraries
target_link_libraries(GameClient PRIVATE
    Vorb
    EnTT::EnTT
    PRIVATE ryml
    PRIVATE ozz_animation
)

# Noesis GUI
target_include_directories(GameClient 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Src/Packages/App/Providers/Include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Src/Packages/App/Theme/Include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Src/Packages/App/ApplicationLauncher/Include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Src/Packages/App/MediaElement/Include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/ui/noesis/vendor/Interactivity/Include # Custom interactivity
)
add_library(Noesis SHARED IMPORTED)
set_target_properties(Noesis PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui"
)
# Set the IMPORTED_LOCATION and IMPORTED_IMPLIB for each configuration
set_target_properties(Noesis PROPERTIES
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisguiBin/windows_x86_64/Noesis.dll"
    IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Lib/windows_x86_64/Noesis.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Bin/windows_x86_64/Noesis.dll"
    IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Lib/windows_x86_64/Noesis.lib"
    IMPORTED_LOCATION_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Bin/windows_x86_64/Noesis.dll"
    IMPORTED_IMPLIB_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Lib/windows_x86_64/Noesis.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Bin/windows_x86_64/Noesis.dll"
    IMPORTED_IMPLIB_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/dependencies/noesisgui/Lib/windows_x86_64/Noesis.lib"
)
target_link_libraries(GameClient
    PRIVATE Noesis
    PRIVATE NoesisApp
)

# Set configuration properties for GameClient
if(MSVC)
    target_compile_options(GameClient PRIVATE
    /W3
    /bigobj
    /Zc:__cplusplus
)
else()
    target_compile_options(GameClient PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Group our stuff into a folder
set_target_properties(Vorb PROPERTIES FOLDER "_Game")
set_target_properties(GameClient PROPERTIES FOLDER "_Game")


# Enable solution folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)