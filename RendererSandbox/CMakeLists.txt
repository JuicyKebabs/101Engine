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
        101RendererAssets
        user32
)

set(SANDBOX_SHADER_OUTPUTS)
foreach(shader TriangleVS TrianglePS)
    if(shader STREQUAL "TriangleVS")
        set(profile vs_5_0)
    else()
        set(profile ps_5_0)
    endif()

    set(source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}.hlsl")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader}.cso")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${RENDERER_FXC}" /nologo /T ${profile} /E main /Fo "${output}" "${source}"
        DEPENDS "${source}"
        VERBATIM)
    list(APPEND SANDBOX_SHADER_OUTPUTS "${output}")
endforeach()

add_custom_target(RendererSandboxShaderAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RendererSandbox>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SANDBOX_SHADER_OUTPUTS} "$<TARGET_FILE_DIR:RendererSandbox>/shaders"
    DEPENDS ${SANDBOX_SHADER_OUTPUTS}
    VERBATIM)

add_dependencies(RendererSandbox RendererSandboxShaderAssets)

add_dependencies(RendererSandboxShaderAssets RendererShaderAssets FrameworkShaderAssets)
add_custom_command(TARGET RendererSandboxShaderAssets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FRAMEWORK_SKINNING_SHADER}"
        "${CMAKE_BINARY_DIR}/Renderer/shaders/LitPS.cso"
        "$<TARGET_FILE_DIR:RendererSandbox>/shaders"
    VERBATIM)

add_custom_command(TARGET RendererSandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Debug/assimp-vc145-mtd.dll,${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Release/assimp-vc145-mt.dll>"
        "$<TARGET_FILE_DIR:RendererSandbox>"
    VERBATIM)
