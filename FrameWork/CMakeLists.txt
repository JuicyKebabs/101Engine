# Framework

file(GLOB_RECURSE FRAMEWORK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(101Framework SHARED ${FRAMEWORK_SOURCES})
set_target_properties(101Framework PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_compile_definitions(101Framework PUBLIC NOMINMAX)

target_include_directories(101Framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/DirectX/d3dx12
    ${CMAKE_SOURCE_DIR}/third_party/DirectX/DirectXTex/include
    ${CMAKE_SOURCE_DIR}/third_party/Assimp/include
    ${CMAKE_SOURCE_DIR}/third_party/ImGui
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/backends
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(101Framework PUBLIC
    d3d12.lib dxgi.lib d3dcompiler.lib dxguid.lib xaudio2.lib winmm.lib
    $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/DirectX/DirectXTex/lib/Debug/DirectXTex_Debug.lib>
    $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/DirectX/DirectXTex/lib/Release/DirectXTex_Release.lib>
    $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/Assimp/lib/Debug/assimp-vc143-mtd.lib>
    $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/Assimp/lib/Release/assimp-vc143-mt.lib>
)
