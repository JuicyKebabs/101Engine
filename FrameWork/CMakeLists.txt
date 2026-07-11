# Framework

file(GLOB_RECURSE FRAMEWORK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(101Framework SHARED ${FRAMEWORK_SOURCES})
add_dependencies(101Framework CompileShaders)
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


# --- シェーダー事前コンパイル ---
# fxc.exe を探す。PATHに無い場合、Windows SDKの標準インストール場所も検索する。
find_program(FXC_EXECUTABLE fxc.exe
    PATHS
        "$ENV{WindowsSdkDir}bin/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/x64"
)

if(NOT FXC_EXECUTABLE)
    message(FATAL_ERROR "fxc.exe not found. Please ensure the Windows 10 SDK is installed.")
endif()

message(STATUS "Using fxc.exe: ${FXC_EXECUTABLE}")

# シェーダーファイルのリスト: ソースパス:プロファイル:エントリポイント
set(SHADER_COMPILE_LIST
    "${CMAKE_SOURCE_DIR}/shader/VertexShader/MeshVS.hlsl|vs_5_0|main"
    "${CMAKE_SOURCE_DIR}/shader/VertexShader/SpriteVS.hlsl|vs_5_0|main"
    "${CMAKE_SOURCE_DIR}/shader/VertexShader/UIVS.hlsl|vs_5_0|main"
    "${CMAKE_SOURCE_DIR}/shader/VertexShader/ShadowVS.hlsl|vs_5_0|main"
    "${CMAKE_SOURCE_DIR}/shader/VertexShader/PostEffectVS.hlsl|vs_5_0|main"
    "${CMAKE_SOURCE_DIR}/shader/PixelShader/MeshPS.hlsl|ps_5_0|main"
    "${CMAKE_SOURCE_DIR}/shader/PixelShader/SpritePS.hlsl|ps_5_0|main"
    "${CMAKE_SOURCE_DIR}/shader/PixelShader/UIPS.hlsl|ps_5_0|main"
    "${CMAKE_SOURCE_DIR}/shader/PixelShader/PostEffectPS.hlsl|ps_5_0|main"
)

set(COMPILED_SHADER_OUTPUTS "")

foreach(SHADER_ENTRY ${SHADER_COMPILE_LIST})
    string(REPLACE "|" ";" SHADER_PARTS ${SHADER_ENTRY})
    list(GET SHADER_PARTS 0 SHADER_SRC)
    list(GET SHADER_PARTS 1 SHADER_PROFILE)
    list(GET SHADER_PARTS 2 SHADER_ENTRY_POINT)

    get_filename_component(SHADER_NAME ${SHADER_SRC} NAME_WE)
    get_filename_component(SHADER_DIR ${SHADER_SRC} DIRECTORY)
    get_filename_component(SHADER_SUBDIR ${SHADER_DIR} NAME)  # "VertexShader" or "PixelShader"

    set(SHADER_OUT "${CMAKE_SOURCE_DIR}/shader/compiled/${SHADER_SUBDIR}/${SHADER_NAME}.cso")

    add_custom_command(
        OUTPUT ${SHADER_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/shader/compiled/${SHADER_SUBDIR}"
        COMMAND ${FXC_EXECUTABLE} /nologo /T ${SHADER_PROFILE} /E ${SHADER_ENTRY_POINT}
                /Fo "${SHADER_OUT}" "${SHADER_SRC}"
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling shader: ${SHADER_NAME} (${SHADER_PROFILE})"
        VERBATIM
    )

    list(APPEND COMPILED_SHADER_OUTPUTS ${SHADER_OUT})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADER_OUTPUTS})