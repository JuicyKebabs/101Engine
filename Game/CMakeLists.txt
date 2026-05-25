file(GLOB_RECURSE GAMECODE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/GameCode/*.cpp"
)

file(GLOB_RECURSE GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

set_property(DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/GameCode"
)

add_executable(101Game WIN32
    ${GAME_SOURCES}
    ${GAMECODE_SOURCES}
)

add_custom_command(TARGET 101Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Debug/assimp-vc142-mtd.dll>
        $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/Assimp/bin/Release/assimp-vc142-mt.dll>
        $<TARGET_FILE_DIR:101Game>
)

target_include_directories(101Game PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
    ${CMAKE_CURRENT_SOURCE_DIR}/GameCode
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(101Game PRIVATE 101Framework)

target_link_options(101Game PRIVATE /WHOLEARCHIVE:101Framework)