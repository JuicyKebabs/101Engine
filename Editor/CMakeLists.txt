# Editor

set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_demo.cpp
	${CMAKE_SOURCE_DIR}/third_party/ImGui/misc/cpp/imgui_stdlib.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/backends/imgui_impl_win32.cpp
)

file(GLOB_RECURSE EDITOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(101Editor WIN32
    ${EDITOR_SOURCES}
    ${IMGUI_SOURCES}
    ${CMAKE_SOURCE_DIR}/Config/Windows/101Engine.manifest
)

target_compile_definitions(101Editor PRIVATE
    ENGINE_BUILD_CONFIGURATION="$<CONFIG>"
)

target_include_directories(101Editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_custom_command(TARGET 101Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Debug/assimp-vc145-mtd.dll>
        $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Release/assimp-vc145-mt.dll>
        $<TARGET_FILE_DIR:101Editor>
)

add_custom_command(TARGET 101Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:101Framework>
        $<TARGET_FILE_DIR:101Editor>
)

target_link_libraries(101Editor PRIVATE 101Framework d3dcompiler shell32 ole32)
add_dependencies(101Editor GameCode)

add_custom_command(TARGET 101Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GameCode>
        $<TARGET_FILE_DIR:101Editor>
)

# Runtime shaders resolve from the executable directory, independent of cwd.
add_dependencies(101Editor RendererShaderAssets)
add_custom_command(TARGET 101Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:101Renderer> $<TARGET_FILE_DIR:101Editor>
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/Renderer/shaders" "$<TARGET_FILE_DIR:101Editor>/shaders"
)

# Editor private shader compile
set(EDITOR_SHADER_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(EDITOR_SHADER_OUTPUTS "")

foreach(EDITOR_SHADER EditorVS EditorPS SolidMaskPS AlphaMask OutlinePS Collider PickingVS PickingSolidPS PickingTexturePS)
    set(EDITOR_SHADER_ENTRY main)
    set(EDITOR_SHADER_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${EDITOR_SHADER}.hlsl")

    if(EDITOR_SHADER MATCHES "^Picking")
    set(EDITOR_SHADER_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Picking.hlsl")
    set(EDITOR_SHADER_ENTRY "${EDITOR_SHADER}")
    endif()

    if(EDITOR_SHADER STREQUAL "EditorVS" OR EDITOR_SHADER STREQUAL "PickingVS")
        set(EDITOR_SHADER_PROFILE vs_5_0)
    else()
        set(EDITOR_SHADER_PROFILE ps_5_0)
    endif()

    if(EDITOR_SHADER STREQUAL "SolidMaskPS")
        set(EDITOR_SHADER_FILE "SolidMask")
    elseif(EDITOR_SHADER STREQUAL "OutlinePS")
        set(EDITOR_SHADER_FILE "Outline")
    else()
        set(EDITOR_SHADER_FILE "${EDITOR_SHADER}")
    endif()

    set(EDITOR_SHADER_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${EDITOR_SHADER}.hlsl")
        if(EDITOR_SHADER MATCHES "^Picking")
            set(EDITOR_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Picking.hlsl")
        endif()
    set(EDITOR_SHADER_OUTPUT
        "${EDITOR_SHADER_DIRECTORY}/${EDITOR_SHADER_FILE}.cso")

    add_custom_command(
        OUTPUT "${EDITOR_SHADER_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${EDITOR_SHADER_DIRECTORY}"
        COMMAND "${RENDERER_FXC}"
            /nologo
            /T ${EDITOR_SHADER_PROFILE}
            /E ${EDITOR_SHADER_ENTRY}
            /Fo "${EDITOR_SHADER_OUTPUT}"
            "${EDITOR_SHADER_SOURCE}"
        DEPENDS
            "${EDITOR_SHADER_SOURCE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/EditorCommon.hlsli"
            "${CMAKE_SOURCE_DIR}/Renderer/shaders/Common.hlsli"
        VERBATIM
    )

    list(APPEND EDITOR_SHADER_OUTPUTS "${EDITOR_SHADER_OUTPUT}")
endforeach()

add_custom_target(EditorShaderAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:101Editor>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${EDITOR_SHADER_OUTPUTS}
        "${FRAMEWORK_SKINNING_SHADER}"
        "$<TARGET_FILE_DIR:101Editor>/shaders"
    DEPENDS ${EDITOR_SHADER_OUTPUTS}
    "${FRAMEWORK_SKINNING_SHADER}"
    VERBATIM
)

add_dependencies(EditorShaderAssets FrameworkShaderAssets)

add_dependencies(101Editor EditorShaderAssets)
