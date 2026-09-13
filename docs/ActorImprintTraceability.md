# ActorImprint Final Design traceability

This audit maps the ActorImprint Final Design to the implementation delivered by ET-08 through ET-18 and the basic Document-tab follow-up in Issue #68. The source page does not render the `U-01` through `U-12` labels beside its headings, so the names below are normalized audit labels reconstructed from each ticket's explicit `従う上位設計` references and the corresponding Final Design sections. The mapping does not replace the source design or ticket acceptance criteria.

- Source: [ActorImprint Final Design](https://app.notion.com/p/3cfdef1c864e803c9484d470d9a439ad)
- Tickets: [ET-08 / #46](https://github.com/JuicyKebabs/101Engine/issues/46) through [ET-18 / #56](https://github.com/JuicyKebabs/101Engine/issues/56), plus [Issue #68](https://github.com/JuicyKebabs/101Engine/issues/68)
- Audit date: 2026-09-13

## Design-unit matrix

| Unit | Final Design responsibility | Owning tickets | Production implementation | Primary verification |
| --- | --- | --- | --- | --- |
| U-01 | Basic model and initial feature boundary: one immutable definition supplies Instance structure and defaults; the first version excludes nesting, variants and unpacking | ET-08, ET-11, ET-16, ET-17 | `ActorImprint.h`, `ActorImprintEditingContext.cpp`, Scene structural policy, workflow boundary | `ActorImprintAssetTests`, `ActorImprintEditingTests`, `ActorImprintStructuralMutationTests`, `ActorImprintWorkflowTests` |
| U-02 | ActorImprint asset format and immutable memory representation | ET-08, ET-09, ET-10, ET-12, ET-13, ET-14, ET-16 | `ActorImprintAssetDeserializer.cpp`, `ActorImprintAssetSerializer.cpp`, `ActorImprint.h`, `.imprint` catalog support | `ActorImprintAssetTests`, `ActorImprintObjectGraphTests`, `ActorImprintSystemTests` |
| U-03 | Persistent identity: AssetGUID, DefinitionRevision, LocalObjectID and non-persistent generation handles have distinct scopes | ET-08, ET-09, ET-12, ET-16 | `LocalObjectId.h`, `DefinitionRevision.h`, `ActorImprintHandle.h`, `.meta` catalog identity, editing ObjectMap | `ActorImprintAssetTests`, `ActorImprintSystemTests`, `ActorImprintEditingTests`, `ActorImprintEndToEndTests` |
| U-04 | App-owned ActorImprintSystem and public runtime generation boundary | ET-09, ET-10, ET-11, ET-13, ET-14, ET-17 | `ActorImprintSystem.h/.cpp`, `ActorImprintMaterialization.cpp`, `Game/src/App/App.cpp`, `Editor/src/Core/EditorApp.cpp` | `ActorImprintSystemTests`, `ActorImprintMaterializationTests`, `ActorImprintEndToEndTests` |
| U-05 | Shared materialization primitive and Scene-owned Instance provenance/LocalObjectID correspondence | ET-10, ET-13, ET-14, ET-17 | `ActorImprintMaterialization.cpp`, `ActorImprintInstanceRegistry.h/.cpp`, private `SceneActorBatch`, restore input | `ActorImprintDetachedActorsTests`, `ActorImprintMaterializationTests`, `ActorImprintScenePersistenceTests`, `ActorImprintReloadTests` |
| U-06 | Instance structure is a runtime Scene invariant; whole-root destruction and deferred Registry cleanup form one lifecycle | ET-11, ET-17 | `SceneStructuralMutation.cpp`, `StructuralMutationResult.h`, `SceneBase`, Registry destroying state | `ActorImprintStructuralMutationTests`, command regressions, `ActorImprintWorkflowTests` |
| U-07 | Reflection is the Property schema; reference codecs translate definition-local, Scene ActorGUID and AssetGUID forms | ET-08, ET-10, ET-12, ET-13, ET-16 | existing `ReflectionSerialization`, `ActorImprintReferenceCodec`, `AssetReferenceCodec`, registered Component metadata | `ActorImprintAssetTests`, `ActorImprintMaterializationTests`, `ActorImprintInstancePersistenceTests`, `ActorImprintEndToEndTests` |
| U-08 | Property Override is the normalized difference from defaults, addressed by LocalObjectID and JSON Pointer | ET-08, ET-12, ET-13, ET-14 | `ActorImprintPropertyOverride`, `ActorImprintInstanceSerializer.cpp`, `ActorImprintInstanceDeserializer.cpp` | `ActorImprintInstanceRecordTests`, `ActorImprintInstancePersistenceTests`, `ActorImprintReloadTests`, `ActorImprintEndToEndTests` |
| U-09 | Scene v4 separates ordinary Actors from Instance records and loads the entire Scene as an unpublished candidate | ET-10, ET-12, ET-13, ET-14, ET-15 | `SceneWriter.cpp`, `SceneLoader.cpp`, `SceneVersion.h`, `SceneManager` candidate replacement | `ActorImprintScenePersistenceTests`, `SceneLoaderTests`, `SceneManagerCandidateTests`, `ActorImprintEndToEndTests` |
| U-10 | Asset changes and DefinitionRevision reload replace the definition and every live Scene atomically at an Editor safe point | ET-09, ET-12, ET-14, ET-15, ET-16, ET-17 | generic `AssetChange`, `ActorImprintReload.cpp`, `EditorDocumentManager::ReloadActorImprint`, `EditorApp` dispatch | `ActorImprintSystemTests`, `ActorImprintReloadTests`, `EditorDocumentTests`, `ActorImprintEndToEndTests` |
| U-11 | A Document owns its WorkingScene, history, selection, viewport and dirty state; the Imprint editor uses a separate authoring Scene/ObjectMap | ET-15, ET-16, ET-17, #68 | `IEditorDocument`, `EditorDocumentManager`, `EditorDocumentWorkflow`, `DocumentTabBar`, `SceneEditorDocument`, `ActorImprintEditorDocument`, `ActorImprintEditingContext` | `EditorDocumentTests`, `ActorImprintEditingTests`, `ActorImprintEndToEndTests` |
| U-12 | ActorImprintsPanel and Editor operation routes call the existing System, policy, transaction and Document boundaries | ET-17, #68 | `ActorImprintsPanel`, `ActorImprintAssetWorkflow`, `MenuBar`, `AssetDragDropPayload`, `InstantiateActorImprintCommand`, generic `AssetPicker` | `ActorImprintWorkflowTests`, `ActorImprintEndToEndTests`, Editor startup smoke |

## End-to-end acceptance evidence

| Acceptance area | Evidence |
| --- | --- |
| Create -> Edit -> Save -> Instantiate -> Override -> Scene Save -> Load -> Reload -> Undo boundary -> Delete constraints | `ActorImprintEndToEndTests` executes the sequence through production workflows, Documents, commands, System and persistence APIs |
| Empty, minimal, hierarchy, multiple Components and same-type occurrences | Empty/invalid graphs are rejected by `ActorImprintObjectGraphTests` and `ActorImprintAssetTests`; minimal and multi-level graphs plus two same-type Components round-trip in asset and E2E tests |
| Editor and game/runtime creation of independent Instances | E2E creates two command-backed Editor Instances and one runtime Instance through `AssetReference<ActorImprint>` |
| Property-only Instance persistence | `ActorImprintStructuralMutationTests` rejects structure changes through Scene, Actor, Component, snapshot/restorer and Editor command routes; `ActorImprintInstancePersistenceTests` and E2E persist scalar, Actor and reference overrides |
| Scene v4 separation and deterministic output | `ActorImprintScenePersistenceTests` verifies record separation/provenance; E2E verifies two Instance records, JSON equality after process-restart load, and byte-identical persisted saves |
| Reference identity across restart | E2E verifies definition-local `LocalObjectID`, cross-Instance `ActorGUID` and typed `AssetGUID` references after a fresh AssetManager/System load |
| Revision migration and atomic multi-Scene reload | `ActorImprintReloadTests` covers added/removed objects, GUID migration, overrides and rollback; E2E replaces two Document-owned Scenes together and checks new defaults plus retained overrides |
| Document state isolation and visible operation routes | `EditorDocumentTests` covers per-Document history, selection, viewport, Canvas and dirty state, then drives activation/close from Document-tab intents through the production workflow; E2E verifies history is cleared only at successful Scene replacement and retained on rejected reload |
| Missing/corrupt diagnostics and work preservation | `ActorImprintReloadTests`, `ActorImprintScenePersistenceTests`, `ActorImprintEditingTests` and E2E assert typed status/error/path data while retaining published definitions, Scenes, files, dirty state and history as applicable |
| Asset deletion constraints | `ActorImprintWorkflowTests` and E2E reject deletion for an open source Document or live Instance, then remove the asset/sidecar/catalog entry after all pins are released |
| ActorImprint asset operation routes | `ActorImprintWorkflowTests` drives MenuBar Create and panel Create/Edit/Delete intents through production callbacks, verifies managed-directory catalog filtering, and injects directory-preparation failure without partial publication |

## Failure-preservation matrix

| Injected failure | Observable preservation evidence |
| --- | --- |
| Asset save replacement | `ActorImprintEditingTests::AtomicSaveFailurePreservesState` locks the destination replacement and verifies original bytes, loaded definition, notification queue and dirty state |
| Instance materialization | `ActorImprintMaterializationTests::Failures` injects Component factory, Property, reference-resolution and exception failures and verifies ActorPool, GUID map, hierarchy, Registry and slot generation |
| Whole-Scene load | `ActorImprintScenePersistenceTests::TestWholeSceneFailureSuppressesLifecycleAndReleasesPins` injects a late reference failure and verifies no lifecycle callbacks, publication or retained definition pins |
| Definition reload | `ActorImprintReloadTests::TestSceneCandidateFailureRollsBackEverything` injects a late second-Scene failure; E2E injects corrupt asset JSON and verifies both Documents, definition and history remain published |

## Architecture and dependency audit

- ActorImprint structure/default data has one persistent source: immutable `ActorImprint`. Runtime Actors remain owned by `ActorPool`; Components remain Actor-owned; Scene owns hierarchy, GUID lookup and the Instance Registry.
- AssetManager remains the only asset scanner/catalog and publishes generic `AssetChange` values. It has no ActorImprintSystem dependency and does not interpret DefinitionRevision or migrate Scenes.
- ActorImprintSystem owns loaded immutable definitions and generation Slots. Game App and EditorApp own the System and outlive their Scenes. `EngineContext` carries non-owning service pointers.
- SceneLoader orchestrates whole-Scene candidates and uses the same System materialization path. SceneWriter delegates Instance records to the ActorImprint serializer; neither duplicates Reflection Property schema.
- Reflection metadata, ComponentRegistry factories/policy, AssetReferenceCodec and ActorReferenceCodec remain shared sources. No per-Component ActorImprint serializer or second Component factory was added.
- Generic Actor and Component objects contain no ActorImprint membership or variant flag. Membership, LocalObjectID correspondence and destroying state exist only in the Scene-owned Registry. System booleans are synchronous reentrancy/transaction guards, not persisted or semantic state.
- Editor UI emits GUID/type intents and calls workflow, Document, command, Scene and System boundaries. It does not own catalog state, definition state, Instance provenance or structural rules.
- Static review found no Framework dependency on Editor code, no Editor type in ActorImprint public runtime APIs, and no second hierarchy, pool, catalog, Property schema or definition mutation route.

## Verification snapshot

- Debug `ALL_BUILD` passed on 2026-09-13 for 101Framework, GameCode, 101Game, 101Editor and all test targets.
- `ctest --test-dir build -C Debug --output-on-failure`: 46/46 passed in 11.91 seconds.
- `ActorImprintEndToEndTests`: passed; its representative Debug observation was 64 Instances of three Actors plus Scene serialization in 108 ms. This is a development observation, not a release benchmark or threshold.
- 101Editor startup smoke: the built process remained alive for five seconds after launch and was then stopped by the test harness.
- `git diff --check` passes; Git reports only repository line-ending conversion notices. Existing CMake/MSBuild warnings about the missing `Game/GameCode` dependency path remain outside ActorImprint scope.

## Explicit limits and deferred candidates

- The System follows the engine's current single-threaded Scene/resource model. Background loading, watcher threads and concurrent materialization are not implemented.
- Atomic definition save is restricted to a fixed local NTFS/ReFS destination and same-directory replacement. Power-loss durability across every filesystem/device is outside the approved contract.
- Materialization prepares unpublished candidates and copies destination indices. Its temporary memory is proportional to destination slots/index entries plus candidate objects; ET-18 recorded a Debug observation rather than imposing an unapproved performance target.
- Registered Component metadata and code must outlive loaded definitions and Scenes. App teardown and GameCode reload explicitly enforce this lifetime.
- Scene replacement invalidates borrowed Actor/Component pointers. Editor state stores GUIDs and re-resolves after replacement; callers must follow the same rule.
- Full pixel-driven Windows/D3D/ImGui interaction has no automation harness. Semantic MenuBar/panel/tab intents are exercised through their production callbacks, and the executable startup smoke covers initialization.
- Subtree conversion, automatic placement offsets, nested ActorImprints, unpacking, variants/inheritance, override visualization, advanced move/rename workflows, tab reordering/pinning/splitting, multiple OS windows and simultaneous Viewports remain separate future work.

No material unresolved design decision was found by the ET-18 audit or the Issue #68 follow-up. The implementation is a completion candidate; acceptance and Issue closure remain with the user.
