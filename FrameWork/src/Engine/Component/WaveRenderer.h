#pragma once
#include "RendererComponent.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Component/MeshRenderer.h"

struct WaveRendererProxy
{
	CommonRendererProxy common;
	TextureHandle textureOverrideHandle = InvalidTextureHandle;
	Vector2 vertexDivisions{ 10, 10 };
	float time = 0.0f;
	float waveAmplitude = 0.5f;
	float waveFrequency = 1.0f;
	Vector2 waveDirection = { 1, 1 };
};

class WaveRenderer : public RendererComponent
{
public:
	struct ParamDesc
	{
		SubmeshRenderTemplate renderTemplate;
		Vector2 vertexDivisions{ 10, 10 };
		float waveSpeed = 1.0f;
		float waveAmplitude = 0.5f;
		float waveFrequency = 1.0f;
		Vector2 waveDirection{ 1.0f, 0.0f };
	};

public:
	WaveRenderer() = default;
	~WaveRenderer() = default;

	AssetReference<TextureAsset> GetWaveTextureAssetReference() const;
	bool TrySetWaveTextureAssetReference(const AssetReference<TextureAsset>& value);

	bool SetWaveTextureAsset(const Guid& assetId);
	void SetVertexDivisions(const Vector2& divisions){ m_vertexDivisions = divisions; m_isProxyDirty = true; }
	void SetWaveSpeed(float speed) { m_waveSpeed = speed; }
	void SetWaveAmplitude(float amplitude) { m_waveAmplitude = amplitude; m_isProxyDirty = true; }
	void SetWaveFrequency(float frequency) { m_waveFrequency = frequency; m_isProxyDirty = true; }
	void SetWaveDirection(const Vector2& direction) { m_waveDirection = direction; m_isProxyDirty = true; }

	const SubmeshRenderTemplate& GetRenderTemplate() const { return m_template; }
	const WaveRendererProxy& GetRenderProxy();
	const Guid& GetWaveTextureAssetId() const;
	Vector2 GetVertexDivisions() const { return m_vertexDivisions; }
	float GetWaveSpeed() const { return m_waveSpeed; }
	float GetWaveAmplitude() const { return m_waveAmplitude; }
	float GetWaveFrequency() const { return m_waveFrequency; }
	Vector2 GetWaveDirection() const { return m_waveDirection; }

	bool IsConfigured() const override { return m_waveTextureId.IsValid(); }
	bool ResolveReferences(SceneBase& scene) override;

private:
	struct PreparedTextureAssetState
	{
		Guid assetId;
		TextureHandle textureHandle = InvalidTextureHandle;
	};

	SubmeshRenderTemplate m_template;	// Render template for the plane mesh
	WaveRendererProxy m_proxy;			// Cached render proxy for this component

	Guid m_waveTextureId;										// Reference to the wave texture asset
	std::optional<Guid> m_pendingWaveTextureId;					// Optional pending wave texture asset ID for asynchronous loading
	TextureHandle m_waveTextureHandle = InvalidTextureHandle;	// Handle to the wave texture for rendering

	Vector2 m_vertexDivisions{ 10, 10 };	// Number of divisions in the plane mesh (X and Y)
	float m_time = 0.0f;					// Time accumulator for wave animation
	float m_waveSpeed = 1.0f;				// Speed of the time passage for the wave animation

	float m_waveAmplitude = 0.5f;			// Amplitude of the wave animation
	float m_waveFrequency = 1.0f;			// Frequency of the wave animation
	Vector2 m_waveDirection{ 1.0f, 0.0f };	// Direction of the wave animation (normalized vector)

private:
	// Override functions for component lifecycle
	void OnAttachOverride() override;
	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override;
	void OnDetachOverride() override;
	void OnDestroyOverride() override;
	void ApplyBlendModeToRenderTemplates() override;

	// Rebuild the render proxy 
	// (Called when GetRenderProxy is called and the transform is dirty)
	void RebuildRenderProxy();

	// Build the world matrix for this renderer
	Matrix4x4 BuildWorldMatrix(Transform* transform) const;

	AssetPrepareResult PrepareWaveTextureAssetState(
		PreparedTextureAssetState& outState,
		const Guid& assetId) const;

	bool SetPendingWaveTextureAssetReference(const AssetReference<TextureAsset>& value);

	void CommitWaveTextureAssetState(PreparedTextureAssetState&& state);
};