#include "SceneActorBatch.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Component/ComponentReflection.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include <stdexcept>
#include <algorithm>

SceneActorBatch::SceneActorBatch(SceneBase& destination) : m_destination(destination)
{
	if (destination.m_actorBatchActive || destination.m_isFinalized)
	{
		throw std::logic_error("Scene cannot begin an Actor transaction.");
	}

	m_candidate.m_unpublishedCandidate = true;
	m_candidate.m_actorBatchActive = true;
	m_candidate.m_actorLookupFallback = &destination;
	m_candidate.m_pEngineContext = destination.m_pEngineContext;
	m_candidate.m_viewportSize = destination.m_viewportSize;
	destination.m_actorBatchActive = true;
}

SceneActorBatch::~SceneActorBatch()
{
	// Keep destination mutation blocked through candidate object destruction.
	m_candidate.m_actorPool.m_slots.clear();
	m_destination.m_actorBatchActive = false;
}

void SceneActorBatch::Stage(std::vector<std::unique_ptr<Actor>> actors)
{
	m_handles = m_destination.m_actorPool.PlanRegistration(actors.size());
	auto& pool = m_candidate.m_actorPool;
	std::size_t size = 0;

	for (auto h : m_handles)
	{
		size = (std::max)(size, static_cast<std::size_t>(h.index) + 1);
	}

	pool.m_slots.resize(size);

	for (std::size_t i = 0; i < actors.size(); ++i)
	{
		auto& actor = actors[i];

		if (!actor || actor->GetOwner() || !actor->GetGuid().IsValid() ||
			!m_destination.FindActorHandle(actor->GetGuid()).IsNull())
		{
			throw std::logic_error("Invalid candidate Actor identity.");
		}

		const auto handle = m_handles[i];

		if (!m_candidate.m_actorGuidMap.emplace(actor->GetGuid(), handle).second)
		{
			throw std::logic_error("Duplicate candidate Actor GUID.");
		}

		m_candidate.m_actorHandleGuidMap.emplace(handle, actor->GetGuid());
		actor->PrepareComponentsForAttach();
		actor->SetHandle(handle);
		actor->SetOwner(&m_candidate);
		pool.m_slots[handle.index] = { std::move(actor), handle.generation, false };
	}
}

bool SceneActorBatch::SetParent(Actor* actor, Actor* parent)
{
	return m_candidate.RestoreParentRelationship(actor, parent);
}

bool SceneActorBatch::ResolveAndValidate(Actor* root, Actor* externalParent)
{
	// The existing reflected serializer validates every declared reference, including
	// references in custom Components without a ResolveReferences override.
	for (const auto handle : m_handles)
	{
		Actor* actor = m_candidate.ResolveActor(handle);

		for (Component* component : actor->GetAllComponents())
		{
			nlohmann::json properties;

			if (!SerializeReflectedComponent(*component, properties, &m_candidate))
			{
				return false;
			}

			if (!component->ResolveReferences(m_candidate))
			{
				DBG("%s",
					std::string("Component reference resolution failed on Actor '" + actor->GetName() + "'.").c_str());
				return false;
			}
		}
	}

	Canvas* governing = m_destination.FindClosestCanvas(externalParent);

	if (!m_candidate.ApplyUIHierarchyConstraints(root, governing, false))
	{
		DBG("UI hierarchy requires a different Transform-family component.");
		return false;
	}

	return true;
}

void SceneActorBatch::PrepareCommit(Actor* root, Actor* externalParent)
{
	m_guidMap = m_destination.m_actorGuidMap;
	m_handleMap = m_destination.m_actorHandleGuidMap;

	for (const auto handle : m_handles)
	{
		Actor* actor = m_candidate.ResolveActor(handle);

		if (!m_guidMap.emplace(actor->GetGuid(), handle).second ||
			!m_handleMap.emplace(handle, actor->GetGuid()).second)
		{
			throw std::logic_error("Candidate publication identity conflict.");
		}
	}

	std::size_t size = m_destination.m_actorPool.m_slots.size();

	for (auto h : m_handles)
	{
		size = (std::max)(size, static_cast<std::size_t>(h.index) + 1);
	}

	m_destination.m_actorPool.m_slots.reserve(size);

	if (externalParent)
	{
		externalParent->m_childHandles.reserve(externalParent->m_childHandles.size() + 1);
	}

	m_root = root;
	m_externalParent = externalParent;
	m_prepared = true;
}

void SceneActorBatch::Commit() noexcept
{
	if (!m_prepared || m_committed)
	{
		std::terminate();
	}

	for (const auto handle : m_handles)
	{
		auto actor = std::move(m_candidate.m_actorPool.m_slots[handle.index].actor);
		actor->SetOwner(&m_destination);

		if (m_destination.m_actorPool.Register(std::move(actor)) != handle)
		{
			std::terminate();
		}
	}

	m_destination.m_actorGuidMap.swap(m_guidMap);
	m_destination.m_actorHandleGuidMap.swap(m_handleMap);

	if (m_externalParent)
	{
		m_root->SetParentHandle(m_externalParent->GetHandle());
	}

	m_committed = true;
}

void SceneActorBatch::Attach() noexcept
{
	if (!m_committed)
	{
		std::terminate();
	}

	// Registry is committed by the caller before any callback can observe the Scene.
	for (const auto handle : m_handles)
	{
		m_destination.ResolveActor(handle)->AttachComponents();
	}
}
