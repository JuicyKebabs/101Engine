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

double AdvanceClipTime(double time, double step, double duration, bool loop)
{
	if (loop && duration > 0.0f)
	{
		return std::fmod(time + step, duration);
	}

	return (std::min)(time + step, duration);
}

}

AnimationClipSlots AnimationPlayer::GetClipSlots() const
{
	AnimationClipSlots value;
	value.defaultSlotAlias = m_defaultSlotAlias;
	value.slots.reserve(m_clipSlots.size());

	for (const AnimationClipSlot& slot : m_clipSlots)
	{
		value.slots.push_back(slot);
	}

	return value;
}

bool AnimationPlayer::TrySetClipSlots(const AnimationClipSlots& value)
{
	std::unordered_set<std::string_view> aliases;
	std::vector<AnimationClipSlot> slots;
	slots.reserve(value.slots.size());

	// Validate each clip slot's alias and ensure there are no duplicates.
	for (const AnimationClipSlot& slot : value.slots)
	{
		if (!IsValidAlias(slot.alias) || !aliases.insert(slot.alias).second)
		{
			return false;
		}

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

bool AnimationPlayer::ResolveClipSlots(std::vector<AnimationClipSlot>& slots, AssetManager& assets)
{
	for (const AnimationClipSlot& slot : slots)
	{
		if (!slot.clipAssetId.IsValid())
		{
			continue;
		}

		const AssetEntry* entry = assets.GetAssetEntry(slot.clipAssetId);

		if (entry && entry->type != AssetType::AnimationClip)
		{
			return false;
		}
	}

	// Preload available clips. Active cursors own their runtime handles.
	for (const AnimationClipSlot& slot : slots)
	{
		if (slot.clipAssetId.IsValid())
		{
			(void)assets.LoadAnimationClip(slot.clipAssetId);
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
		[&alias](const AnimationClipSlot& slot)
		{
			return slot.alias == alias;
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
	AnimationClipSlot slot;
	slot.alias = std::move(alias);
	slot.clipAssetId = clipGuid;

	m_clipSlots.push_back(std::move(slot));

	return true;
}

bool AnimationPlayer::SwitchClip(std::string_view alias, float blendSeconds)
{
	if (!std::isfinite(blendSeconds) || blendSeconds < 0.0f)
	{
		return false;
	}

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
		[alias](const AnimationClipSlot& slot)
		{
			return slot.alias == alias;
		});

	if (found == m_clipSlots.end() || !context || !context->pAssetManager)
	{
		DBG("AnimationPlayer: Clip alias or AssetManager is unavailable.");
		return false;
	}

	// Get the  animation clip and the skinned model view for the target renderer.
	AssetManager& assetManager = *context->pAssetManager;
	const auto handle = assetManager.LoadAnimationClip(found->clipAssetId);
	const AnimationClip* clip = handle ? assetManager.FindAnimationClip(*handle) : nullptr;
	const auto model = assetManager.FindLoadedSkinnedModel(target->GetAssetId());
	
	Playback candidate;

	// Check if the clip is suitable for the model by attempting to bind its tracks to the model's nodes.
	if (!clip || !model || !animation::BindTracks(*clip, model->nodes, candidate.current.bindings)) {
		DBG("AnimationPlayer: Clip is unavailable or its tracks do not match the model.");
		return false;
	}

	candidate.slotIndex = static_cast<std::size_t>(std::distance(m_clipSlots.begin(), found));
	candidate.modelId = target->GetAssetId();
	candidate.modelRevision = model->revision;
	candidate.current.clipHandle = *handle;

	animation::LocalPose initialPose;
	std::vector<Matrix4x4> initialModelPose;

	if (!animation::EvaluateLocalPose(*clip, candidate.current.bindings, model->nodes, 0.0, initialPose) ||
		!animation::BuildModelPoseFromLocal(model->nodes, initialPose, initialModelPose))
	{
		DBG("AnimationPlayer: Failed to evaluate the clip's initial pose.");
		return false;
	}

	const bool hasVisiblePose = m_playback && target->HasAnimationPose();
	const bool canBlend =
		hasVisiblePose && m_playback->modelId == candidate.modelId &&
		m_playback->modelRevision == candidate.modelRevision;

	if (blendSeconds > 0.0f && canBlend)
	{
		// Validate bind-pose decomposition for nodes animated by only one side.
		animation::LocalPose intermediate;

		if (!animation::BlendLocalPoses(model->nodes, m_playback->appliedPose, initialPose, 0.5f, intermediate))
		{
			DBG("AnimationPlayer: Failed to prepare the clip transition.");
			return false;
		}

		auto& transition = candidate.transition.emplace();
		transition.durationSeconds = blendSeconds;
		candidate.appliedPose = m_playback->appliedPose;

		if (m_playback->transition)
		{
			transition.frozenPose = candidate.appliedPose;
		}
		else
		{
			transition.outgoing = m_playback->current;
		}
	}
	else
	{
		if (!target->ApplyAnimationPose(*model, initialModelPose))
		{
			DBG("AnimationPlayer: Failed to apply the clip's initial pose.");
			return false;
		}

		candidate.appliedPose = std::move(initialPose);
	}

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
		SwitchClip(m_clipSlots[m_playback->slotIndex].alias);
	}
}

std::string  AnimationPlayer::GetCurrentClipName() const
{
	if (m_playback)
	{
		return m_clipSlots[m_playback->slotIndex].alias;
	}

	return {};
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
	const AnimationClip* clip = assets.FindAnimationClip(playback.current.clipHandle);
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
	const Guid modelId = target->GetAssetId();
	const bool modelChanged = playback.modelId != modelId || playback.modelRevision != model->revision;

	if (modelChanged || !target->HasAnimationPose())
	{
		playback.transition.reset();
	}

	if (modelChanged)
	{
		// Re-bind the tracks of the clip to the new model's nodes.
		if (!animation::BindTracks(*clip, model->nodes, playback.current.bindings))
		{
			DBG("AnimationPlayer: Active clip cannot bind to the new model.");
			target->ResetToBindPose();
			m_playback.reset();
			m_isPlaying = false;
			return;
		}

		playback.modelId = modelId;
		playback.modelRevision = model->revision;

		if (clip->sourceModelGuid != target->GetAssetId())
		{
			DBG("AnimationPlayer: Clip is bound to a different model. Deformation quality is unverified.");
		}
	}

	const bool validDelta = std::isfinite(deltaTime) && deltaTime >= 0.0f;
	const bool advance = m_isPlaying && !playback.holdFirstPose && validDelta;
	
	const double frameStep = advance ? static_cast<double>(deltaTime) : 0.0;
	const double clipStep = frameStep * m_speed;

	auto& current = playback.current;
	const double nextTime = advance
		? AdvanceClipTime(current.timeSeconds, clipStep, clip->durationSeconds, m_loop)
		: current.timeSeconds;

	const bool loop = m_loop && clip->durationSeconds > 0.0;
	const bool reachedEnd = advance && !loop && nextTime >= clip->durationSeconds;

	auto* transition = playback.transition ? &*playback.transition : nullptr;
	const double nextElapsed = transition
		? (std::min)(transition->elapsedSeconds + frameStep, transition->durationSeconds)
		: 0.0;

	const bool blendChanged = transition && nextElapsed != transition->elapsedSeconds;
	const bool poseChanged = modelChanged || !target->HasAnimationPose() || nextTime != current.timeSeconds;

	if (poseChanged || blendChanged)
	{
		animation::LocalPose pose;
		bool evaluated = animation::EvaluateLocalPose(*clip, current.bindings, model->nodes, nextTime, pose);
		double nextSourceTime = 0.0;
		bool blendActive = transition && nextElapsed < transition->durationSeconds;

		if (evaluated && blendActive)
		{
			animation::LocalPose sampledSource;
			const animation::LocalPose * sourcePose = &transition->frozenPose;

			if (transition->outgoing)
			{
				auto& outgoing = *transition->outgoing;
				const auto* sourceClip = assets.FindAnimationClip(outgoing.clipHandle);

				if (sourceClip)
				{
					nextSourceTime = AdvanceClipTime(
						outgoing.timeSeconds,
						clipStep,
						sourceClip->durationSeconds,
						m_loop);

					evaluated =
						animation::EvaluateLocalPose(
							*sourceClip,
							outgoing.bindings,
							model->nodes,
							nextSourceTime,
							sampledSource);

					sourcePose = &sampledSource;
				}
				else
				{
					DBG("AnimationPlayer: Outgoing clip is no longer available.");
					blendActive = false;
				}
			}

			if (evaluated && blendActive)
			{
				const float weight = static_cast<float>(nextElapsed / transition->durationSeconds);
				evaluated = animation::BlendLocalPoses(model->nodes, *sourcePose, pose, weight, pose);
			}
		}

		std::vector<Matrix4x4> nodeToModel;
		const bool applied =
			evaluated &&
			animation::BuildModelPoseFromLocal(model->nodes, pose, nodeToModel)&&
			target->ApplyAnimationPose(*model, nodeToModel);

		if (!applied)
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

		playback.appliedPose = std::move(pose);

		if (blendActive)
		{
			transition->elapsedSeconds = nextElapsed;

			if (transition->outgoing)
			{
				transition->outgoing->timeSeconds = nextSourceTime;
			}
		}
		else
		{
			playback.transition.reset();
		}
	}

	current.timeSeconds = nextTime;
	playback.holdFirstPose = false;

	if (reachedEnd && !playback.transition)
	{
		m_isPlaying = false;
	}
}
