# Editor

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
        $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Debug/assimp-vc143-mtd.dll>
        $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Release/assimp-vc143-mt.dll>
        $<TARGET_FILE_DIR:101Editor>
)

add_custom_command(TARGET 101Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:101Framework>
        $<TARGET_FILE_DIR:101Editor>
)

target_link_libraries(101Editor PRIVATE 101Framework)
