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

# シェーダーのソースだけを変更し、実行ファイルの再リンクが不要な場合も実行する。
add_custom_target(RendererSandboxShaderAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/Renderer/shaders"
        "$<TARGET_FILE_DIR:RendererSandbox>/shaders"
    VERBATIM
)
add_dependencies(RendererSandboxShaderAssets RendererShaderAssets)
add_dependencies(RendererSandbox RendererSandboxShaderAssets)
