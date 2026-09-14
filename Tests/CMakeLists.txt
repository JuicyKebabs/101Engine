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

add_executable(MouseInputTests
    MouseInputTests.cpp
)

target_include_directories(MouseInputTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(MouseInputTests PRIVATE 101Framework)

add_test(NAME MouseInputTests COMMAND MouseInputTests)
set_tests_properties(MouseInputTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(SkyRendererTests
    SkyRendererTests.cpp
)

target_include_directories(SkyRendererTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(SkyRendererTests PRIVATE 101Framework)

add_test(NAME SkyRendererTests COMMAND SkyRendererTests)
set_tests_properties(SkyRendererTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(PropertyMetadataTests
    PropertyMetadataTests.cpp
)

target_include_directories(PropertyMetadataTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(PropertyMetadataTests PRIVATE 101Framework)

add_test(NAME PropertyMetadataTests COMMAND PropertyMetadataTests)
set_tests_properties(PropertyMetadataTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ReflectionSerializationTests
    ReflectionSerializationTests.cpp
)

target_include_directories(ReflectionSerializationTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ReflectionSerializationTests PRIVATE 101Framework)

add_test(NAME ReflectionSerializationTests COMMAND ReflectionSerializationTests)
set_tests_properties(ReflectionSerializationTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ActorReferenceCodecTests
    ActorReferenceCodecTests.cpp
)

target_include_directories(ActorReferenceCodecTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorReferenceCodecTests PRIVATE 101Framework)

add_test(NAME ActorReferenceCodecTests COMMAND ActorReferenceCodecTests)
set_tests_properties(ActorReferenceCodecTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(AssetReferenceCodecTests
    AssetReferenceCodecTests.cpp
)

target_include_directories(AssetReferenceCodecTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(AssetReferenceCodecTests PRIVATE 101Framework)

add_test(NAME AssetReferenceCodecTests COMMAND AssetReferenceCodecTests)
set_tests_properties(AssetReferenceCodecTests PROPERTIES
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

add_executable(SceneManagerCandidateTests
    SceneManagerCandidateTests.cpp
)

target_include_directories(SceneManagerCandidateTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(SceneManagerCandidateTests PRIVATE 101Framework)

add_test(NAME SceneManagerCandidateTests COMMAND SceneManagerCandidateTests)
set_tests_properties(SceneManagerCandidateTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(EditorCommandHistoryTests
    EditorCommandHistoryTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
)

target_include_directories(EditorCommandHistoryTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

add_test(NAME EditorCommandHistoryTests COMMAND EditorCommandHistoryTests)
set_tests_properties(EditorCommandHistoryTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(EditorDocumentTests
    EditorDocumentTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Core/CanvasEditContext.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorCameraController.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorSelection.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorViewCamera.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorViewportContext.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Document/EditorDocumentManager.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/EditorDocumentWorkflow.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Document/IEditorDocument.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Document/SceneEditorDocument.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/UI/DocumentTabBar.cpp
	${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui.cpp
	${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_draw.cpp
	${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_tables.cpp
	${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_widgets.cpp
)

target_include_directories(EditorDocumentTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(EditorDocumentTests PRIVATE 101Framework)

add_test(NAME EditorDocumentTests COMMAND EditorDocumentTests)
set_tests_properties(EditorDocumentTests PROPERTIES
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

add_executable(ActorTagEditingTests
	ActorTagEditingTests.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/ChangeActorTagCommand.cpp
)

target_include_directories(ActorTagEditingTests PRIVATE
	${CMAKE_SOURCE_DIR}/Editor/src
	${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorTagEditingTests PRIVATE 101Framework)
add_test(NAME ActorTagEditingTests COMMAND ActorTagEditingTests)
set_tests_properties(ActorTagEditingTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

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

add_executable(TransformEditCommandTests
    TransformEditCommandTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/TransformEditCommand.cpp
)

target_include_directories(TransformEditCommandTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(TransformEditCommandTests PRIVATE 101Framework)

add_test(NAME TransformEditCommandTests COMMAND TransformEditCommandTests)
set_tests_properties(TransformEditCommandTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(RectTransformEditCommandTests
    RectTransformEditCommandTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/RectTransformEditCommand.cpp
)

target_include_directories(RectTransformEditCommandTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(RectTransformEditCommandTests PRIVATE 101Framework)

add_test(NAME RectTransformEditCommandTests COMMAND RectTransformEditCommandTests)
set_tests_properties(RectTransformEditCommandTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(SceneClonerTests
    SceneClonerTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Scene/SceneCloner.cpp
)

target_include_directories(SceneClonerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(SceneClonerTests PRIVATE 101Framework)

add_test(NAME SceneClonerTests COMMAND SceneClonerTests)
set_tests_properties(SceneClonerTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ActorImprintScenePersistenceTests
    ActorImprintScenePersistenceTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Scene/SceneCloner.cpp
)

target_include_directories(ActorImprintScenePersistenceTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorImprintScenePersistenceTests PRIVATE 101Framework)

add_test(NAME ActorImprintScenePersistenceTests COMMAND ActorImprintScenePersistenceTests)
set_tests_properties(ActorImprintScenePersistenceTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(SceneEditorUpdateTests
    SceneEditorUpdateTests.cpp
)

target_include_directories(SceneEditorUpdateTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(SceneEditorUpdateTests PRIVATE 101Framework)

add_test(NAME SceneEditorUpdateTests COMMAND SceneEditorUpdateTests)
set_tests_properties(SceneEditorUpdateTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(EditorCameraControllerTests
    EditorCameraControllerTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorCameraController.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorViewCamera.cpp
)

target_include_directories(EditorCameraControllerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(EditorCameraControllerTests PRIVATE 101Framework)

add_test(NAME EditorCameraControllerTests COMMAND EditorCameraControllerTests)
set_tests_properties(EditorCameraControllerTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(ReflectionInspectorTests
    ReflectionInspectorTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/UI/Inspector/ReflectionInspector.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ImGui/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(ReflectionInspectorTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)
target_link_libraries(ReflectionInspectorTests PRIVATE 101Framework)
add_test(NAME ReflectionInspectorTests COMMAND ReflectionInspectorTests)
set_tests_properties(ReflectionInspectorTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ComponentPropertyEditCommandTests
    ComponentPropertyEditCommandTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Tools/BehaviorTemplateGenerator.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/ComponentPropertyEditCommand.cpp
)
target_include_directories(ComponentPropertyEditCommandTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/Framework/src
)
target_link_libraries(ComponentPropertyEditCommandTests PRIVATE 101Framework)
add_test(NAME ComponentPropertyEditCommandTests COMMAND ComponentPropertyEditCommandTests)
set_tests_properties(ComponentPropertyEditCommandTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintObjectGraphTests ActorImprintObjectGraphTests.cpp)
target_include_directories(ActorImprintObjectGraphTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src)
target_link_libraries(ActorImprintObjectGraphTests PRIVATE 101Framework)
add_test(NAME ActorImprintObjectGraphTests COMMAND ActorImprintObjectGraphTests)
set_tests_properties(ActorImprintObjectGraphTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintAssetTests ActorImprintAssetTests.cpp)
target_include_directories(ActorImprintAssetTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src)
target_link_libraries(ActorImprintAssetTests PRIVATE 101Framework)
add_test(NAME ActorImprintAssetTests COMMAND ActorImprintAssetTests)
set_tests_properties(ActorImprintAssetTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintSystemTests ActorImprintSystemTests.cpp)
target_include_directories(ActorImprintSystemTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src)
target_link_libraries(ActorImprintSystemTests PRIVATE 101Framework)
add_test(NAME ActorImprintSystemTests COMMAND ActorImprintSystemTests)
set_tests_properties(ActorImprintSystemTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintDetachedActorsTests ActorImprintDetachedActorsTests.cpp)
target_include_directories(ActorImprintDetachedActorsTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src)
target_link_libraries(ActorImprintDetachedActorsTests PRIVATE 101Framework)
add_test(NAME ActorImprintDetachedActorsTests COMMAND ActorImprintDetachedActorsTests)
set_tests_properties(ActorImprintDetachedActorsTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintMaterializationTests ActorImprintMaterializationTests.cpp)
target_include_directories(ActorImprintMaterializationTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src)
target_link_libraries(ActorImprintMaterializationTests PRIVATE 101Framework)
add_test(NAME ActorImprintMaterializationTests COMMAND ActorImprintMaterializationTests)
set_tests_properties(ActorImprintMaterializationTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintStructuralMutationTests
    ActorImprintStructuralMutationTests.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/CreateActorCommand.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/DeleteActorCommand.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/AddComponentCommand.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/RemoveComponentCommand.cpp
    ${CMAKE_SOURCE_DIR}/Editor/src/Command/ReparentActorCommand.cpp
)
target_include_directories(ActorImprintStructuralMutationTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src ${CMAKE_SOURCE_DIR}/Editor/src)
target_link_libraries(ActorImprintStructuralMutationTests PRIVATE 101Framework)
add_test(NAME ActorImprintStructuralMutationTests COMMAND ActorImprintStructuralMutationTests)
set_tests_properties(ActorImprintStructuralMutationTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintInstanceRecordTests ActorImprintInstanceRecordTests.cpp)
target_include_directories(ActorImprintInstanceRecordTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src)
target_link_libraries(ActorImprintInstanceRecordTests PRIVATE 101Framework)
add_test(NAME ActorImprintInstanceRecordTests COMMAND ActorImprintInstanceRecordTests)
set_tests_properties(ActorImprintInstanceRecordTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintInstancePersistenceTests ActorImprintInstancePersistenceTests.cpp)
target_include_directories(ActorImprintInstancePersistenceTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src)
target_link_libraries(ActorImprintInstancePersistenceTests PRIVATE 101Framework)
add_test(NAME ActorImprintInstancePersistenceTests COMMAND ActorImprintInstancePersistenceTests)
set_tests_properties(ActorImprintInstancePersistenceTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintReloadTests ActorImprintReloadTests.cpp)
target_include_directories(ActorImprintReloadTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src)
target_link_libraries(ActorImprintReloadTests PRIVATE 101Framework)
add_test(NAME ActorImprintReloadTests COMMAND ActorImprintReloadTests)
set_tests_properties(ActorImprintReloadTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintEditingTests
	ActorImprintEditingTests.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingContext.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingObjectMap.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingSnapshot.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/AddComponentCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/ChangeActorTagCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/ComponentPropertyEditCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/CreateActorCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/DeleteActorCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/RemoveComponentCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/ReparentActorCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/RenameActorCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/CanvasEditContext.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/AtomicFileReplacement.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorCameraController.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorSelection.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorViewCamera.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorViewportContext.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/ActorImprintEditorDocument.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/EditorDocumentManager.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/IEditorDocument.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Tag/TagManagementWorkflow.cpp
)
target_include_directories(ActorImprintEditingTests PRIVATE
	${CMAKE_SOURCE_DIR}/Editor/src
	${CMAKE_SOURCE_DIR}/Framework/src
)
target_link_libraries(ActorImprintEditingTests PRIVATE 101Framework)
add_test(NAME ActorImprintEditingTests COMMAND ActorImprintEditingTests)
set_tests_properties(ActorImprintEditingTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintWorkflowTests
	ActorImprintWorkflowTests.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintAssetWorkflow.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingContext.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingObjectMap.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingSnapshot.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/InstantiateActorImprintCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/AtomicFileReplacement.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/CanvasEditContext.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorCameraController.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorSelection.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorViewCamera.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorViewportContext.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/ActorImprintEditorDocument.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/EditorDocumentManager.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/IEditorDocument.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/UI/Inspector/AssetPicker.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/UI/ActorImprintsPanel.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/UI/EditorUI.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/UI/HierarchyPanel.cpp
	${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui.cpp
	${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_draw.cpp
	${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_tables.cpp
	${CMAKE_SOURCE_DIR}/third_party/ImGui/imgui_widgets.cpp
)
target_include_directories(ActorImprintWorkflowTests PRIVATE
	${CMAKE_SOURCE_DIR}/Editor/src
	${CMAKE_SOURCE_DIR}/Framework/src
)
target_link_libraries(ActorImprintWorkflowTests PRIVATE 101Framework)
add_test(NAME ActorImprintWorkflowTests COMMAND ActorImprintWorkflowTests)
set_tests_properties(ActorImprintWorkflowTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ActorImprintEndToEndTests
	ActorImprintEndToEndTests.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintAssetWorkflow.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingContext.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingObjectMap.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/ActorImprint/ActorImprintEditingSnapshot.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/AddComponentCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/ComponentPropertyEditCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/CreateActorCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/EditorCommandHistory.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/InstantiateActorImprintCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Command/RenameActorCommand.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/AtomicFileReplacement.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/CanvasEditContext.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorCameraController.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorSelection.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorViewCamera.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Core/EditorViewportContext.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/ActorImprintEditorDocument.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/EditorDocumentManager.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/IEditorDocument.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Document/SceneEditorDocument.cpp
)
target_include_directories(ActorImprintEndToEndTests PRIVATE
	${CMAKE_SOURCE_DIR}/Editor/src
	${CMAKE_SOURCE_DIR}/Framework/src
)
target_link_libraries(ActorImprintEndToEndTests PRIVATE 101Framework)
add_test(NAME ActorImprintEndToEndTests COMMAND ActorImprintEndToEndTests)
set_tests_properties(ActorImprintEndToEndTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(SceneAssetTests
	SceneAssetTests.cpp
	${CMAKE_SOURCE_DIR}/Editor/src/Scene/SceneAssetWorkflow.cpp
)
target_include_directories(SceneAssetTests PRIVATE
	${CMAKE_SOURCE_DIR}/Editor/src
	${CMAKE_SOURCE_DIR}/Framework/src
)
target_link_libraries(SceneAssetTests PRIVATE 101Framework)
add_test(NAME SceneAssetTests COMMAND SceneAssetTests)
set_tests_properties(SceneAssetTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(BuiltinMeshAssetTests BuiltinMeshAssetTests.cpp)
target_include_directories(BuiltinMeshAssetTests PRIVATE ${CMAKE_SOURCE_DIR}/Framework/src)
target_link_libraries(BuiltinMeshAssetTests PRIVATE 101Framework)
add_test(NAME BuiltinMeshAssetTests COMMAND BuiltinMeshAssetTests)
set_tests_properties(BuiltinMeshAssetTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
