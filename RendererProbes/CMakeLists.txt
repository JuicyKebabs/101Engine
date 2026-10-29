# R7 implementation guide: section 4.40.
# Build the limited client-side CPU Mesh/ImGui probes separately from the Renderer core.
# They use Engine headers and ImGui, but do not link the old Engine Device or rendering backend.
# Renderer symbols are supplied by the caller's DLL or single-TU inspection implementation.

add_library(RendererIntegrationProbes STATIC
    EngineMeshBridge.cpp
    MinimalImGuiAdapter.cpp
    "${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_widgets.cpp"
)

target_include_directories(RendererIntegrationProbes PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/Renderer/include"
    "${CMAKE_SOURCE_DIR}/third_party/ImGui"
)

target_include_directories(RendererIntegrationProbes PRIVATE
    "${CMAKE_SOURCE_DIR}/FrameWork/src"
    "${CMAKE_SOURCE_DIR}/third_party/Assimp/include"
)

target_compile_definitions(RendererIntegrationProbes PUBLIC NOMINMAX)
target_link_libraries(RendererIntegrationProbes PRIVATE d3dcompiler)

# Keep Renderer symbols unresolved here: the DLL client links 101Renderer;
# the inspection executable links its single candidate implementation instead.
