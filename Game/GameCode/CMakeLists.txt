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

# GameCode.dllは実行中にホットリロードで再ビルドされる。Debug構成では
# リンカが自動的に /DEBUG を付与し、GameCode.pdb を生成・更新しようとするが、
# デバッガがアタッチされている間はこの.pdbがロックされており、
# 再ビルド時に LNK1201 (PDB書き込み失敗) で失敗する。
# /DEBUG:NONE でリンカによる.pdb生成自体を無効化する。
# (/Z7は各.objにデバッグ情報を埋め込むが、リンカの/DEBUG自体は別途無効化が必要)
target_compile_options(GameCode PRIVATE /Z7)
target_link_options(GameCode PRIVATE "/OPT:NOREF" "/DEBUG:NONE")

target_include_directories(GameCode PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(GameCode PUBLIC 101Framework)