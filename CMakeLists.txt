# R7 implementation guide: section 4.41.
# Register the guide's limited client integration probes.
# The guide's sibling ../validation addition is external-candidate infrastructure only.
# Production validation is injected from the implementation workspace instead of adding that dependency here.

cmake_minimum_required(VERSION 3.20)
project(101Engine)

include(CTest)

# Stage1: 101FrameworkをDLL化したため、ランタイムライブラリを動的(/MD, /MDd)に変更
# Debug   -> MultiThreadedDebugDLL (/MDd)
# Release -> MultiThreadedDLL      (/MD)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(ENGINE101_BUILD_ENGINE "Build Framework, GamecCode, Editor, and Game" ON)
option(ENGINE101_BUILD_RENDERER_SANDBOX "Build RendererSandbox" ON)

add_subdirectory(Renderer)

# Renderer専用実行環境
if(ENGINE101_BUILD_RENDERER_SANDBOX)
    add_subdirectory(RendererSandbox)
endif()

# Engine実行環境
if(ENGINE101_BUILD_ENGINE)
    add_subdirectory(Framework)
    add_subdirectory(Game/GameCode)
    add_subdirectory(Editor)
    add_subdirectory(Game)

    # テスト環境の追加
    if(BUILD_TESTING)
        add_subdirectory(Tests)
    endif()
endif()
add_subdirectory(RendererProbes)
