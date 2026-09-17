#include "ActorImprintSystem.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"

ActorImprintHandle ActorImprintSystem::Load(const Guid& assetGuid)
{
	// Find the handle for the assetGuid if it is already loaded
	// to avoid reloading the same asset multiple times.
	const auto existing = FindHandle(assetGuid);

	// A reload Scene candidate may use every definition already retained by the
	// supplied live Scenes, including another Asset currently marked Missing.
	if (m_buildingReloadCandidate && !existing.IsNull() && Resolve(existing))
	{
		return existing;
	}

	if (m_materializing || m_reloading)
	{
		DBG("ActorImprint load is unavailable during a materialization or reload transaction.");
		return {};
	}

	if (!existing.IsNull() && IsMissing(existing))
	{
		DBG("ActorImprint asset is missing or awaiting a validated same-GUID restore.");
		return {};
	}

	AssetManagerAssetReferenceContext context(m_assets);
	const auto validation = context.Validate(assetGuid, AssetType::ActorImprint);

	if (!validation)
	{
		DBG("ActorImprint asset reference did not resolve in the catalog.");
		return {};
	}

	if (!existing.IsNull())
	{
		return existing;
	}

	auto candidate = ActorImprintAssetDeserializer::Load(m_assets.GetAssetPath(assetGuid), &context);

	if (!candidate)
	{
		DBG("ActorImprint candidate load failed; the pool was not changed.");
		return {};
	}

	if (m_freeIndices.empty() && m_slots.size() >= UINT32_MAX)
	{
		DBG("ActorImprint slot capacity exhausted.");
		return {};
	}

	const bool reuse = !m_freeIndices.empty();
	const auto index = reuse ? m_freeIndices.back() : static_cast<std::uint32_t>(m_slots.size());

	if (!reuse)
	{
		m_slots.emplace_back();
	}

	Slot& slot = m_slots[index];
	const ActorImprintHandle handle{ index, slot.generation };
	try
	{
		m_handles.emplace(assetGuid, handle);
	}
	catch (...)
	{
		if (!reuse)
		{
			m_slots.pop_back();
		}

		throw;
	}

	if (reuse)
	{
		m_freeIndices.pop_back();
	}

	slot.assetGuid = assetGuid;
	slot.definition = std::move(candidate);
	slot.missing = false;

	return handle;
}

ActorImprintHandle ActorImprintSystem::FindHandle(const Guid& assetGuid) const
{
	const auto entry = m_handles.find(assetGuid);
	return entry == m_handles.end() ? ActorImprintHandle{} : entry->second;
}

const ActorImprint* ActorImprintSystem::Resolve(ActorImprintHandle handle) const
{
	if (handle.index >= m_slots.size())
	{
		return nullptr;
	}

	const Slot& slot = m_slots[handle.index];

	if (slot.generation != handle.generation || !slot.definition)
	{
		return nullptr;
	}

	return slot.definition.get();
}

const ActorImprint* ActorImprintSystem::ResolveForSceneCandidate(ActorImprintHandle handle) const
{
	if (!Resolve(handle))
	{
		return nullptr;
	}

	if (m_buildingReloadCandidate && handle == m_reloadHandle && m_reloadDefinition)
	{
		return m_reloadDefinition;
	}

	return m_slots[handle.index].definition.get();
}

Guid ActorImprintSystem::GetAssetGuid(ActorImprintHandle handle) const
{
	return Resolve(handle) ? m_slots[handle.index].assetGuid : Guid{};
}

ActorImprintAvailability ActorImprintSystem::GetAvailability(ActorImprintHandle handle) const
{
	if (handle.index >= m_slots.size())
	{
		return ActorImprintAvailability::Invalid;
	}

	const Slot& slot = m_slots[handle.index];

	if (slot.generation != handle.generation || !slot.definition)
	{
		return ActorImprintAvailability::Invalid;
	}

	const AssetEntry* entry = m_assets.GetAssetEntry(slot.assetGuid);

	if (slot.missing || !entry || entry->type != AssetType::ActorImprint)
	{
		return ActorImprintAvailability::Missing;
	}

	return ActorImprintAvailability::Available;
}

std::size_t ActorImprintSystem::GetLiveInstanceCount(const Guid& assetGuid) const
{
	const ActorImprintHandle handle = FindHandle(assetGuid);
	return Resolve(handle) ? m_slots[handle.index].instances : 0;
}

bool ActorImprintSystem::Unload(ActorImprintHandle handle)
{
	if (m_materializing || m_reloading || !Resolve(handle) || m_slots[handle.index].instances != 0)
	{
		return false;
	}

	Slot& slot = m_slots[handle.index];
	// Retire exhausted generations permanently rather than aliasing a stale handle.
	if (slot.generation != UINT32_MAX)
	{
		m_freeIndices.push_back(handle.index);
	}

	m_handles.erase(slot.assetGuid);
	slot.definition.reset();
	slot.assetGuid = {};
	slot.missing = false;

	if (slot.generation != UINT32_MAX)
	{
		++slot.generation;
	}

	return true;
}

bool ActorImprintSystem::Clear()
{
	if (m_materializing || m_reloading)
	{
		return false;
	}

	for (const auto& slot : m_slots)
	{
		if (slot.instances != 0)
		{
			return false;
		}
	}

	for (std::size_t i = 0; i < m_slots.size(); ++i)
	{
		if (m_slots[i].definition)
		{
			Unload({ static_cast<std::uint32_t>(i), m_slots[i].generation });
		}
	}

	return true;
}

void ActorImprintSystem::ReleaseInstance(ActorImprintHandle handle)
{
	if (!Resolve(handle) || m_slots[handle.index].instances == 0)
	{
		std::terminate();
	}

	m_slots[handle.index].instances--;
}
