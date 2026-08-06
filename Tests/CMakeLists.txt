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

add_executable(RenameActorCommandTests
    RenameActorCommandTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/RenameActorCommand.cpp
)

target_include_directories(RenameActorCommandTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(RenameActorCommandTests PRIVATE 101Framework)

add_test(NAME RenameActorCommandTests COMMAND RenameActorCommandTests)
set_tests_properties(RenameActorCommandTests PROPERTIES
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

add_executable(ActorSubtreeSnapshotTests
    ActorSubtreeSnapshotTests.cpp
)

target_include_directories(ActorSubtreeSnapshotTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorSubtreeSnapshotTests PRIVATE 101Framework)

add_test(NAME ActorSubtreeSnapshotTests COMMAND ActorSubtreeSnapshotTests)
set_tests_properties(ActorSubtreeSnapshotTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ActorDeserializerTests
    ActorDeserializerTests.cpp
)

target_include_directories(ActorDeserializerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorDeserializerTests PRIVATE 101Framework)

add_test(NAME ActorDeserializerTests COMMAND ActorDeserializerTests)
set_tests_properties(ActorDeserializerTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ActorSubtreeRestorerTests
    ActorSubtreeRestorerTests.cpp
)

target_include_directories(ActorSubtreeRestorerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorSubtreeRestorerTests PRIVATE 101Framework)

add_test(NAME ActorSubtreeRestorerTests COMMAND ActorSubtreeRestorerTests)
set_tests_properties(ActorSubtreeRestorerTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ActorReferenceTests
    ActorReferenceTests.cpp
)

target_include_directories(ActorReferenceTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorReferenceTests PRIVATE 101Framework)

add_test(NAME ActorReferenceTests COMMAND ActorReferenceTests)
set_tests_properties(ActorReferenceTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(CameraActorReferenceTests
    CameraActorReferenceTests.cpp
)

target_include_directories(CameraActorReferenceTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(CameraActorReferenceTests PRIVATE 101Framework)

add_test(NAME CameraActorReferenceTests COMMAND CameraActorReferenceTests)
set_tests_properties(CameraActorReferenceTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(DeleteActorCommandTests
    DeleteActorCommandTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/DeleteActorCommand.cpp
)

target_include_directories(DeleteActorCommandTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(DeleteActorCommandTests PRIVATE 101Framework)

add_test(NAME DeleteActorCommandTests COMMAND DeleteActorCommandTests)
set_tests_properties(DeleteActorCommandTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(TransformSubtreeSnapshotTests
    TransformSubtreeSnapshotTests.cpp
)

target_include_directories(TransformSubtreeSnapshotTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(TransformSubtreeSnapshotTests PRIVATE 101Framework)

add_test(NAME TransformSubtreeSnapshotTests COMMAND TransformSubtreeSnapshotTests)
set_tests_properties(TransformSubtreeSnapshotTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ReparentActorCommandTests
    ReparentActorCommandTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/ReparentActorCommand.cpp
)

target_include_directories(ReparentActorCommandTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ReparentActorCommandTests PRIVATE 101Framework)

add_test(NAME ReparentActorCommandTests COMMAND ReparentActorCommandTests)
set_tests_properties(ReparentActorCommandTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(UIHierarchyConstraintTests
    UIHierarchyConstraintTests.cpp
)

target_include_directories(UIHierarchyConstraintTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(UIHierarchyConstraintTests PRIVATE 101Framework)

add_test(NAME UIHierarchyConstraintTests COMMAND UIHierarchyConstraintTests)
set_tests_properties(UIHierarchyConstraintTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ComponentSnapshotTests
    ComponentSnapshotTests.cpp
)

target_include_directories(ComponentSnapshotTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ComponentSnapshotTests PRIVATE 101Framework)

add_test(NAME ComponentSnapshotTests COMMAND ComponentSnapshotTests)
set_tests_properties(ComponentSnapshotTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ComponentImmediateMutationTests
    ComponentImmediateMutationTests.cpp
)

target_include_directories(ComponentImmediateMutationTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ComponentImmediateMutationTests PRIVATE 101Framework)

add_test(NAME ComponentImmediateMutationTests COMMAND ComponentImmediateMutationTests)
set_tests_properties(ComponentImmediateMutationTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ComponentCommandTests
    ComponentCommandTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/AddComponentCommand.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/RemoveComponentCommand.cpp
)

target_include_directories(ComponentCommandTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ComponentCommandTests PRIVATE 101Framework)

add_test(NAME ComponentCommandTests COMMAND ComponentCommandTests)
set_tests_properties(ComponentCommandTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
