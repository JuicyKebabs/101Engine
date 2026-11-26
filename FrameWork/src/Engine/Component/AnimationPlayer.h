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
	bool SwitchClip(std::string_view alias, float blendSeconds = 0.0f);
	void RestartCurrentClip();
	std::string GetCurrentClipName() const;

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

	struct ClipCursor
	{
		AnimationClipHandle clipHandle;
		std::vector<animation::TrackBinding> bindings;
		double timeSeconds = 0.0;
	};

	struct Transition
	{
		std::optional<ClipCursor> outgoing;
		animation::LocalPose frozenPose;
		double durationSeconds = 0.0;
		double elapsedSeconds = 0.0;
	};

	// Playback state for the currently active clip.
	struct Playback
	{
		std::size_t slotIndex = 0;
		Guid modelId;
		std::uint64_t modelRevision = 0;
		ClipCursor current;
		animation::LocalPose appliedPose;
		std::optional<Transition> transition;
		bool holdFirstPose = true;
	};

	// All registered animation clips, each identified by an alias.
	std::vector<AnimationClipSlot> m_clipSlots;
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
	static bool ResolveClipSlots(std::vector<AnimationClipSlot>& slots, AssetManager& assets);

	void ProcessAnimation(float deltaTime);

	friend class AnimationSystem;
};