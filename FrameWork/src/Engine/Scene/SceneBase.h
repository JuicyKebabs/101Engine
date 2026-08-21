#pragma once
#include <unordered_map>
#include "Engine/Window/WindowInfo.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/ComPtr/ComPtr.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorPool.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Graphics/RenderSystem.h"
#include "Engine/Graphics/CameraSystem.h"
#include "Engine/Graphics/LightTypes.h"
#include "Engine/Physics/CollisionSystem.h"

// Forward declarations
class SceneManager;
class SceneLoader;
class Component;
class ActorSubtreeRestorer;
enum class TransformKind;


//----------------------------------------------------------------------------------------
// SceneBase class
// This class represents a base of scene
// Scene has a list of root actors and all actors in the scene
// This also has systems which are used to process the core functionalities of the scene
//----------------------------------------------------------------------------------------

// Scene base class
class SceneBase
{
public:
	static constexpr DirectX::XMFLOAT3 SKY_BOX_SIZE = { 50.0f, 50.0f, 50.0f }; // Skybox size
public:
	SceneBase();	// Constructor
	~SceneBase();	// Destructor

	// Main processing functions
	void Initialize(EngineContext& context);	// Initialization
	void PreUpdate(float deltaTime);			// Pre-update
	void Update(float deltaTime);				// Update
	void LateUpdate(float deltaTime);			// Late update
	void OnRender(								// Render
		EngineContext& context,
		const CameraInfo* overrideCameraInfo = nullptr,
		RenderViewPolicy viewPolicy = {}
		);
	void Finalize();							// Finalize

	// Add an actor to the scene
	Actor* AddRootActor(std::unique_ptr<Actor> actor);

	// Add a child actor to the scene
	// Called by Actor::AddChildActor to add a child actor to the scene
	Actor* AddChildActor(std::unique_ptr<Actor> actor, ActorHandle parentHandle);

	// Remove an actor from the scene (mark it for destruction)
	// Actual release happens at the end of the LateUpdate via ActorPool::CollectGarbage
	void RemoveActor(Actor* actor, bool cascadeToChildren = true)
	{
		if (!actor || actor->GetOwner() != this || actor->IsDestroyed()) return;

		auto children = actor->GetDirectChildren();
		if (cascadeToChildren)
		{
			// Recursively remove all children of the actor
			for (auto* child : children)
			{
				RemoveActor(child, true);
			}
		}
		else
		{
			// A surviving child must not retain a handle to a deleted parent.
			for (auto* child : children)
			{
				child->SetParentHandle(ActorHandle::Null());
			}
		}

		actor->MarkForDestruction();
		m_actorPool.Destroy(actor->GetHandle());
	}

	// Remove an actor from the scene by name
	void RemoveActor(const std::string& name)
	{
		std::vector<Actor*> targets;
		m_actorPool.ForEach([&](Actor* actor) {
			if (actor->GetName() == name)
			{
				targets.push_back(actor);
			}
			});

		for (Actor* actor : targets)
		{
			RemoveActor(actor, /*cascadeToChildren=*/true);
		}
	}

	Actor* ResolveActor(ActorHandle handle) const { return m_actorPool.Resolve(handle); }		// Resolve an actor handle to an actor pointer
	Actor* ResolveActor(const Guid& guid) const { return ResolveActor(FindActorHandle(guid)); }	// Resolve an actor GUID to an actor pointer

	const ActorPool& GetActorPool() const { return m_actorPool; }	// Get actor pool

	// Get root actors (actors without parents, owned by the scene)
	std::vector<Actor*> GetRootActors() const
	{
		std::vector<Actor*> result;
		m_actorPool.ForEach([&](Actor* actor) {
				if (actor->GetParentHandle().IsNull())
				{
					result.push_back(actor);
				}
			});
		return result;
	}

	// Get all actors in the scene (including children)
	std::vector<Actor*> GetAllActors() const
	{
		std::vector<Actor*> result;
		m_actorPool.ForEach([&](Actor* actor) { result.push_back(actor); });
		return result;
	}

	// Find an actor handle by its GUID
	ActorHandle FindActorHandle(const Guid& guid) const
	{
		auto it = m_actorGuidMap.find(guid);
		if (it == m_actorGuidMap.end())
		{
			return ActorHandle::Null();
		}

		if (!m_actorPool.IsValid(it->second))
		{
			return ActorHandle::Null();
		}

		return it->second;
	}

	// Adding given component to given actor immediately, without waiting for the next update cycle
	// Never call this from runtime game code. This function is intended for Editor commands.
	Component* AddActorComponentImmediate(
		Actor* actor,
		std::unique_ptr<Component> component,
		std::size_t occurrenceIndex
	);

	// Removing given component from given actor immediately, without waiting for the next update cycle
	// Never call this from runtime game code. This function is intended for Editor commands.
	bool RemoveActorComponentImmediate(Actor* actor, Component* component);

	// Change the parent of an actor to a new parent
	// Passing nullptr as newParent will make the actor a root actor
	bool ReparentActor(Actor* actor, Actor* newParent);

	// Set the render mode of a canvas
	// This ensure that all canvas in the hierarchy of the given actor have the same render mode as the governing canvas
	bool SetCanvasRenderMode(Canvas* canvas, CanvasRenderMode renderMode);

	// Set the reference size of a canvas
	// Mark all RectTransform in the hierarchy of the given actor as dirty to update their layout
	bool SetCanvasReferenceSize(Canvas* canvas, const Vector2& referenceSize);

	// Set the scale mode of a canvas
	bool SetCanvasScaleMode(Canvas* canvas, CanvasScaleMode scaleMode);

	// Set the match width or height of a canvas
	bool SetCanvasMatchWidthOrHeight(Canvas* canvas, float match);
	
	// Setters
	void SetDirectionalLight(const DirectionalLight& light) { m_directionalLight = light; }	// Set directional light
	void SetSceneManager(SceneManager* sceneManager) { m_pSceneManager = sceneManager; }	// Set scene manager	
	void SetViewportSize(const UINT width, const UINT height);								// Set viewport size and apply it to all affected systems and components

	// Getters
	RenderSystem* GetRenderSystem() const { return m_pRenderSystem.get(); }				// Get render system
	CameraSystem* GetCameraSystem() const { return m_pCameraSystem.get(); }				// Get camera system
	CollisionSystem* GetCollisionSystem() const { return m_pCollisionSystem.get(); }	// Get collision system
	DirectionalLight GetDirectionalLight() const { return m_directionalLight; }			// Get directional light
	Vector2 GetViewportSize() const { return m_viewportSize; }							// Get viewport size
	SceneManager* GetSceneManager() const { return m_pSceneManager; }					// Get scene manager
	EngineContext* GetEngineContext() const { return m_pEngineContext; }				// Get engine context

protected:
	DirectionalLight m_directionalLight;	// Directional light

private:
	ActorPool m_actorPool;	// Actor pool (used for allocating actors)

	std::unique_ptr<RenderSystem> m_pRenderSystem = nullptr;		// Render system
	std::unique_ptr<CameraSystem> m_pCameraSystem = nullptr;		// Camera system
	std::unique_ptr<CollisionSystem> m_pCollisionSystem = nullptr;	// Collision system

	std::unordered_map<Guid, ActorHandle> m_actorGuidMap;		// Map of actor GUIDs to actor handles (used for resolving actors by GUID)
	std::unordered_map<ActorHandle, Guid> m_actorHandleGuidMap;	// Map of actor handles to actor GUIDs (used for resolving GUIDs by actor handle)

	Vector2 m_viewportSize = Vector2::One();	// Viewport size used as the root Screen-Space UI layout size

	SceneManager* m_pSceneManager = nullptr;	// Pointer to the scene manager (used for scene switching)
	EngineContext* m_pEngineContext = nullptr;	// Pointer to the engine context (used for accessing engine systems)

private:
	friend class SceneLoader;
	friend class Actor;
	friend class ActorSubtreeRestorer;

	// Helper function for Actor registration
	Actor* RegisterActor(std::unique_ptr<Actor> actor, ActorHandle parentHandle, bool applyUIConstraints);

	// Helper function for Actor restoration (used by SceneLoader)
	Actor* RegisterRestoredActor(std::unique_ptr<Actor> actor);

	// Reapply UI hierarchy constraints when a hierarchy-sensitive
	// component is added to an Actor already registered in this Scene.
	void OnActorComponentAdded(
		Actor* actor,
		Component* component
	);

	// Check if reparenting would create a hierarchy cycle
	bool WouldCreateHierarchyCycle(const Actor* actor, const Actor* newParent) const;

	// Ensure that the actor has the required transform component (Transform or RectTransform)
	bool EnsureActorTransformKind(Actor* actor, TransformKind requiredKind);

	// Just construct the parent-child relationship without applying any UI hierarchy constraints
	// (This is for unique construction method of parent-child relationship in SceneLoader)
	bool RestoreParentRelationship(Actor* child, Actor* parent);

	// Internal function to reparent an actor, with an option to apply UI hierarchy constraints
	// (This option is for unique construction method of parent-child relationship in SceneLoader)
	bool ReparentActorInternal(Actor* actor, Actor* newParent, bool applyUIConstraints);

	// Find the topmost canvas in the hierarchy of the given actor
	// This canvas has the highest priority in determining RenderMode of the UI hierarchy which the given actor belongs to
	// Used for determining several UI-related settings which are inherited from the topmost canvas
	Canvas* FindTopmostCanvas(Actor* actor) const;

	// Find the closest canvas in the hierarchy of the given actor
	// This canvas represents the boundary that groups the given Actor as a UI
	Canvas* FindClosestCanvas(Actor* actor) const;

	// Apply UI hierarchy constraints to the given actor based on the governing canvas
	bool ApplyUIHierarchyConstraints(Actor* actor, Canvas* governingCanvas);

	// Apply UI hierarchy constraints to all actors in the scene
	bool ApplyAllUIHierarchyConstraints();

	// Helper to mark every RectTransform in the hierarchy of the given actor as dirty
	// This is used to update the layout of UI elements when the viewport size changes
	void MarkRectTransformHierarchyDirty(Actor* root);

	// Rollback Actors registered during a failed subtree restoration transaction
	// The handles must refer only to newly restored, uninitialized Actors
	// This must not be used for normal Actor removal.
	// Only use it to roll back a failed restoration transaction.
	bool RollbackRestoredActors(const std::vector<ActorHandle>& restoredHandles);
};
