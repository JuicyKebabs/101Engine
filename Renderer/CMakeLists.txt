add_library(101Renderer SHARED
    src/Renderer.cpp
    src/Backend/ApiResult.cpp
    src/Backend/Backend.cpp
    src/Backend/Device.cpp
    src/Backend/CommandQueue.cpp
    src/Backend/CommandContext.cpp
    src/Backend/DescriptorHeap.cpp
    src/Backend/SwapChain.cpp
    src/Math/Math.cpp
    src/Resource/GeometryCpuData.cpp
    src/Resource/TextureCpuData.cpp
    src/Backend/Buffer.cpp
    src/Backend/GeometryGpuData.cpp
    src/Backend/TextureGpuData.cpp
    src/Backend/FrameResources.cpp
    src/Graph/RenderPass.cpp
    src/Graph/RenderGraph.cpp
    src/Graph/RenderPassContext.cpp
    src/Graph/GraphExecutor.cpp
    src/FrameRenderer.cpp
    src/FrameDrawContext.cpp
    src/Resource/ShaderData.cpp
    src/DrawSystem.cpp
    src/DrawCommandPreparation.cpp
    src/FallbackResources.cpp
    src/SceneOutputResources.cpp
    src/Graph/FrameTextureResolver.cpp
    src/Graph/FrameResourceBindings.cpp
    src/Backend/RenderTargetTextureGpuData.cpp
    src/Resource/RenderTargetTextureRegistry.cpp
    src/Backend/DepthTexture.cpp
    src/Backend/DrawPacket.cpp
    src/Backend/PipelineGpu.cpp
)

# ディレクトリのインクルード
target_include_directories(101Renderer
    # 自身と利用側に利用経路を公開
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    # Renderer内部でのみ使用
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/directX/d3dx12"
)

# プリプロセッサ定義
target_compile_definitions(101Renderer
    PRIVATE
        RENDERER_BUILD_DLL
        NOMINMAX
)

# ライブラリのリンク
target_link_libraries(101Renderer
    PRIVATE
        d3d12
        dxgi
        dxguid
        d3dcompiler
)

if(BUILD_TESTING)
    add_executable(RendererFrameSubmissionTests tests/FrameSubmissionTests.cpp)
    target_compile_definitions(RendererFrameSubmissionTests PRIVATE NOMINMAX)
    target_link_libraries(RendererFrameSubmissionTests PRIVATE 101Renderer user32)
    add_test(NAME Renderer.FrameSubmission COMMAND RendererFrameSubmissionTests)
endif()


# Shader compilation is an external build step, available with Engine disabled.
file(GLOB RENDERER_SDK_BIN_DIRS "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
list(SORT RENDERER_SDK_BIN_DIRS COMPARE NATURAL ORDER DESCENDING)

find_program(
    RENDERER_FXC fxc.exe PATHS
    ${RENDERER_SDK_BIN_DIRS}
    "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/x64")

if(NOT RENDERER_FXC)
    message(FATAL_ERROR "FXC is required to compile Renderer shader assets")
endif()

set(RENDERER_SHADER_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(RENDERER_SHADER_OUTPUTS "")

foreach(RENDERER_SHADER MeshVS FullscreenVS ColorPS TexturePS ExtraPS LitPS DisplayPS)
    if(RENDERER_SHADER STREQUAL "MeshVS" OR RENDERER_SHADER STREQUAL "FullscreenVS")
        set(RENDERER_SHADER_PROFILE vs_5_0)
    else()
        set(RENDERER_SHADER_PROFILE ps_5_0)
    endif()

    set(RENDERER_SHADER_OUTPUT "${RENDERER_SHADER_DIRECTORY}/${RENDERER_SHADER}.cso")

    add_custom_command(
        OUTPUT "${RENDERER_SHADER_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RENDERER_SHADER_DIRECTORY}"
        COMMAND "${RENDERER_FXC}" /nologo /T ${RENDERER_SHADER_PROFILE} /E main /Fo "${RENDERER_SHADER_OUTPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${RENDERER_SHADER}.hlsl"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${RENDERER_SHADER}.hlsl" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Common.hlsli" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/DisplayTransform.hlsli" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/MaterialParams.hlsli"
        VERBATIM)

    list(APPEND RENDERER_SHADER_OUTPUTS "${RENDERER_SHADER_OUTPUT}")
    
endforeach()

add_custom_target(RendererShaderAssets DEPENDS ${RENDERER_SHADER_OUTPUTS})
# R8 CPU contract check: use the exported API without relying on upload readiness.
if(BUILD_TESTING)
    add_executable(RendererGeometryDataTests tests/GeometryDataTests.cpp)
    target_compile_definitions(RendererGeometryDataTests PRIVATE NOMINMAX)
    target_link_libraries(RendererGeometryDataTests PRIVATE 101Renderer)
    add_test(NAME Renderer.GeometryData COMMAND RendererGeometryDataTests)
endif()

# Embed the private GPU programs so Renderer initialization has no asset path.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Common.hlsli" R8_COMMON)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/MaterialParams.hlsli" R8_MATERIAL_PARAMS)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/MeshVS.hlsl" R8_BUILTIN_MESH)
string(REPLACE "#include \"Common.hlsli\"" "${R8_COMMON}" R8_BUILTIN_MESH "${R8_BUILTIN_MESH}")
string(REPLACE "#include \"MaterialParams.hlsli\"" "${R8_MATERIAL_PARAMS}" R8_BUILTIN_MESH "${R8_BUILTIN_MESH}")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/FullscreenVS.hlsl" R8_BUILTIN_FULLSCREEN)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/DisplayTransform.hlsli" R8_DISPLAY_FUNCTIONS)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/DisplayPS.hlsl" R8_BUILTIN_DISPLAY)
string(REPLACE "#include \"DisplayTransform.hlsli\"" "${R8_DISPLAY_FUNCTIONS}" R8_BUILTIN_DISPLAY "${R8_BUILTIN_DISPLAY}")
configure_file(src/BuiltinShaderSources.h.in generated/BuiltinShaderSources.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    shaders/Common.hlsli shaders/MaterialParams.hlsli shaders/MeshVS.hlsl shaders/FullscreenVS.hlsl
    shaders/DisplayTransform.hlsli shaders/DisplayPS.hlsl)
