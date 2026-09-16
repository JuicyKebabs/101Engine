#pragma once

#include "Engine/ActorImprint/ActorImprintInstanceRecord.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Scene/SceneBase.h"
#include "nlohmann/json_fwd.hpp"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Actor;
class EngineContext;
class ActorImprintSystem;

// Error codes that can occur during scene loading.
// These codes are used to indicate specific issues encountered while loading a scene.
enum class SceneLoadErrorCode
{
	None,
	InvalidArgument,
	FileOpenFailed,
	JsonParseFailed,
	UnsupportedVersion,
	InvalidSchema,
	InvalidActorGuid,
	DuplicateActorGuid,
	InvalidHierarchy,
	ActorDeserializationFailed,
	InstanceDeserializationFailed,
	InstanceMaterializationFailed,
	ReferenceResolutionFailed,
	UIHierarchyFailed,
	InvalidSceneSettings,
	InvalidInstanceRegistry,
};

// Options for loading a scene,
struct SceneLoadOptions
{
	// How to behave when encountering unknown properties in the scene JSON.
	UnknownPropertyPolicy unknownComponentPropertyPolicy = UnknownPropertyPolicy::Reject;
};

// Store the result of a scene load operation for outputting.
struct SceneLoadError
{
	SceneLoadErrorCode code = SceneLoadErrorCode::None;
	std::string path;      // JSON Pointer into the Scene record.
	std::string assetPath; // Requested Scene asset path, or a caller-supplied in-memory label.
	std::string message;
};

// Store the result of a scene load operation.
struct SceneLoadResult
{
	std::unique_ptr<SceneBase> scene;	// The loaded Scene, or nullptr on failure.
	SceneLoadError error;				// Error information if the load failed.

	explicit operator bool() const { return scene != nullptr; }
};

// Builds a complete Scene in private storage. Callers publish the returned Scene
// only after success, so every recoverable load failure leaves their current Scene
// and owner-specific state unchanged.
class SceneLoader
{
public:
	static SceneLoadResult LoadCandidate(const std::string& filePath, EngineContext& context);
	
	static SceneLoadResult LoadCandidate(
		const std::string& filePath,
		EngineContext& context,
		SceneLoadOptions options);

	static SceneLoadResult LoadCandidate(
		const nlohmann::json& sceneRecord,
		EngineContext& context,
		std::string assetPath = "<memory>");

	static SceneLoadResult LoadCandidate(
		const nlohmann::json& sceneRecord,
		EngineContext& context,
		std::string assetPath,
		SceneLoadOptions options);

private:
	friend class ActorImprintSystem;
	// ET-14 uses the same complete Scene validation while deferring lifecycle
	// callbacks until every live Scene candidate has succeeded.
	static SceneLoadResult LoadPreparedCandidate(
		const nlohmann::json& sceneRecord,
		EngineContext& context, 
		std::string assetPath);

	static SceneLoadResult LoadCandidateImpl(
		const nlohmann::json& sceneRecord,
		EngineContext& context, 
		std::string assetPath, 
		bool publish,
		SceneLoadOptions options);

	struct ActorLoadRecord
	{
		const nlohmann::json* actorJson = nullptr;
		Guid actorGuid;
		bool hasParent = false;
		Guid parentGuid;
		std::size_t sourceIndex = 0;
	};

	struct InstanceLoadRecord
	{
		ActorImprintSerializedInstanceRecord record;
		const nlohmann::json* sourceJson = nullptr;
		std::size_t sourceIndex = 0;
	};

	enum class ActorProvenance
	{
		Ordinary,
		ImprintRoot,
		ImprintMember,
	};

	struct ActorOrigin
	{
		ActorProvenance provenance = ActorProvenance::Ordinary;
		std::string path;
	};

	enum class VisitState
	{
		Unvisited,
		Visiting,
		Visited,
	};

	static bool BuildActorLoadRecords(
		const nlohmann::json& sceneJson, bool strictV4,
		std::vector<ActorLoadRecord>& outRecords,
		std::unordered_map<Guid, ActorOrigin>& origins, 
		SceneLoadError& error);

	static bool BuildInstanceLoadRecords(
		const nlohmann::json& sceneJson,
		std::vector<InstanceLoadRecord>& outRecords,
		std::unordered_map<Guid, ActorOrigin>& origins, 
		SceneLoadError& error);

	static bool ValidateParentReferences(
		const std::vector<ActorLoadRecord>& records,
		SceneLoadError& error);

	static bool ValidateHierarchyCycles(
		const std::vector<ActorLoadRecord>& records,
		SceneLoadError& error);

	static bool RestoreOrdinaryActors(
		const std::vector<ActorLoadRecord>& records,
		SceneBase& scene,
		SceneLoadOptions options,
		SceneLoadError& error);

	static bool RestoreOrdinaryHierarchy(
		const std::vector<ActorLoadRecord>& records,
		SceneBase& scene, SceneLoadError& error);

	static bool RestoreInstances(
		const std::vector<InstanceLoadRecord>& records,
		const std::unordered_set<Guid>& reservedSceneGuids,
		std::unordered_map<Guid, ActorOrigin>& origins,
		SceneBase& scene, SceneLoadError& error);

	static bool RestoreComponentReferences(
		const std::vector<InstanceLoadRecord>& instanceRecords,
		const std::unordered_map<Guid, ActorOrigin>& origins,
		SceneBase& scene,
		SceneLoadError& error);

	static bool ValidateSceneSettings(
		const nlohmann::json& sceneJson,
		bool strictV4,
		SceneLoadError& error);

	static bool ApplySceneSettings(
		const nlohmann::json& sceneJson,
		SceneBase& scene,
		SceneLoadError& error);

	static bool ValidateInstanceRegistry(SceneBase& scene, SceneLoadError& error);
	static void ConfigureMainCamera(SceneBase& scene, const std::vector<Actor*>& actors);
	static void ConfigureInitialSky(SceneBase& scene, const std::vector<Actor*>& actors);
};
