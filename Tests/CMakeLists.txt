add_executable(ActorPoolTests
    ActorPoolTests.cpp
)

target_include_directories(ActorPoolTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorPoolTests PRIVATE 101Framework)

add_test(NAME ActorPoolTests COMMAND ActorPoolTests)
set_tests_properties(ActorPoolTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ActorFactoryTests
    ActorFactoryTests.cpp
)

target_include_directories(ActorFactoryTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorFactoryTests PRIVATE 101Framework)

add_test(NAME ActorFactoryTests COMMAND ActorFactoryTests)
set_tests_properties(ActorFactoryTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(SceneGuidTests
    SceneGuidTests.cpp
)

target_include_directories(SceneGuidTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(SceneGuidTests PRIVATE 101Framework)

add_test(NAME SceneGuidTests COMMAND SceneGuidTests)
set_tests_properties(SceneGuidTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(SceneLoaderTests
    SceneLoaderTests.cpp
)

target_include_directories(SceneLoaderTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(SceneLoaderTests PRIVATE 101Framework)

add_test(NAME SceneLoaderTests COMMAND SceneLoaderTests)
set_tests_properties(SceneLoaderTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(EditorCommandHistoryTests
    EditorCommandHistoryTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
)

target_include_directories(EditorCommandHistoryTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
)

add_test(NAME EditorCommandHistoryTests COMMAND EditorCommandHistoryTests)
set_tests_properties(EditorCommandHistoryTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(CreateActorCommandTests
    CreateActorCommandTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/CreateActorCommand.cpp
)

target_include_directories(CreateActorCommandTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(CreateActorCommandTests PRIVATE 101Framework)

add_test(NAME CreateActorCommandTests COMMAND CreateActorCommandTests)
set_tests_properties(CreateActorCommandTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ComponentSerializationTests
    ComponentSerializationTests.cpp
)

target_include_directories(ComponentSerializationTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ComponentSerializationTests PRIVATE 101Framework)

add_test(NAME ComponentSerializationTests COMMAND ComponentSerializationTests)
set_tests_properties(ComponentSerializationTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
