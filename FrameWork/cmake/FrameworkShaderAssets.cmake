# Compiling shader files owned by Framework.
set(FRAMEWORK_SHADER_DIRECTORY "${CMAKE_BINARY_DIR}/Framework/shaders")
set(FRAMEWORK_SKINNING_SHADER "${FRAMEWORK_SHADER_DIRECTORY}/SkinnedMeshVS.cso")
set(FRAMEWORK_SKINNING_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../shaders/SkinnedMeshVS.hlsl")
set(FRAMEWORK_SKINNING_COMMON "${CMAKE_SOURCE_DIR}/Renderer/shaders/Common.hlsli")

add_custom_command(
    OUTPUT "${FRAMEWORK_SKINNING_SHADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${FRAMEWORK_SHADER_DIRECTORY}"
    COMMAND "${RENDERER_FXC}" /nologo /T vs_5_0 /E main
        /Fo "${FRAMEWORK_SKINNING_SHADER}" "${FRAMEWORK_SKINNING_SOURCE}"
    DEPENDS
        "${FRAMEWORK_SKINNING_SOURCE}"
        "${FRAMEWORK_SKINNING_COMMON}"
    VERBATIM)

add_custom_target(FrameworkShaderAssets DEPENDS "${FRAMEWORK_SKINNING_SHADER}")
