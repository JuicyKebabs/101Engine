#pragma once

#include "Component.h"
#include "Engine/Animation/AnimationClipSlots.h"
#include "Engine/Animation/AnimationPose.h"
#include "Engine/Core/GUID/Guid.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class AnimationSystem;
class AssetManager;
class SkinnedMeshRenderer;

//------------------------------------------------------------------------------------------------
// AnimationPlayer
// Component that manages the playback of animation clips on a SkinnedMeshRenderer.
// Manages tht multiple animation clips with aliases, playback state, and speed/looping settings.
//------------------------------------------------------------------------------------------------

class AnimationPlayer : public Component
{
public:
	bool AddClip(std::string alias, const Guid& clipGuid);
	bool SwitchClip(std::string_view alias);
	void RestartCurrentClip();

	void SetIsPlaying(bool isPlaying) { m_isPlaying = isPlaying; }
	bool IsPlaying() const { return m_isPlaying; }
	void SetLoop(bool loop) { m_loop = loop; }
	bool GetLoop() const { return m_loop; }
	bool SetPlaybackSpeed(float speed);
	float GetPlaybackSpeed() const { return m_speed; }
	void SetPlayOnStart(bool playOnStart) { m_playOnStart = playOnStart; }
	bool GetPlayOnStart() const { return m_playOnStart; }

	AnimationClipSlots GetClipSlots() const;
	bool TrySetClipSlots(const AnimationClipSlots& value);
	bool ResolveReferences(SceneBase& scene) override;

private:
	// One slot per animation clip, identified by alias.
	struct Slot
	{
		AnimationClipSlot setting;
		AnimationClipHandle clipHandle;
	};

	// Playback state for the currently active clip.
	struct Playback
	{
		std::size_t slotIndex = 0;
		std::uint64_t modelRevision = 0;
		std::vector<animation::TrackBinding> bindings;
		double timeSeconds = 0.0;
		bool holdFirstPose = true;
	};

	// All registered animation clips, each identified by an alias.
	std::vector<Slot> m_clipSlots;
	std::string m_defaultSlotAlias;	// The alias of the clip used for initial playback.

	// Optional playback state for the currently active clip.
	// If no clip is active, this will be std::nullopt.
	std::optional<Playback> m_playback;	

	float m_speed = 1.0f;
	bool m_loop = true;
	bool m_playOnStart = false;	// If the default clip is started automatically when the component starts.

	bool m_isPlaying = false;

private:
	void OnStartOverride() override;
	void OnDetachOverride() override;
	void PreUpdateOverride(float deltaTime) override {}
	void UpdateOverride(float deltaTime) override {}
	void LateUpdateOverride(float deltaTime) override {}
	void OnDestroyOverride() override {}

	SkinnedMeshRenderer* ResolveSkinnedMeshRenderer() const;
	static bool ResolveClipSlots(std::vector<Slot>& slots, AssetManager& assets);

	void ProcessAnimation(float deltaTime);

	friend class AnimationSystem;
};