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
)

if(BUILD_TESTING)
    add_executable(RendererFrameSubmissionTests tests/FrameSubmissionTests.cpp)
    target_compile_definitions(RendererFrameSubmissionTests PRIVATE NOMINMAX)
    target_link_libraries(RendererFrameSubmissionTests PRIVATE 101Renderer user32)
    add_test(NAME Renderer.FrameSubmission COMMAND RendererFrameSubmissionTests)
endif()
