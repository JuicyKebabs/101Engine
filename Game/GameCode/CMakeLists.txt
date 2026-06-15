# GameCode

file(GLOB_RECURSE GAMECODE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

set_property(DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_library(GameCode SHARED ${GAMECODE_SOURCES})
set_target_properties(GameCode PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_compile_definitions(GameCode PUBLIC NOMINMAX)

target_link_options(GameCode PRIVATE "/OPT:NOREF")

target_include_directories(GameCode PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(GameCode PUBLIC 101Framework)