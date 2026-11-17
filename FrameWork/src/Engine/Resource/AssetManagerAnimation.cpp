#include "AssetManager.h"

#include "Engine/Core/Debug/Debug.h"

#include <cmath>
#include <exception>
#include <fstream>
#include <limits>
#include <utility>

std::optional<AnimationClipHandle> AssetManager::LoadAnimationClip(const Guid& guid)
{
	const AssetEntry* entry = GetAssetEntry(guid);

	if (!entry || entry->type != AssetType::AnimationClip)
	{
		return std::nullopt;
	}

	// Searching for the cached handle.
	const auto cachedHandle = m_loadedAnimationClips.find(guid);

	if (cachedHandle != m_loadedAnimationClips.end())
	{
		const AnimationClipHandle handle = cachedHandle->second;

		if (FindAnimationClip(handle) &&
			m_animationClipCache[handle.index].guid == guid)
		{
			return handle;
		}

		// Invalid handle, generation mismatch, or GUID mismatch.
		// Remove the invalid entry from the cache and load the animation clip again.
		m_loadedAnimationClips.erase(cachedHandle);
	}
	

	if (m_animationClipCache.size() >= std::numeric_limits<std::uint32_t>::max())
	{
		DBG("LoadAnimationClip :  Cache is full. Cannot load more animation clips.");
		return std::nullopt;
	}

	try
	{
		// Load asset file and deserialize the animation clip.
		const auto path = std::filesystem::path(m_assetRoot) / std::filesystem::u8path(entry->relativePath);
		std::ifstream input(path, std::ios::binary);
		AnimationClip clip = DeserializeAnimationClip(nlohmann::json::parse(input));

		// Validate and normalize the rotation keys in the animation track.
		for (auto& track : clip.tracks)
		{
			for (auto& key : track.rotations)
			{
				auto& q = key.value;
				const double x = q.x;
				const double y = q.y;
				const double z = q.z;
				const double w = q.w;
				const double length = std::sqrt(x * x + y * y + z * z + w * w);

				if (!std::isfinite(length) || length == 0.0)
				{
					DBG("LoadAnimationClip :  Animation clip has an invalid rotation key: %s", entry->relativePath.c_str());
					return std::nullopt;
				}

				q.x = static_cast<float>(x / length);
				q.y = static_cast<float>(y / length);
				q.z = static_cast<float>(z / length);
				q.w = static_cast<float>(w / length);
			}
		}

		// Add the loaded animation clip to the cache.
		CachedAnimationClip cachedClip;
		cachedClip.guid = guid;
		cachedClip.clip = std::make_unique<AnimationClip>(std::move(clip));
		cachedClip.generation = 0;

		m_animationClipCache.push_back(std::move(cachedClip));
		
		// Create a handle for the loaded animation clip and store it.
		AnimationClipHandle handle;
		handle.index = static_cast<std::uint32_t>(m_animationClipCache.size() - 1);
		handle.generation = cachedClip.generation;
		m_loadedAnimationClips[guid] = handle;

		return handle;
	}
	catch (const std::exception& error)
	{
		DBG("LoadAnimationClip :  Exception occurred while loading animation clip: %s", error.what());
		return std::nullopt;
	}
}

const AnimationClip* AssetManager::FindAnimationClip(AnimationClipHandle handle) const
{
	if (handle.index == AnimationClipHandle::InvalidIndex ||
		handle.index >= m_animationClipCache.size())
	{
		return nullptr;
	}

	const auto& slot = m_animationClipCache[handle.index];

	if (slot.generation != handle.generation)
	{
		return nullptr;
	}

	return slot.clip.get();
}