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

target_include_directories(101Game PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
    ${CMAKE_CURRENT_SOURCE_DIR}/GameCode
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(101Game PRIVATE 101Framework)