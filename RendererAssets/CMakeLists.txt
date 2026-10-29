# Optional CPU file conversion. The core 101Renderer target never links Assimp,
# DirectXTex, Framework or ImGui as a consequence of this helper existing.
add_library(101RendererAssets STATIC src/AssetFiles.cpp)
target_include_directories(101RendererAssets PUBLIC include PRIVATE
    "${CMAKE_SOURCE_DIR}/third_party/Assimp/include"
    "${CMAKE_SOURCE_DIR}/third_party/DirectX/DirectXTex/include")
target_compile_definitions(101RendererAssets PRIVATE NOMINMAX)
target_link_libraries(101RendererAssets PUBLIC 101Renderer PRIVATE
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/Assimp/lib/Debug/assimp-vc143-mtd.lib>"
    "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/Assimp/lib/Release/assimp-vc143-mt.lib>"
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/DirectX/DirectXTex/lib/Debug/DirectXTex_Debug.lib>"
    "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/DirectX/DirectXTex/lib/Release/DirectXTex_Release.lib>"
    windowscodecs ole32)
