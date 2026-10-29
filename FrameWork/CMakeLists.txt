# Engine services and CPU scene/asset ownership. GPU objects live in 101Renderer.
file(GLOB_RECURSE FRAMEWORK_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(101Framework SHARED ${FRAMEWORK_SOURCES})
set_target_properties(101Framework PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_compile_definitions(101Framework PUBLIC NOMINMAX)
target_include_directories(101Framework PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/third_party"
    "${CMAKE_SOURCE_DIR}/third_party/ImGui"
    "${CMAKE_SOURCE_DIR}/third_party/ImGui/backends")
target_link_libraries(101Framework PUBLIC 101Renderer 101RendererAssets PRIVATE
    xaudio2 winmm ole32)
add_dependencies(101Framework RendererShaderAssets)
