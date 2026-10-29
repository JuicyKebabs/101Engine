# R7 implementation guide: section 4.34.
# Compile the guide's new GPU output owner and registry into the existing Renderer DLL.
# Shader compilation and existing target/link boundaries remain unchanged.

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
file(GLOB R4_SDK_BIN_DIRS "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
list(SORT R4_SDK_BIN_DIRS COMPARE NATURAL ORDER DESCENDING)

find_program(
    RENDERER_FXC fxc.exe PATHS
    ${R4_SDK_BIN_DIRS}
    "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/x64")

if(NOT RENDERER_FXC)
    message(FATAL_ERROR "FXC is required for the R4 sample Shader assets")
endif()

set(R4_SHADER_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(R4_SHADER_OUTPUTS "")

foreach(R4_SHADER MeshVS ColorPS TexturePS ExtraPS LitPS)
    if(R4_SHADER STREQUAL "MeshVS")
        set(R4_PROFILE vs_5_0)
    else()
        set(R4_PROFILE ps_5_0)
    endif()

    set(R4_OUTPUT "${R4_SHADER_DIRECTORY}/${R4_SHADER}.cso")

    add_custom_command(
        OUTPUT "${R4_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${R4_SHADER_DIRECTORY}"
        COMMAND "${RENDERER_FXC}" /nologo /T ${R4_PROFILE} /E main /Fo "${R4_OUTPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${R4_SHADER}.hlsl"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${R4_SHADER}.hlsl" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Common.hlsli"
        VERBATIM)

    list(APPEND R4_SHADER_OUTPUTS "${R4_OUTPUT}")
    
endforeach()

add_custom_target(RendererShaderAssets DEPENDS ${R4_SHADER_OUTPUTS})
