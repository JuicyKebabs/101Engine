#include "SceneBase.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Component/TransformConversion.h"
#include <exception>
#include <unordered_set>

using Reason = StructuralMutationReason;

StructuralMutationResult SceneBase::ValidateMutationActor(const Actor* actor) const
{
	if (!actor) return { Reason::InvalidActor };
	if (actor->GetOwner() != this) return { Reason::ForeignScene };
	if (ResolveActor(actor->GetHandle()) != actor) return { Reason::StaleHandle };
	if (actor->IsDestroyed()) return { Reason::PendingDestroy };
	if (m_actorBatchActive) return { Reason::TransactionInProgress };
	return {};
}

StructuralMutationResult SceneBase::CanAddChildActor(const Actor* parent) const
{
	const auto valid = ValidateMutationActor(parent);
	if (!valid) return valid;
	if (m_imprintInstances.FindMember(parent->GetHandle())) return { Reason::ImprintMemberImmutable };
	return {};
}

StructuralMutationResult SceneBase::CanAddRootActor() const
{
	if (m_actorBatchActive || m_isFinalized) return { Reason::TransactionInProgress };
	if (m_structurePolicy != SceneStructurePolicy::SingleRootClosedSubtree) return {};
	for (Actor* actor : GetRootActors())
		if (actor && !actor->IsDestroyed()) return { Reason::SingleRootInvariant };
	return {};
}

StructuralMutationResult SceneBase::CanAddComponent(const Actor* actor, std::type_index type) const
{
	const auto valid = ValidateMutationActor(actor);
	if (!valid) return valid;
	if (m_imprintInstances.FindMember(actor->GetHandle())) return { Reason::ImprintMemberImmutable };
	if (!actor->CanAddComponentLocal(type)) return { Reason::ComponentPolicyViolation };
	if (m_structurePolicy == SceneStructurePolicy::SingleRootClosedSubtree &&
		(ComponentRegistry::Get().GetNameByTypeIndex(type).empty() || !ComponentRegistry::Get().GetMetadata(type)))
		return { Reason::ComponentPolicyViolation };
	if (type == typeid(Canvas)) return CanApplyUIHierarchy(actor, FindClosestCanvas(actor->GetParent()), actor, CanvasRenderMode::ScreenSpace);
	return {};
}

StructuralMutationResult SceneBase::CanRemoveComponent(const Actor* actor, const Component* component) const
{
	const auto valid = ValidateMutationActor(actor);
	if (!valid) return valid;
	if (!component || component->GetOwner() != actor) return { Reason::InvalidComponent };
	if (component->IsDestroyed()) return { Reason::PendingDestroy };
	if (m_imprintInstances.FindMember(actor->GetHandle())) return { Reason::ImprintMemberImmutable };
	const auto policy = ComponentRegistry::Get().GetPolicy(typeid(*component));
	if (!policy || policy->cardinality == ComponentCardinality::UniqueRequired) return { Reason::ComponentPolicyViolation };
	if (dynamic_cast<const Canvas*>(component)) return CanApplyUIHierarchy(actor, FindClosestCanvas(actor->GetParent()), actor, std::nullopt);
	return {};
}

Component* SceneBase::AddActorComponent(Actor* actor, std::unique_ptr<Component> component, StructuralMutationResult* result)
{
	if (!component) { StructuralMutationResult{ Reason::InvalidComponent }.Report(result); return nullptr; }
	const auto type = std::type_index(typeid(*component));
	if (!CanAddComponent(actor, type).Report(result)) return nullptr;
	return actor->AddComponentInternal(std::move(component), type);
}

bool SceneBase::RemoveActorComponent(Actor* actor, Component* component, StructuralMutationResult* result)
{
	if (!CanRemoveComponent(actor, component).Report(result)) return false;
	StructuralMutationScope mutationScope(this);
	const bool removesCanvas = dynamic_cast<Canvas*>(component) != nullptr;
	component->m_destroyed = true;
	// Pending hierarchy components are absent from queries immediately. Recompute
	// derived state now so later mutations never observe a half-removed Canvas.
	if (removesCanvas && !ApplyUIHierarchyConstraints(actor, FindClosestCanvas(actor->GetParent())))
		std::terminate();
	return true;
}

StructuralMutationResult SceneBase::CanReparent(const Actor* actor, const Actor* parent) const
{
	auto valid = ValidateMutationActor(actor);
	if (!valid) return valid;
	if (parent)
	{
		valid = ValidateMutationActor(parent);
		if (!valid) return valid;
		if (m_imprintInstances.FindMember(parent->GetHandle())) return { Reason::ImprintMemberImmutable };
	}
	const auto* member = m_imprintInstances.FindMember(actor->GetHandle());
	if (member && member->root != actor->GetHandle()) return { Reason::ImprintMemberImmutable };
	if (m_structurePolicy == SceneStructurePolicy::SingleRootClosedSubtree &&
		(actor->GetParentHandle().IsNull() || !parent)) return { Reason::SingleRootInvariant };
	if (WouldCreateHierarchyCycle(actor, parent)) return { Reason::HierarchyCycle };
	return CanApplyUIHierarchy(actor, FindClosestCanvas(const_cast<Actor*>(parent)));
}

StructuralMutationResult SceneBase::CanReferenceActor(const Guid& guid) const
{
	if (m_structurePolicy != SceneStructurePolicy::SingleRootClosedSubtree || !guid.IsValid()) return {};
	const Actor* actor = ResolveActor(guid);
	if (!actor || actor->GetOwner() != this || actor->IsDestroyed()) return { Reason::ExternalActorReference };
	return {};
}

TransformKind SceneBase::RequiredUITransformKind(bool underCanvas, std::optional<CanvasRenderMode> canvasMode)
{
	return underCanvas || (canvasMode && *canvasMode != CanvasRenderMode::WorldSpace)
		? TransformKind::RectTransform : TransformKind::Transform;
}

StructuralMutationResult SceneBase::CanApplyUIHierarchy(const Actor* root, const Canvas* governingCanvas,
	const Actor* canvasOverrideOwner, std::optional<CanvasRenderMode> canvasOverride) const
{
	std::vector<std::pair<const Actor*, std::optional<CanvasRenderMode>>> pending{
		{ root, governingCanvas ? std::optional(governingCanvas->GetRenderMode()) : std::nullopt } };
	while (!pending.empty())
	{
		auto [actor, inherited] = pending.back(); pending.pop_back();
		const auto valid = ValidateMutationActor(actor);
		if (!valid) return valid;
		const auto* canvas = actor->GetComponentByClass<Canvas>();
		auto authored = actor == canvasOverrideOwner ? canvasOverride :
			(canvas ? std::optional(canvas->GetAuthoredRenderMode()) : std::nullopt);
		const auto effective = authored ? std::optional(inherited.value_or(*authored)) : std::nullopt;
		const auto required = RequiredUITransformKind(inherited.has_value(), effective);
		const auto* transform = actor->GetComponentByClass<Transform>();
		if (!transform) return { Reason::ComponentPolicyViolation };
		if (m_imprintInstances.FindMember(actor->GetHandle()) && TransformConversion::GetKind(*transform) != required)
			return { Reason::UIConstraintViolation };
		for (const auto handle : actor->GetChildrenHandles()) pending.emplace_back(ResolveActor(handle), effective ? effective : inherited);
	}
	return {};
}

StructuralMutationResult SceneBase::CanCaptureOrdinarySubtree(const Actor* root) const
{
	std::vector<const Actor*> pending{ root };
	std::unordered_set<ActorHandle> visited;
	while (!pending.empty())
	{
		const Actor* actor = pending.back(); pending.pop_back();
		const auto valid = ValidateMutationActor(actor);
		if (!valid) return valid;
		if (m_imprintInstances.FindMember(actor->GetHandle())) return { Reason::InstanceSnapshotRequired };
		if (!visited.insert(actor->GetHandle()).second) return { Reason::HierarchyCycle };
		for (const auto handle : actor->GetChildrenHandles()) pending.push_back(ResolveActor(handle));
	}
	return {};
}

StructuralMutationResult SceneBase::CanReplaceTransform(const Actor* actor, TransformKind kind) const
{
	const auto valid = ValidateMutationActor(actor);
	if (!valid) return valid;
	const auto* transform = actor->GetComponentByClass<Transform>();
	if (!transform) return { Reason::ComponentPolicyViolation };
	if (m_imprintInstances.FindMember(actor->GetHandle()))
	{
		const std::type_index replacementType = kind == TransformKind::Transform
			? std::type_index(typeid(Transform)) : std::type_index(typeid(RectTransform));
		if (std::type_index(typeid(*transform)) != replacementType)
			return { Reason::ImprintMemberImmutable };
	}
	return {};
}

StructuralMutationResult SceneBase::ValidateInstanceDestruction(ActorHandle root) const
{
	const auto* record = m_imprintInstances.FindInstance(root);
	if (!record || !m_imprintInstances.m_system) return { Reason::InvalidInstance };
	if (record->destroying) return { Reason::PendingDestroy };
	const auto* definition = m_imprintInstances.m_system->ResolveForSceneCandidate(record->imprint);
	if (!definition || record->assetGuid != m_imprintInstances.m_system->GetAssetGuid(record->imprint) ||
		record->sourceRevision != definition->GetRevision() || record->root != root ||
		record->rootId != definition->GetRootActorId() ||
		record->actors.size() != definition->GetActors().size())
		return { Reason::InvalidInstance };
	const auto rootIdentity = record->actors.find(record->rootId);
	if (rootIdentity == record->actors.end() || rootIdentity->second.handle != root)
		return { Reason::InvalidInstance };
	std::size_t expectedComponentCount = 0;
	for (const auto& actorDefinition : definition->GetActors())
		expectedComponentCount += actorDefinition.components.size();
	if (record->components.size() != expectedComponentCount) return { Reason::InvalidInstance };
	for (const auto& actorDefinition : definition->GetActors())
	{
		Actor* actor = m_imprintInstances.ResolveActor(root, actorDefinition.id);
		auto valid = ValidateMutationActor(actor);
		if (!valid) return valid;
		const auto* member = m_imprintInstances.FindMember(actor->GetHandle());
		if (!member || member->root != root || member->objectId != actorDefinition.id) return { Reason::InvalidInstance };
		if (actorDefinition.parentId != 0)
		{
			Actor* parent = m_imprintInstances.ResolveActor(root, actorDefinition.parentId);
			if (!parent || actor->GetParentHandle() != parent->GetHandle()) return { Reason::InvalidInstance };
		}
		else
		{
			if (actor->GetHandle() != root) return { Reason::InvalidInstance };
			Actor* parent = actor->GetParent();
			if (parent)
			{
				valid = ValidateMutationActor(parent);
				if (!valid || m_imprintInstances.FindMember(parent->GetHandle())) return { Reason::InvalidInstance };
			}
		}
		for (const auto childHandle : actor->GetChildrenHandles())
		{
			const auto* child = m_imprintInstances.FindMember(childHandle);
			if (!child || child->root != root) return { Reason::InvalidInstance };
		}
		for (const auto& component : actorDefinition.components)
		{
			Component* runtime = m_imprintInstances.ResolveComponent(root, component.id);
			if (!runtime || runtime->GetOwner() != actor || runtime->IsDestroyed() ||
				std::type_index(typeid(*runtime)) != component.type)
				return { Reason::InvalidInstance };
		}
	}
	return {};
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

StructuralMutationResult SceneBase::CanDestroy(ActorHandle actor, bool cascade) const
{
	if (!ResolveActor(actor)) return { Reason::StaleHandle };
	return CanDestroy(ResolveActor(actor), cascade);
}

StructuralMutationResult SceneBase::CanDestroy(const Actor* root, bool cascade) const
{
	if (m_structurePolicy == SceneStructurePolicy::SingleRootClosedSubtree && root &&
		root->GetOwner() == this && root->GetParentHandle().IsNull())
		return { Reason::SingleRootInvariant };
	std::vector<const Actor*> pending{ root };
	std::unordered_set<ActorHandle> visited;
	while (!pending.empty())
	{
		const Actor* actor = pending.back(); pending.pop_back();
		const auto valid = ValidateMutationActor(actor);
		if (!valid) return valid;
		if (!visited.insert(actor->GetHandle()).second) return { Reason::HierarchyCycle };
		if (const auto* member = m_imprintInstances.FindMember(actor->GetHandle()))
		{
			if (member->root != actor->GetHandle()) return { Reason::InstanceDestroyRequired };
			const auto instance = ValidateInstanceDestruction(member->root);
			if (!instance) return instance;
			continue;
		}
		if (cascade) for (const auto handle : actor->GetChildrenHandles())
		{
			const Actor* child = ResolveActor(handle);
			if (child && child->IsDestroyed()) continue;
			pending.push_back(child);
		}
		if (!cascade) for (const auto handle : actor->GetChildrenHandles())
		{
			const auto move = CanReparent(ResolveActor(handle), nullptr);
			if (!move) return move;
		}
	}
	return {};
}

bool SceneBase::RemoveActor(Actor* root, bool cascade, StructuralMutationResult* result)
{
	if (!CanDestroy(root, cascade).Report(result)) return false;
	if (const auto* member = m_imprintInstances.FindMember(root->GetHandle()))
		return m_imprintInstances.m_system->DestroyInstance(*this, member->root, result);
	// Gather before mutation; allocation failure cannot leave a partially marked subtree.
	std::vector<Actor*> ordinary;
	std::vector<ActorHandle> instances;
	std::vector<Actor*> pending{ root };
	while (!pending.empty())
	{
		Actor* actor = pending.back(); pending.pop_back();
		if (const auto* member = m_imprintInstances.FindMember(actor->GetHandle()))
		{
			instances.push_back(member->root);
			continue;
		}
		ordinary.push_back(actor);
		if (cascade) for (Actor* child : actor->GetDirectChildren())
			if (!child->IsDestroyed()) pending.push_back(child);
	}
	StructuralMutationScope mutationScope(this);
	for (auto instance : instances)
		m_imprintInstances.m_system->CommitDestroyInstance(*this, instance);
	for (Actor* actor : ordinary)
	{
		if (!cascade) for (Actor* child : actor->GetDirectChildren())
			if (!ReparentActorInternal(child, nullptr, /*applyUIConstraints=*/true)) std::terminate();
		actor->MarkForDestruction();
		m_actorPool.Destroy(actor->GetHandle());
	}
	return true;
}
