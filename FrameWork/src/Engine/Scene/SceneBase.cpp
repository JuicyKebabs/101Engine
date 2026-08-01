#include "SceneBase.h"
#include "SceneLoader.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/TransformConversion.h"
#include <unordered_set>
#include <cmath>

// Constructor
SceneBase::SceneBase()
{
	m_pCameraSystem = std::make_unique<CameraSystem>();
	m_pRenderSystem = std::make_unique<RenderSystem>();
	m_pCollisionSystem = std::make_unique<CollisionSystem>();
}

// Destructor
SceneBase::~SceneBase()
{
}

// Initialization
void SceneBase::Initialize(EngineContext& context)
{
	m_pEngineContext = &context;
}

// Post-update (for late update)
void SceneBase::PreUpdate(float deltaTime)
{
	m_actorPool.ForEach([deltaTime](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		actor->PreUpdate(deltaTime);
		});
}

// Update
void SceneBase::Update(float deltaTime)
{
	m_actorPool.ForEach([deltaTime](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		actor->Update(deltaTime);
		});

	// Flush transforms recursively from root actors
	m_actorPool.ForEach([this](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		if (!actor->GetParentHandle().IsNull()) return;	// only recurse from roots to maintain correct order
		actor->FlushTransform();
		});
}

// Late update
void SceneBase::LateUpdate(float deltaTime)
{
	m_actorPool.ForEach([deltaTime](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		actor->LateUpdate(deltaTime);
		});

	// Flush transforms/collider
	m_actorPool.ForEach([this](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		if (!actor->GetParentHandle().IsNull()) return;
		actor->FlushTransform();
		actor->FlushColliderTransforms();
		});

	// Check colliders
	m_pCollisionSystem->CheckColliders();

	// Update collision system
	m_pCollisionSystem->CheckCollisions();

	//Flush camera parameters to update camera information
	if (m_pCameraSystem) {
		m_pCameraSystem->Flush(deltaTime);
	}

	// ActorPool owns the final OnDestroy + release step. All hierarchy-aware
	// destruction requests have already been marked through RemoveActor().
	const auto collectedHandle =  m_actorPool.CollectGarbage();

	// Remove collected actors from the guid map
	for (const ActorHandle& handle : collectedHandle)
	{
		auto it = m_actorHandleGuidMap.find(handle);
		if (it != m_actorHandleGuidMap.end())
		{
			m_actorGuidMap.erase(it->second);
			m_actorHandleGuidMap.erase(it);
		}
	}
}

// Render
void SceneBase::OnRender(EngineContext& context, const CameraInfo* overrideCameraInfo)
{
	const auto* pCameraInfo = overrideCameraInfo ? overrideCameraInfo : m_pCameraSystem->GetCameraInfo();

	// Check if main camera exists before rendering.
	if (!pCameraInfo)
	{
		DBG("SceneBase::OnRender: No main camera set, skipping render.");		
		return;
	}

	auto cameraInfo = *pCameraInfo;	// Make a copy of the camera info to pass to the render system (in case the render system needs to modify it for sorting or other purposes)

	m_pRenderSystem->FlushRegisters();
	m_pRenderSystem->BuildFrameRenderData(cameraInfo);
	context.pRenderer->SubmitFrameRenderData(m_pRenderSystem->GetFrameRenderData());
	context.pRenderer->SubmitCameraInfo(cameraInfo);
	context.pRenderer->SubmitDirectionalLight(m_directionalLight);
}

// Finalization
void SceneBase::Finalize()
{
	m_pCollisionSystem->ClearColliders();
}

Actor* SceneBase::AddRootActor(std::unique_ptr<Actor> actor)
{
	return RegisterActor(std::move(actor), ActorHandle::Null(), /*applyUIConstraints=*/true);
}

Actor* SceneBase::AddChildActor(std::unique_ptr<Actor> actor, ActorHandle parentHandle)
{
	// Parent handle must be valid for a child actor
	if (parentHandle.IsNull()) return nullptr;

	return RegisterActor(std::move(actor), parentHandle, /*applyUIConstraints=*/true);
}

Actor* SceneBase::RegisterActor(
	std::unique_ptr<Actor> actor,
	ActorHandle parentHandle,
	bool applyUIConstraints
)
{
	if (!actor) return nullptr;

	// Ensure the actor has exactly one Transform-family component
	if (actor->CountComponentFamily(ComponentFamily::Transform) != 1)
	{
		DBG("SceneBase::RegisterActor: Actor '%s' must have exactly one Transform-family component.",actor->GetName().c_str());
		return nullptr;
	}

	Guid guid = actor->GetGuid();

	// Check if the actor's Guid is valid
	if (!guid.IsValid())
	{
		DBG("SceneBase::RegisterActor: Actor has an invalid Guid.");
		return nullptr;
	}

	// Check if the actor's Guid is already registered
	if (m_actorGuidMap.find(guid) != m_actorGuidMap.end())
	{
		DBG("SceneBase::RegisterActor: Duplicate Actor Guid: %s", guid.ToString().c_str());
		return nullptr;
	}

	// Check if the parent handle is valid (if not null)
	if (!parentHandle.IsNull() && !m_actorPool.IsValid(parentHandle))
	{
		DBG("SceneBase::RegisterActor: Parent handle is invalid.");
		return nullptr;
	}

	// Check if the parent actor is pending destruction
	if (!parentHandle.IsNull())
	{
		Actor* parent = m_actorPool.Resolve(parentHandle);
		if (!parent || parent->IsDestroyed())
		{
			DBG("SceneBase::RegisterActor: Parent is pending destruction.");
			return nullptr;
		}
	}

	// Set this scene as the owner of the actor
	actor->SetOwner(this);

	// Register the actor in the ActorPool
	const ActorHandle handle = m_actorPool.Register(std::move(actor));
	if (handle.IsNull())
	{
		DBG("SceneBase::RegisterActor: Failed to register actor in ActorPool.");
		return nullptr;
	}

	// Register handle and guid in the maps
	m_actorGuidMap.emplace(guid, handle);
	m_actorHandleGuidMap.emplace(handle, guid);

	// Resolve the actor pointer from the handle
	Actor* registered = m_actorPool.Resolve(handle);

	// Set the parent handle if provided
	if (registered && !parentHandle.IsNull())
	{
		registered->SetParentHandle(parentHandle);
	}

	// Apply UI hierarchy constraints if requested
	if (registered && applyUIConstraints)
	{
		Canvas* governingCanvas = FindTopmostCanvas(registered->GetParent());
		if (!ApplyUIHierarchyConstraints(registered, governingCanvas))
		{
			DBG("SceneBase::RegisterActor: Failed to apply UI hierarchy constraints.");
		}
	}

	return registered;
}

Actor* SceneBase::RegisterRestoredActor(std::unique_ptr<Actor> actor)
{
	return RegisterActor(std::move(actor), ActorHandle::Null(), /*applyUIConstraints=*/false);
}

bool SceneBase::ReparentActor(Actor* actor, Actor* newParent)
{
	return ReparentActorInternal(actor, newParent, /*applyUIConstraints=*/true);
}

bool SceneBase::RestoreParentRelationship(Actor* actor, Actor* newParent)
{
	return ReparentActorInternal(actor, newParent, /*applyUIConstraints=*/false);
}

bool SceneBase::ReparentActorInternal(Actor* actor, Actor* newParent, bool applyUIConstraints)
{
	if (!actor || actor->GetOwner() != this)
	{
		DBG("SceneBase::ReparentActor: Actor does not belong to this scene.");
		return false;
	}

	if (actor->IsDestroyed())
	{
		DBG("SceneBase::ReparentActor: Actor '%s' is pending destruction.", actor->GetName().c_str());
		return false;
	}

	if (newParent)
	{
		if (newParent->GetOwner() != this)
		{
			DBG("SceneBase::ReparentActor: New parent does not belong to this scene.");
			return false;
		}

		if (newParent->IsDestroyed())
		{
			DBG("SceneBase::ReparentActor: New parent '%s' is pending destruction.", newParent->GetName().c_str());
			return false;
		}

		if (newParent == actor)
		{
			DBG("SceneBase::ReparentActor: Actor cannot be parented to itself.");
			return false;
		}
	}

	// Get handle of the new parent (or null if newParent is nullptr)
	const ActorHandle newParentHandle = newParent ? newParent->GetHandle() : ActorHandle::Null();

	if (actor->GetParentHandle() == newParentHandle) return true; // No change needed

	// Check for hierarchy cycle
	if (WouldCreateHierarchyCycle(actor, newParent))
	{
		DBG("SceneBase::ReparentActor: Reparenting Actor '%s' would create a hierarchy cycle.", actor->GetName().c_str());
		return false;
	}

	// Update the parent handle of the actor
	actor->SetParentHandle(newParentHandle);

	// Apply UI hierarchy constraints if requested
	if (applyUIConstraints)
	{
		Canvas* governingCanvas = FindTopmostCanvas(newParent);

		if (!ApplyUIHierarchyConstraints(actor, governingCanvas))
		{
			DBG("SceneBase::ReparentActor: Failed to apply UI hierarchy constraints for Actor '%s'.", actor->GetName().c_str());
			return false;
		}
	}

	return true;
}

bool SceneBase::SetCanvasRenderMode(Canvas* canvas, CanvasRenderMode renderMode)
{
	if (!canvas)
	{
		DBG("SceneBase::SetCanvasRenderMode: Canvas is null.");
		return false;
	}

	const int modeValue = static_cast<int>(renderMode);

	if (modeValue < 0 || modeValue >= static_cast<int>(CanvasRenderMode::Max))
	{
		DBG("SceneBase::SetCanvasRenderMode: Invalid render mode.");
		return false;
	}

	Actor* owner = canvas->GetOwner();

	if (!owner || owner->GetOwner() != this)
	{
		DBG("SceneBase::SetCanvasRenderMode: Canvas does not belong to an actor in this scene.");
		return false;
	}

	if (owner->IsDestroyed())
	{
		DBG("SceneBase::SetCanvasRenderMode: Canvas owner '%s' is pending destruction.", owner->GetName().c_str());
		return false;
	}

	Canvas* topmostCanvas = FindTopmostCanvas(owner);

	if (topmostCanvas != canvas)
	{
		DBG("SceneBase::SetCanvasRenderMode: Nested Canvas '%s' inherits its topmost Canvas render mode.", owner->GetName().c_str());
		return false;
	}

	// Set the render mode of the topmost canvas
	canvas->SetRenderMode(renderMode);

	// Apply UI hierarchy constraints
	if (!ApplyUIHierarchyConstraints(owner, nullptr))
	{
		DBG("SceneBase::SetCanvasRenderMode: Failed to apply UI hierarchy constraints.");
		return false;
	}

	return true;
}

bool SceneBase::SetCanvasReferenceSize(Canvas* canvas, const Vector2& referenceSize)
{
	if (!canvas)
	{
		DBG("SceneBase::SetCanvasReferenceSize: Canvas is null.");
		return false;
	}

	if (!std::isfinite(referenceSize.x) ||
		!std::isfinite(referenceSize.y) ||
		referenceSize.x <= 0.0f ||
		referenceSize.y <= 0.0f)
	{
		DBG("SceneBase::SetCanvasReferenceSize : Invalid reference size.");
		return false;
	}

	Actor* owner = canvas->GetOwner();
	if (!owner || owner->GetOwner() != this)
	{
		DBG("SceneBase::SetCanvasReferenceSize: Canvas does not belong to an actor in this scene.");
		return false;
	}

	if (owner->IsDestroyed())
	{
		DBG("SceneBase::SetCanvasReferenceSize: Canvas owner '%s' is pending destruction.", owner->GetName().c_str());
		return false;
	}

	// Set the reference size of the canvas
	canvas->SetWorldReferenceSize(referenceSize);

	// Mark all RectTransform under this canvas as dirty 
	// to update their layout based on the new reference size
	MarkRectTransformHierarchyDirty(owner);
	
	return true;
}

void SceneBase::SetViewportSize(UINT width, UINT height)
{
	if (width == 0 || height == 0) return;

	Vector2 newSize(static_cast<float>(width), static_cast<float>(height));

	if (m_viewportSize == newSize) return;

	// Update the viewport size
	m_viewportSize = newSize;

	for (Actor* actor : GetAllActors())
	{
		if (!actor || actor->IsDestroyed()) continue;

		Canvas* canvas = actor->GetComponentByClass<Canvas>();

		// Find the topmost canvas whose render mode is screen-space
		if (!canvas) continue;
		if (FindTopmostCanvas(actor) != canvas) continue;
		if (canvas->GetRenderMode() != CanvasRenderMode::ScreenSpace) continue;

		// Mark all RectTransform under this canvas as dirty 
		// to update their layout based on the new viewport size
		MarkRectTransformHierarchyDirty(actor);
	}
}

bool SceneBase::WouldCreateHierarchyCycle(const Actor* actor, const Actor* newParent) const
{
	if (!actor || !newParent) return false;

	std::unordered_set<const Actor*> visited;	// Collection of the actors which have been already visited in the traversal.

	// Traverse up the hierarchy from the new parent to check for cycles
	for (const Actor* current = newParent; current; current = current->GetParent())
	{
		// Reaching the target actor means the proposed parent is its descendant
		if (current == actor) return true;

		// Re-visiting an ancestor means the existing hierarchy is already cyclic
		if (!visited.insert(current).second) return true;
	}

	return false;
}

bool SceneBase::EnsureActorTransformKind(Actor* actor, TransformKind requiredKind)
{
	if (!actor || actor->GetOwner() != this)
	{
		DBG("SceneBase::EnsureActorTransformKind: Actor does not belong to this scene.");
		return false;
	}

	if (actor->IsDestroyed())
	{
		DBG("SceneBase::EnsureActorTransformKind: Actor '%s' is pending destruction.", actor->GetName().c_str());
		return false;
	}

	// Get the current transform from the actor
	Transform* currentTransform = actor->GetComponentByClass<Transform>();

	if (!currentTransform)
	{
		DBG("SceneBase::EnsureActorTransformKind: Actor '%s' has no Transform-family component.", actor->GetName().c_str());
		return false;
	}

	// Skip when trying to convert to the same kind
	if (TransformConversion::GetKind(*currentTransform) == requiredKind)
	{
		return true;
	}

	// Build a chain of transforms from the root to the target actor
	std::vector<Transform*> transformChain;
	for (Actor* current = actor; current; current = current->GetParent())
	{
		Transform* transform = current->GetComponentByClass<Transform>();

		if (!transform)
		{
			DBG("SceneBase::EnsureActorTransformKind: Actor '%s' has no Transform-family component.", current->GetName().c_str());
			return false;
		}

		transformChain.push_back(transform);
	}

	// Update from the root toward the target actor
	for (auto it = transformChain.rbegin(); it != transformChain.rend(); ++it)
	{
		(*it)->UpdateGeometry();
	}

	// Capture the snapshot of the current transform
	const TransformSnapshot snapshot = TransformConversion::Capture(*currentTransform);

	// Create a new transform of the required kind from the snapshot
	std::unique_ptr<Transform> converted = TransformConversion::Create(requiredKind, snapshot);

	if (!converted)
	{// In case of conversion fails
		DBG("SceneBase::EnsureActorTransformKind: Failed to convert Actor '%s' transform.", actor->GetName().c_str());
		return false;
	}

	// Try to replace with the new transform in the actor
	if (!actor->ReplaceTransformComponent(std::move(converted)))
	{
		DBG("SceneBase::EnsureActorTransformKind: Failed to replace Actor '%s' transform.", actor->GetName().c_str());
		return false;
	}

	return true;
}

Canvas* SceneBase::FindTopmostCanvas(Actor* actor) const
{
	Canvas* topmostCanvas = nullptr;

	// Traverse up the hierarchy from the actor to find the topmost Canvas component
	for (Actor* current = actor; current; current = current->GetParent())
	{
		Canvas* canvas = current->GetComponentByClass<Canvas>();
		if (canvas)
		{
			topmostCanvas = canvas;
		}
	}

	return topmostCanvas;
}

bool SceneBase::ApplyUIHierarchyConstraints(Actor* actor, Canvas* governingCanvas)
{
	if (!actor) return false;

	Canvas* actorCanvas = actor->GetComponentByClass<Canvas>();

	// Whether the Actor is a descendant of a governing Canvas
	const bool isUnderCanvas = (governingCanvas != nullptr);

	// Check if tht given actor is a root canvas
	if (actorCanvas)
	{// In case of the given actor has a canvas
		// Inherit render mode from the governing canvas if it exists
		if (governingCanvas) actorCanvas->SetRenderMode(governingCanvas->GetRenderMode());
	}

	TransformKind requiredKind = TransformKind::Transform;

	if (isUnderCanvas)
	{// All actors under a canvas must have RectTransform
		requiredKind = TransformKind::RectTransform;
	}
	else if (actorCanvas)
	{// In case of the given actor has a root canvas
		requiredKind = actorCanvas->GetRenderMode()
			== CanvasRenderMode::WorldSpace
			? TransformKind::Transform			// WorldSpace canvas uses normal Transform
			: TransformKind::RectTransform;		// ScreenSpace canvas uses RectTransform
	}

	// Ensure the actor has the required transform kind
	if (!EnsureActorTransformKind(actor, requiredKind))
	{
		DBG("SceneBase::ApplyUIHierarchyConstraints: Failed to ensure transform kind for Actor '%s'.", actor->GetName().c_str());
		return false;
	}

	Canvas* childGoverningCanvas = governingCanvas;

	if (!childGoverningCanvas && actorCanvas)
	{// In case of the given actor is a root canvas
		// Set the given actor's canvas as the governing canvas for its children
		childGoverningCanvas = actorCanvas;
	}

	// Recursively apply UI hierarchy constraints to all child actors
	for (Actor* child : actor->GetDirectChildren())
	{
		if (!ApplyUIHierarchyConstraints(child, childGoverningCanvas))
		{
			return false;
		}
	}

	return true;
}

bool SceneBase::ApplyAllUIHierarchyConstraints()
{
	for (Actor* root : GetRootActors())
	{
		if (!ApplyUIHierarchyConstraints(root, nullptr))
		{
			return false;
		}
	}

	return true;
}

void SceneBase::MarkRectTransformHierarchyDirty(Actor* root)
{
	if (!root) return;

	// Mark the RectTransform of the root actor as dirty if it exists
	if (RectTransform* rectTransform = root->GetComponentByClass<RectTransform>())
	{
		rectTransform->MarkDirty();
	}

	// Mark children dirty recursively
	for (Actor* child : root->GetDirectChildren())
	{
		MarkRectTransformHierarchyDirty(child);
	}
}