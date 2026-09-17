#include "SceneBase.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Component/TransformConversion.h"
#include <exception>
#include <unordered_set>

bool SceneBase::ValidateMutationActor(const Actor* actor) const
{
	if (!actor)
	{
		return false;
	}

	if (actor->GetOwner() != this)
	{
		return false;
	}

	if (ResolveActor(actor->GetHandle()) != actor)
	{
		return false;
	}

	if (actor->IsDestroyed())
	{
		return false;
	}

	if (m_actorBatchActive)
	{
		return false;
	}

	return true;
}

bool SceneBase::CanAddChildActor(const Actor* parent) const
{
	const auto valid = ValidateMutationActor(parent);

	if (!valid)
	{
		return valid;
	}

	if (m_imprintInstances.FindMember(parent->GetHandle()))
	{
		return false;
	}

	return true;
}

bool SceneBase::CanAddRootActor() const
{
	if (m_actorBatchActive || m_isFinalized)
	{
		return false;
	}

	if (m_structurePolicy != SceneStructurePolicy::SingleRootClosedSubtree)
	{
		return true;
	}

	for (Actor* actor : GetRootActors())
	{
		if (actor && !actor->IsDestroyed())
		{
			return false;
		}
	}

	return true;
}

bool SceneBase::CanAddComponent(const Actor* actor, std::type_index type) const
{
	const auto valid = ValidateMutationActor(actor);

	if (!valid)
	{
		return valid;
	}

	if (m_imprintInstances.FindMember(actor->GetHandle()))
	{
		return false;
	}

	if (!actor->CanAddComponentLocal(type))
	{
		return false;
	}

	if (m_structurePolicy == SceneStructurePolicy::SingleRootClosedSubtree &&
		(ComponentRegistry::Get().GetNameByTypeIndex(type).empty() || !ComponentRegistry::Get().GetMetadata(type)))
	{
		return false;
	}

	if (type == typeid(Canvas))
	{
		return CanApplyUIHierarchy(actor, FindClosestCanvas(actor->GetParent()), actor, CanvasRenderMode::ScreenSpace);
	}

	return true;
}

bool SceneBase::CanRemoveComponent(const Actor* actor, const Component* component) const
{
	const auto valid = ValidateMutationActor(actor);

	if (!valid)
	{
		return valid;
	}

	if (!component || component->GetOwner() != actor)
	{
		return false;
	}

	if (component->IsDestroyed())
	{
		return false;
	}

	if (m_imprintInstances.FindMember(actor->GetHandle()))
	{
		return false;
	}

	const auto policy = ComponentRegistry::Get().GetPolicy(typeid(*component));

	if (!policy || policy->cardinality == ComponentCardinality::UniqueRequired)
	{
		return false;
	}

	if (dynamic_cast<const Canvas*>(component))
	{
		return CanApplyUIHierarchy(actor, FindClosestCanvas(actor->GetParent()), actor, std::nullopt);
	}

	return true;
}

Component* SceneBase::AddActorComponent(Actor* actor, std::unique_ptr<Component> component)
{
	if (!component)
	{
		DBG("Structural operation rejected: InvalidComponent.");
		return nullptr;
	}

	const auto type = std::type_index(typeid(*component));

	if (!CanAddComponent(actor, type))
	{
		DBG("Scene structural operation was rejected.");
		return nullptr;
	}

	return actor->AddComponentInternal(std::move(component), type);
}

bool SceneBase::RemoveActorComponent(Actor* actor, Component* component)
{
	if (!CanRemoveComponent(actor, component))
	{
		DBG("Scene structural operation was rejected.");
		return false;
	}

	StructuralMutationScope mutationScope(this);
	const bool removesCanvas = dynamic_cast<Canvas*>(component) != nullptr;
	component->m_destroyed = true;
	// Pending hierarchy components are absent from queries immediately. Recompute
	// derived state now so later mutations never observe a half-removed Canvas.
	if (removesCanvas && !ApplyUIHierarchyConstraints(actor, FindClosestCanvas(actor->GetParent())))
	{
		std::terminate();
	}

	return true;
}

bool SceneBase::CanReparent(const Actor* actor, const Actor* parent) const
{
	auto valid = ValidateMutationActor(actor);

	if (!valid)
	{
		return valid;
	}

	if (parent)
	{
		valid = ValidateMutationActor(parent);

		if (!valid)
		{
			return valid;
		}

		if (m_imprintInstances.FindMember(parent->GetHandle()))
		{
			return false;
		}
	}

	const auto* member = m_imprintInstances.FindMember(actor->GetHandle());

	if (member && member->root != actor->GetHandle())
	{
		return false;
	}

	if (m_structurePolicy == SceneStructurePolicy::SingleRootClosedSubtree &&
		(actor->GetParentHandle().IsNull() || !parent))
	{
		return false;
	}

	if (WouldCreateHierarchyCycle(actor, parent))
	{
		return false;
	}

	return CanApplyUIHierarchy(actor, FindClosestCanvas(const_cast<Actor*>(parent)));
}

bool SceneBase::CanReferenceActor(const Guid& guid) const
{
	if (m_structurePolicy != SceneStructurePolicy::SingleRootClosedSubtree || !guid.IsValid())
	{
		return true;
	}

	const Actor* actor = ResolveActor(guid);

	if (!actor || actor->GetOwner() != this || actor->IsDestroyed())
	{
		return false;
	}

	return true;
}

TransformKind SceneBase::RequiredUITransformKind(bool underCanvas, std::optional<CanvasRenderMode> canvasMode)
{
	if (underCanvas || (canvasMode && *canvasMode != CanvasRenderMode::WorldSpace))
	{
		return TransformKind::RectTransform;
	}

	return TransformKind::Transform;
}

bool SceneBase::CanApplyUIHierarchy(
	const Actor* root,
	const Canvas* governingCanvas,
	const Actor* canvasOverrideOwner,
	std::optional<CanvasRenderMode> canvasOverride) const
{
	std::vector<std::pair<const Actor*, std::optional<CanvasRenderMode>>> pending{
		{ root, governingCanvas ? std::optional(governingCanvas->GetRenderMode()) : std::nullopt } };
	while (!pending.empty())
	{
		auto [actor, inherited] = pending.back();
		pending.pop_back();
		const auto valid = ValidateMutationActor(actor);

		if (!valid)
		{
			return valid;
		}

		const auto* canvas = actor->GetComponentByClass<Canvas>();
		std::optional<CanvasRenderMode> authored;

		if (actor == canvasOverrideOwner)
		{
			authored = canvasOverride;
		}
		else if (canvas)
		{
			authored = canvas->GetAuthoredRenderMode();
		}

		const auto effective = authored ? std::optional(inherited.value_or(*authored)) : std::nullopt;
		const auto required = RequiredUITransformKind(inherited.has_value(), effective);
		const auto* transform = actor->GetComponentByClass<Transform>();

		if (!transform)
		{
			return false;
		}

		if (m_imprintInstances.FindMember(actor->GetHandle()) && TransformConversion::GetKind(*transform) != required)
		{
			return false;
		}

		for (const auto handle : actor->GetChildrenHandles())
		{
			pending.emplace_back(ResolveActor(handle), effective ? effective : inherited);
		}
	}
	return true;
}

bool SceneBase::CanCaptureOrdinarySubtree(const Actor* root) const
{
	std::vector<const Actor*> pending{ root };
	std::unordered_set<ActorHandle> visited;
	while (!pending.empty())
	{
		const Actor* actor = pending.back();
		pending.pop_back();
		const auto valid = ValidateMutationActor(actor);

		if (!valid)
		{
			return valid;
		}

		if (m_imprintInstances.FindMember(actor->GetHandle()))
		{
			return false;
		}

		if (!visited.insert(actor->GetHandle()).second)
		{
			return false;
		}

		for (const auto handle : actor->GetChildrenHandles())
		{
			pending.push_back(ResolveActor(handle));
		}
	}
	return true;
}

bool SceneBase::CanReplaceTransform(const Actor* actor, TransformKind kind) const
{
	const auto valid = ValidateMutationActor(actor);

	if (!valid)
	{
		return valid;
	}

	const auto* transform = actor->GetComponentByClass<Transform>();

	if (!transform)
	{
		return false;
	}

	if (m_imprintInstances.FindMember(actor->GetHandle()))
	{
		const std::type_index replacementType = kind == TransformKind::Transform
													? std::type_index(typeid(Transform))
													: std::type_index(typeid(RectTransform));

		if (std::type_index(typeid(*transform)) != replacementType)
		{
			return false;
		}
	}

	return true;
}

bool SceneBase::ValidateInstanceDestruction(ActorHandle root) const
{
	const auto* record = m_imprintInstances.FindInstance(root);

	if (!record || !m_imprintInstances.m_system)
	{
		return false;
	}

	if (record->destroying)
	{
		return false;
	}

	const auto* definition = m_imprintInstances.m_system->ResolveForSceneCandidate(record->imprint);

	if (!definition || record->assetGuid != m_imprintInstances.m_system->GetAssetGuid(record->imprint) ||
		record->sourceRevision != definition->GetRevision() || record->root != root ||
		record->rootId != definition->GetRootActorId() ||
		record->actors.size() != definition->GetActors().size())
	{
		return false;
	}

	const auto rootIdentity = record->actors.find(record->rootId);

	if (rootIdentity == record->actors.end() || rootIdentity->second.handle != root)
	{
		return false;
	}

	std::size_t expectedComponentCount = 0;

	for (const auto& actorDefinition : definition->GetActors())
	{
		expectedComponentCount += actorDefinition.components.size();
	}

	if (record->components.size() != expectedComponentCount)
	{
		return false;
	}

	for (const auto& actorDefinition : definition->GetActors())
	{
		Actor* actor = m_imprintInstances.ResolveActor(root, actorDefinition.id);
		auto valid = ValidateMutationActor(actor);

		if (!valid)
		{
			return valid;
		}

		const auto* member = m_imprintInstances.FindMember(actor->GetHandle());

		if (!member || member->root != root || member->objectId != actorDefinition.id)
		{
			return false;
		}

		if (actorDefinition.parentId != 0)
		{
			Actor* parent = m_imprintInstances.ResolveActor(root, actorDefinition.parentId);

			if (!parent || actor->GetParentHandle() != parent->GetHandle())
			{
				return false;
			}
		}
		else
		{
			if (actor->GetHandle() != root)
			{
				return false;
			}

			Actor* parent = actor->GetParent();

			if (parent)
			{
				valid = ValidateMutationActor(parent);

				if (!valid || m_imprintInstances.FindMember(parent->GetHandle()))
				{
					return false;
				}
			}
		}

		for (const auto childHandle : actor->GetChildrenHandles())
		{
			const auto* child = m_imprintInstances.FindMember(childHandle);

			if (!child || child->root != root)
			{
				return false;
			}
		}

		for (const auto& component : actorDefinition.components)
		{
			Component* runtime = m_imprintInstances.ResolveComponent(root, component.id);

			if (!runtime || runtime->GetOwner() != actor || runtime->IsDestroyed() ||
				std::type_index(typeid(*runtime)) != component.type)
			{
				return false;
			}
		}
	}

	return true;
}

void SceneBase::CommitInstanceDestruction(ActorHandle root)
{
	auto& record = m_imprintInstances.m_instances.at(root);
	record.destroying = true;
	record.components.clear();

	for (const auto& [id, identity] : record.actors)
	{
		ResolveActor(identity.handle)->MarkForDestruction();
		m_actorPool.Destroy(identity.handle);
	}
}

bool SceneBase::CanDestroy(ActorHandle actor, bool cascade) const
{
	if (!ResolveActor(actor))
	{
		return false;
	}

	return CanDestroy(ResolveActor(actor), cascade);
}

bool SceneBase::CanDestroy(const Actor* root, bool cascade) const
{
	if (m_structurePolicy == SceneStructurePolicy::SingleRootClosedSubtree && root &&
		root->GetOwner() == this && root->GetParentHandle().IsNull())
	{
		return false;
	}

	std::vector<const Actor*> pending{ root };
	std::unordered_set<ActorHandle> visited;
	while (!pending.empty())
	{
		const Actor* actor = pending.back();
		pending.pop_back();
		const auto valid = ValidateMutationActor(actor);

		if (!valid)
		{
			return valid;
		}

		if (!visited.insert(actor->GetHandle()).second)
		{
			return false;
		}

		if (const auto* member = m_imprintInstances.FindMember(actor->GetHandle()))
		{
			if (member->root != actor->GetHandle())
			{
				return false;
			}

			const auto instance = ValidateInstanceDestruction(member->root);

			if (!instance)
			{
				return instance;
			}

			continue;
		}

		if (cascade)
		{
			for (const auto handle : actor->GetChildrenHandles())
			{
				const Actor* child = ResolveActor(handle);

				if (child && child->IsDestroyed())
				{
					continue;
				}

				pending.push_back(child);
			}
		}

		if (!cascade)
		{
			for (const auto handle : actor->GetChildrenHandles())
			{
				const auto move = CanReparent(ResolveActor(handle), nullptr);

				if (!move)
				{
					return move;
				}
			}
		}
	}
	return true;
}

bool SceneBase::RemoveActor(Actor* root, bool cascade)
{
	if (!CanDestroy(root, cascade))
	{
		DBG("Scene structural operation was rejected.");
		return false;
	}

	if (const auto* member = m_imprintInstances.FindMember(root->GetHandle()))
	{
		return m_imprintInstances.m_system->DestroyInstance(*this, member->root);
	}

	// Gather before mutation; allocation failure cannot leave a partially marked subtree.
	std::vector<Actor*> ordinary;
	std::vector<ActorHandle> instances;
	std::vector<Actor*> pending{ root };
	while (!pending.empty())
	{
		Actor* actor = pending.back();
		pending.pop_back();

		if (const auto* member = m_imprintInstances.FindMember(actor->GetHandle()))
		{
			instances.push_back(member->root);
			continue;
		}

		ordinary.push_back(actor);

		if (cascade)
		{
			for (Actor* child : actor->GetDirectChildren())
			{
				if (!child->IsDestroyed())
				{
					pending.push_back(child);
				}
			}
		}
	}
	StructuralMutationScope mutationScope(this);

	for (auto instance : instances)
	{
		m_imprintInstances.m_system->CommitDestroyInstance(*this, instance);
	}

	for (Actor* actor : ordinary)
	{
		if (!cascade)
		{
			for (Actor* child : actor->GetDirectChildren())
			{
				if (!ReparentActorInternal(child, nullptr, /*applyUIConstraints=*/true))
				{
					std::terminate();
				}
			}
		}

		actor->MarkForDestruction();
		m_actorPool.Destroy(actor->GetHandle());
	}

	return true;
}
