#include "AnimationPlayer.h"

#include "Engine/Actor/Actor.h"
#include "Engine/Component/SkinnedMeshRenderer.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Scene/SceneBase.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <unordered_set>
#include <utility>

namespace
{

bool IsValidAlias(std::string_view alias)
{
	return !alias.empty() && alias.find('\0') == std::string_view::npos;
}

bool ApplyPose(
	SkinnedMeshRenderer& target,
	const SkinnedModelView& model,
	const AnimationClip& clip,
	std::span<const animation::TrackBinding> bindings,
	double timeSeconds)
{
	std::vector<Matrix4x4> nodeToModel;

	// Evaluate the animation pose for the given clip and bindings at the specified time
	// and store the resulting node-to-model transforms in nodeToModel.
	return animation::EvaluatePose(clip, bindings, model.nodes, timeSeconds, nodeToModel) &&
		   target.ApplyAnimationPose(model, nodeToModel);
}

}

AnimationClipSlots AnimationPlayer::GetClipSlots() const
{
	AnimationClipSlots value;
	value.defaultSlotAlias = m_defaultSlotAlias;
	value.slots.reserve(m_clipSlots.size());

	for (const Slot& slot : m_clipSlots)
	{
		value.slots.push_back(slot.setting);
	}

	return value;
}

bool AnimationPlayer::TrySetClipSlots(const AnimationClipSlots& value)
{
	std::unordered_set<std::string_view> aliases;
	std::vector<Slot> slots;
	slots.reserve(value.slots.size());

	// Validate each clip slot's alias and ensure there are no duplicates.
	for (const AnimationClipSlot& setting : value.slots)
	{
		if (!IsValidAlias(setting.alias) || !aliases.insert(setting.alias).second)
		{
			return false;
		}

		Slot slot;
		slot.setting = setting;
		slots.push_back(std::move(slot));
	}

	// Validate that the default slot alias is either empty or exists in the provided slots.
	if (!value.defaultSlotAlias.empty() && !aliases.contains(value.defaultSlotAlias))
	{
		return false;
	}
	
	// Check if the current clip slots are already equal to the provided value.
	if (GetClipSlots() == value)
	{
		return true;
	}

	const EngineContext* context = GetEngineContext();

	// Resolve every clip asset in the provided slots using the AssetManager.
	if (context && context->pAssetManager && !ResolveClipSlots(slots, *context->pAssetManager))
	{
		return false;
	}

	std::string defaultSlotAlias = value.defaultSlotAlias;
	m_clipSlots.swap(slots);
	m_defaultSlotAlias.swap(defaultSlotAlias);

	// Reset the pose of target SkinnedMeshRenderer and clear the current playback state.
	if (m_playback)
	{
		if (auto* target = ResolveSkinnedMeshRenderer())
		{
			target->ResetToBindPose();
		}
	}

	m_playback.reset();
	m_isPlaying = false;
	return true;
}

bool AnimationPlayer::ResolveReferences(SceneBase& scene)
{
	if (m_clipSlots.empty())
	{
		return true;
	}

	const Actor* actor = GetOwner();
	const EngineContext* context = scene.GetEngineContext();

	if (!actor || actor->GetOwner() != &scene || !context)
	{
		return false;
	}

	if (!context->pAssetManager)
	{
		return false;
	}

	return ResolveClipSlots(m_clipSlots, *context->pAssetManager);
}

bool AnimationPlayer::ResolveClipSlots(std::vector<Slot>& slots, AssetManager& assets)
{
	for (const Slot& slot : slots)
	{
		if (!slot.setting.clipAssetId.IsValid())
		{
			continue;
		}

		const AssetEntry* entry = assets.GetAssetEntry(slot.setting.clipAssetId);

		if (entry && entry->type != AssetType::AnimationClip)
		{
			return false;
		}
	}

	// Load animation clips for each valid slot into the AssetManager.
	// and store the resulting handles in the corresponding slots.
	for (Slot& slot : slots)
	{
		slot.clipHandle = {};

		if (!slot.setting.clipAssetId.IsValid())
		{
			continue;
		}

		if (const auto handle = assets.LoadAnimationClip(slot.setting.clipAssetId))
		{
			slot.clipHandle = *handle;
		}
	}

	return true;
}

bool AnimationPlayer::AddClip(std::string alias, const Guid& clipGuid)
{
	const EngineContext* context = GetEngineContext();

	if (!IsValidAlias(alias) || !context || !context->pAssetManager)
	{
		return false;
	}

	// Check duplicate alias in the existing clip slots.
	const auto existing = std::find_if(
		m_clipSlots.begin(), m_clipSlots.end(),
		[&alias](const Slot& slot)
		{
			return slot.setting.alias == alias;
		});

	if (existing != m_clipSlots.end())
	{
		return false;
	}

	// Get handle for the animation clip.
	const auto handle = context->pAssetManager->LoadAnimationClip(clipGuid);

	if (!handle)
	{
		return false;
	}

	// Add the new clip slot with the alias and handle.
	Slot slot;
	slot.setting.alias = std::move(alias);
	slot.setting.clipAssetId = clipGuid;
	slot.clipHandle = *handle;

	m_clipSlots.push_back(std::move(slot));

	return true;
}

bool AnimationPlayer::SwitchClip(std::string_view alias)
{
	// Get the target SkinnedMeshRenderer component from the owning actor.
	SkinnedMeshRenderer* target = ResolveSkinnedMeshRenderer();

	if (!target || !target->IsConfigured())
	{
		DBG("AnimationPlayer: Target SkinnedMeshRenderer is unavailable or not configured.");
		return false;
	}

	const EngineContext* context = GetEngineContext();
	
	// Check if the alias exists in the current clip slots.
	const auto found = std::find_if(
		m_clipSlots.begin(), m_clipSlots.end(),
		[alias](const Slot& slot)
		{
			return slot.setting.alias == alias;
		});

	if (found == m_clipSlots.end() || !context || !context->pAssetManager)
	{
		DBG("AnimationPlayer: Clip alias or AssetManager is unavailable.");
		return false;
	}

	// Get the  animation clip and the skinned model view for the target renderer.
	AssetManager& assetManager = *context->pAssetManager;
	const auto handle = assetManager.LoadAnimationClip(found->setting.clipAssetId);
	const AnimationClip* clip = handle ? assetManager.FindAnimationClip(*handle) : nullptr;
	const auto model = assetManager.FindLoadedSkinnedModel(target->GetAssetId());
	
	Playback candidate;

	// Check if the clip is suitable for the model by attempting to bind its tracks to the model's nodes.
	if (!clip || !model || !animation::BindTracks(*clip, model->nodes, candidate.bindings))
	{
		DBG("AnimationPlayer: Clip is unavailable or its tracks do not match the model.");
		return false;
	}

	// Apply the initial pose of the clip to the target renderer at time 0.0 seconds.
	if (!ApplyPose(*target, *model, *clip, candidate.bindings, candidate.timeSeconds))
	{
		DBG("AnimationPlayer: Failed to apply the clip's initial pose.");
		return false;
	}

	// Update the playback state with the new clip and its associated data.
	candidate.slotIndex = static_cast<std::size_t>(std::distance(m_clipSlots.begin(), found));
	candidate.modelRevision = model->revision;
	found->clipHandle = *handle;
	m_playback = std::move(candidate);

	if (clip->sourceModelGuid != target->GetAssetId())
	{
		DBG("AnimationPlayer: Clip is bound to a different model. Deformation quality is unverified.");
	}

	return true;
}

void AnimationPlayer::RestartCurrentClip()
{
	if (m_playback)
	{
		// Reset the playback time to 0.0 seconds and reapply the initial pose of the current clip.
		SwitchClip(m_clipSlots[m_playback->slotIndex].setting.alias);
	}
}

bool AnimationPlayer::SetPlaybackSpeed(float speed)
{
	if (!std::isfinite(speed) || speed < 0.0f)
	{
		return false;
	}

	m_speed = speed;
	return true;
}

void AnimationPlayer::OnStartOverride()
{
	Actor* actor = GetOwner();
	SceneBase* scene = actor ? actor->GetOwner() : nullptr;

	if (scene)
	{
		// Register this AnimationPlayer with the scene's AnimationSystem to receive updates.
		scene->GetAnimationSystem()->Register(this);

		// If configured to play on start, and there is no current playback, attempt to switch to the default clip.
		if (m_playOnStart && !m_playback && !m_defaultSlotAlias.empty())
		{
			SetIsPlaying(SwitchClip(m_defaultSlotAlias));
		}
	}
}

void AnimationPlayer::OnDetachOverride()
{
	Actor* actor = GetOwner();
	SceneBase* scene = actor ? actor->GetOwner() : nullptr;

	if (scene)
	{
		// Unregister this AnimationPlayer from the scene's AnimationSystem.
		scene->GetAnimationSystem()->Unregister(this);
	}

	if (auto* target = ResolveSkinnedMeshRenderer())
	{
		// Reset the target renderer to its bind pose.
		target->ResetToBindPose();
	}
}

SkinnedMeshRenderer* AnimationPlayer::ResolveSkinnedMeshRenderer() const
{
	Actor* actor = GetOwner();
	return actor ? actor->GetComponentByClass<SkinnedMeshRenderer>() : nullptr;
}

void AnimationPlayer::ProcessAnimation(float deltaTime)
{
	SkinnedMeshRenderer* target = ResolveSkinnedMeshRenderer();

	if (!m_playback || !target || !target->IsConfigured())
	{
		return;
	}

	const EngineContext* context = GetEngineContext();

	if (!context || !context->pAssetManager)
	{
		return;
	}

	// Get the current playback state and the associated animation clip and model.
	auto& playback = *m_playback;
	const auto& assets = *context->pAssetManager;
	const AnimationClip* clip = assets.FindAnimationClip(m_clipSlots[playback.slotIndex].clipHandle);
	const auto model = assets.FindLoadedSkinnedModel(target->GetAssetId());

	if (!clip || !model)
	{
		// Stop playing and reset SkinnedMeshRenderer to bind pose.
		DBG("AnimationPlayer: Active clip or model is no longer available.");
		target->ResetToBindPose();
		m_playback.reset();
		m_isPlaying = false;
		return;
	}

	// Check if the model has changed since the last playback update.
	const bool modelChanged = playback.modelRevision != model->revision;

	if (modelChanged)
	{
		// Re-bind the tracks of the clip to the new model's nodes.
		if (!animation::BindTracks(*clip, model->nodes, playback.bindings))
		{
			DBG("AnimationPlayer: Active clip cannot bind to the new model.");
			target->ResetToBindPose();
			m_playback.reset();
			m_isPlaying = false;
			return;
		}

		playback.modelRevision = model->revision;

		if (clip->sourceModelGuid != target->GetAssetId())
		{
			DBG("AnimationPlayer: Clip is bound to a different model. Deformation quality is unverified.");
		}
	}

	// Advance the playback time based on deltaTime, speed, and looping settings.
	double nextTime = playback.timeSeconds;
	bool reachedEnd = false;

	if (m_isPlaying && !playback.holdFirstPose && std::isfinite(deltaTime) && deltaTime >= 0.0f)
	{
		const double duration = clip->durationSeconds;
		const double advanced = nextTime + static_cast<double>(deltaTime) * m_speed;

		if (m_loop && duration > 0.0)
		{
			// Wrap the playback time around using modulo to loop the animation.
			nextTime = std::fmod(advanced, duration);
		}
		else
		{
			// Clamp the playback time to the duration of the clip
			// and check if the end has been reached.
			nextTime = (std::min)(advanced, duration);
			reachedEnd = advanced >= duration;
		}
	}

	// Determine if the pose needs to be evaluated and applied based on model changes, animation state, and time.
	const bool needsPose = modelChanged || !target->HasAnimationPose() || nextTime != playback.timeSeconds;

	// If the pose needs to be updated, evaluate the animation clip at the new time and apply it to the target renderer.
	if (needsPose && !ApplyPose(*target, *model, *clip, playback.bindings, nextTime))
	{
		DBG("AnimationPlayer: Failed to apply the evaluated pose.");

		if (modelChanged)
		{
			target->ResetToBindPose();
			m_playback.reset();
		}

		m_isPlaying = false;
		return;
	}

	playback.timeSeconds = nextTime;
	playback.holdFirstPose = false;

	if (reachedEnd)
	{
		m_isPlaying = false;
	}
}