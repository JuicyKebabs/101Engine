cmake_minimum_required(VERSION 3.20)
project(101Engine)

# Stage1: 101FrameworkをDLL化したため、ランタイムライブラリを動的(/MD, /MDd)に変更
# Debug   -> MultiThreadedDebugDLL (/MDd)
# Release -> MultiThreadedDLL      (/MD)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(Framework)
add_subdirectory(Game/GameCode)
add_subdirectory(Editor)
add_subdirectory(Game)
