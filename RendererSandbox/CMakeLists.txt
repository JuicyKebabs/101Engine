add_executable(RendererSandbox WIN32
    src/main.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../Config/Windows/101Engine.manifest"
)


target_compile_definitions(RendererSandbox
    PRIVATE
        NOMINMAX
)

target_link_libraries(RendererSandbox
    PRIVATE
        101Renderer
        user32
)

add_dependencies(RendererSandbox RendererShaderAssets)
target_compile_definitions(RendererSandbox PRIVATE R4_SHADER_DIRECTORY="${CMAKE_BINARY_DIR}/Renderer/shaders")