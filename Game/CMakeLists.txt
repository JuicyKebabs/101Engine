# Game

file(GLOB_RECURSE GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(101Game WIN32
    ${GAME_SOURCES}
    ${CMAKE_SOURCE_DIR}/Config/Windows/101Engine.manifest
)

add_custom_command(TARGET 101Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Debug/assimp-vc143-mtd.dll>
        $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Release/assimp-vc143-mt.dll>
        $<TARGET_FILE_DIR:101Game>
)

add_custom_command(TARGET 101Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:101Framework>
        $<TARGET_FILE_DIR:101Game>
)

add_custom_command(TARGET 101Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GameCode>
        $<TARGET_FILE_DIR:101Game>
)

target_include_directories(101Game PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(101Game PRIVATE 101Framework)

# Runtime shaders resolve from the executable directory, independent of cwd.
add_dependencies(101Game RendererShaderAssets)
add_custom_command(TARGET 101Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:101Renderer> $<TARGET_FILE_DIR:101Game>
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/Renderer/shaders" "$<TARGET_FILE_DIR:101Game>/shaders"
)
