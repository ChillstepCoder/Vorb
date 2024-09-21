cmake_minimum_required(VERSION 3.30)

# The toolchain file setup should ideally be in a CMakePresets.json file,
# but we'll keep it here for simplicity
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

project(AnimaMundi VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find required packages
find_package(tinyspline CONFIG REQUIRED)
find_package(Bitsery CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-joltphysics CONFIG REQUIRED)
message(STATUS "JoltPhysics targets: ${unofficial-joltphysics_TARGETS}")
find_package(Boost REQUIRED COMPONENTS container unordered pool circular_buffer heap geometry graph)

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

# Link Vorb with required libraries
target_link_libraries(Vorb
    PRIVATE
    tinyspline::tinyspline
    Bitsery::bitsery
    zstd::libzstd
    spdlog::spdlog
    unofficial::joltphysics::Jolt
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
