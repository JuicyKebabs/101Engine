set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/backends/imgui_impl_dx12.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/backends/imgui_impl_win32.cpp
)

file(GLOB_RECURSE EDITOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(101Editor WIN32
    ${EDITOR_SOURCES}
    ${IMGUI_SOURCES}
)

add_custom_command(TARGET 101Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Debug/assimp-vc142-mtd.dll>
        $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Release/assimp-vc142-mt.dll>
        $<TARGET_FILE_DIR:101Game>
)

target_link_libraries(101Editor PRIVATE 101Framework)

target_link_options(101Editor PRIVATE /WHOLEARCHIVE:101Framework)