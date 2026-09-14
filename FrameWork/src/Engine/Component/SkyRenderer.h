#pragma once
#include "Engine/Component/RendererComponent.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Actor/ActorReference.h"
#include "Engine/Resource/AssetReference.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Core/GUID/Guid.h"
#include <optional>

class Transform;

class SkyRenderer : public RendererComponent
{
public:
	enum class FollowMode
	{
		Owner,	// Follow the owner actor's world position
		MainCamera,	// Follow the main camera actor's world position
		Actor,	// Follow a specific actor's world position (set via SetFollowActor)
	};

public:
	bool SetAsActiveSkyRenderer();
	bool SetSkyTextureAsset(const Guid& texture);

	const SubmeshRenderTemplate& GetRenderTemplate() const { return m_template; }
	const MeshRendererProxy& GetRenderProxy();
	bool IsConfigured() const override;

	void SetFollowMode(FollowMode mode);
	bool SetFollowActor(Actor* actor);
	bool SetFollowActor(const Guid& guid);

	FollowMode GetFollowMode() const { return m_followMode; }
	const ActorReference& GetFollowActorReference() const { return m_followActor; }
	void SetFollowActorReference(const ActorReference& actor);
	AssetReference<TextureAsset> GetSkyTextureAssetReference() const;
	bool TrySetSkyTextureAssetReference(const AssetReference<TextureAsset>& value);
	Guid GetSkyTextureAssetId() const;
	bool ResolveReferences(SceneBase& scene) override;

private:
	struct PreparedTextureAssetState
	{
		Guid assetId;
		TextureHandle textureHandle = InvalidTextureHandle;
	};

	SubmeshRenderTemplate m_template;	// Render template for the sky sphere mesh
	MeshRendererProxy m_proxy;			// Cached render proxy for this component

	FollowMode m_followMode = FollowMode::Owner;	// Selects the source of the sky sphere's world position
	ActorReference m_followActor;					// Reference to the actor to follow when FollowMode is Actor
	Guid m_skyTextureId;							// Reference to the sky texture asset
	std::optional<Guid> m_pendingSkyTextureId;
	ActorHandle m_cachedFollowActorHandle = ActorHandle::Null();
	uint64_t m_cachedFollowTransformGeneration = static_cast<uint64_t>(-1);
	float m_sphereScale = 500.0f;

private:
	void OnAttachOverride() override;
	void OnStartOverride() override {};
	void PreUpdateOverride(float deltaTime) override {};
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override {};
	void OnDetachOverride() override;
	void OnDestroyOverride() override;
	void ApplyBlendModeToRenderTemplates() override;

	AssetPrepareResult PrepareSkyRendererState(PreparedTextureAssetState& outPreparedState, const Guid& textureId) const;
	void CommitSkyTextureState(PreparedTextureAssetState&& state);
	bool SetPendingSkyTextureAssetReference(const AssetReference<TextureAsset>& value);
	void InvalidateFollowCache();
	Transform* ResolveFollowTransform(Actor*& outActor);
	void RefreshFollowState();

	void RebuildRenderProxy();
};
