cmake_minimum_required(VERSION 3.20)
project(solitaire-project LANGUAGES CXX)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
else()
  message(STATUS "ccache not found")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(external/anvil2)

set(TARGET_SOURCES
  src/main.cpp
  src/solitaire.cpp
  src/draw.cpp
  src/assets.cpp
  src/layout.cpp
)

add_executable(solitaire ${TARGET_SOURCES})

target_precompile_headers(solitaire PRIVATE
  <vector>
  <array>
  <string>
  <optional>
  <cassert>
  <cmath>
  <cstdint>
  <cstdio>
  <cstring>
  <chrono>
  <fstream>
  <filesystem>
  <algorithm>
  <random>
  <memory>
  <variant>
  <iostream>
  <unordered_map>
  <unordered_set>
)

target_link_libraries(solitaire PRIVATE anvil2)

target_include_directories(solitaire PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

# Copy assets to the build directory
# How to make sure this only runs on change?
if (NOT EMSCRIPTEN)
  add_custom_target(copy_assets_target ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:solitaire>/assets
    COMMENT "Copying assets..."
    VERBATIM
  )

  add_dependencies(solitaire copy_assets_target)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(solitaire PRIVATE
    -Wall
    -Wextra
    -Wpedantic
  )
endif()
