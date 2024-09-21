cmake_minimum_required(VERSION 3.30)

# Set the project name and version
project(AnimaMundi VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the path to vcpkg
set(VCPKG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")

# Include vcpkg
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# Add vcpkg manifest mode
set(VCPKG_MANIFEST_MODE ON)
set(VCPKG_MANIFEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Vorb")

# Vorb static library
add_library(Vorb STATIC)
target_include_directories(Vorb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add Vorb sources
file(GLOB_RECURSE VORB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl"
)
target_sources(Vorb PRIVATE ${VORB_SOURCES})

# Add Vorb headers
file(GLOB_RECURSE VORB_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)
target_sources(Vorb PRIVATE ${VORB_HEADERS})

# Set configuration properties for Vorb
if(MSVC)
    target_compile_options(Vorb PRIVATE /W3)
else()
    target_compile_options(Vorb PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GameClient executable
add_executable(GameClient)

# Add GameClient sources and headers
file(GLOB_RECURSE GAMECLIENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src/*.inl"
)
target_sources(GameClient PRIVATE ${GAMECLIENT_SOURCES})

target_include_directories(GameClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/GameClient/src)

# Link GameClient with Vorb and set Vorb as a dependency
target_link_libraries(GameClient PRIVATE Vorb)
add_dependencies(GameClient Vorb)

# Set configuration properties for GameClient
if(MSVC)
    target_compile_options(GameClient PRIVATE /W3)
else()
    target_compile_options(GameClient PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add custom command to update vcpkg as a pre-build step for Vorb
add_custom_command(
    TARGET Vorb
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Updating vcpkg..."
    COMMAND "${VCPKG_ROOT}/vcpkg" update
)