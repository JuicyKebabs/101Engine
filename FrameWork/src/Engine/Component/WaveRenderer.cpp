#include "WaveRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/MeshManager.h"

AssetReference<TextureAsset> WaveRenderer::GetWaveTextureAssetReference() const
{
	AssetReference<TextureAsset> value;
	value.SetValue({ GetWaveTextureAssetId(), AssetType::Texture, m_waveTextureId.IsValid() });
	return value;
}

bool WaveRenderer::TrySetWaveTextureAssetReference(const AssetReference<TextureAsset>& value)
{
	Actor* owner = GetOwner();

	if (!owner || !owner->GetOwner() || (value.HasValue() && !value.IsResolved()))
	{
		return SetPendingWaveTextureAssetReference(value);
	}

	PreparedTextureAssetState prepared;
	const Guid assetId = value.HasValue() ? value.GetGuid() : Guid{};

	if (PrepareWaveTextureAssetState(prepared, assetId) != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitWaveTextureAssetState(std::move(prepared));
	return true;
}

bool WaveRenderer::SetWaveTextureAsset(const Guid& assetId)
{
	PreparedTextureAssetState prepared;

	if (PrepareWaveTextureAssetState(prepared, assetId) != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitWaveTextureAssetState(std::move(prepared));
	return true;
}

void WaveRenderer::OnAttachOverride()
{
	auto* owner = GetOwner();
	if (!owner)
	{
		return;
	}
	
	auto* scene = owner->GetOwner();
	if (!scene)
	{
		return;
	}
	
	auto* renderSystem = scene->GetRenderSystem();
	if (!renderSystem)
	{
		return;
	}

	EngineContext* context = GetEngineContext();

	if (!context || !context->pMeshManager)
	{
		return;
	}

	// Fix the mesh to be a quad mesh for the wave renderer
	MeshManager* meshManager = context->pMeshManager;
	const MeshHandle quadMesh = meshManager->LoadDefaultMesh(DefaultMesh::Quad);
	MeshGPU* meshGPU = meshManager->GetMeshGPU(quadMesh);

	if (quadMesh == InvalidMeshHandle || !meshGPU)
	{
		return;
	}

	m_template.meshDesc.meshHandle = quadMesh;
	m_template.meshDesc.boundsCenter = meshGPU->GetBoundsCenter();
	m_template.meshDesc.boundsRadius = meshGPU->GetBoundsRadius();

	m_template.materialDesc.psoKey.vsKey.fileID = VS_FILE_ID::Wave;					// Use the original wave vertex shader for the vertex animation
	m_template.materialDesc.psoKey.psKey.fileID = PS_FILE_ID::Mesh;					// Use same pixel shader as the mesh renderer for the water surface
	m_template.materialDesc.psoKey = m_template.materialDesc.psoKey.WithLighting();	// Enable lighting for the water surface
	m_template.materialDesc.lightingEnabled = true;

	m_isProxyDirty = true;
	
	renderSystem->Register(this);
}

void WaveRenderer::OnStartOverride()
{}

void WaveRenderer::PreUpdateOverride(float deltaTime)
{}

void WaveRenderer::UpdateOverride(float deltaTime)
{
	// Update time and mark the proxy as dirty if time has changed
	float m_oldTime = m_time;
	m_time += deltaTime * m_waveSpeed;

	if (m_oldTime != m_time)
	{
		m_isProxyDirty = true;
	}
}

void WaveRenderer::LateUpdateOverride(float deltaTime)
{}

void WaveRenderer::OnDetachOverride()
{
	auto* owner = GetOwner();

	if (!owner)
	{
		return;
	}

	auto* scene = owner->GetOwner();

	if (!scene)
	{
		return;
	}

	auto* renderSystem = scene->GetRenderSystem();

	if (!renderSystem)
	{
		return;
	}

	renderSystem->Unregister(this);
}

void WaveRenderer::OnDestroyOverride()
{}

const WaveRendererProxy& WaveRenderer::GetRenderProxy()
{
	auto owner = GetOwner();
	auto transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;

	if (transform && m_transformGeneration != transform->GetWorldGeneration())
	{
		m_isProxyDirty = true;
	}

	if (m_isProxyDirty)
	{
		RebuildRenderProxy();

		if (transform)
		{
			m_transformGeneration = transform->GetWorldGeneration();
		}

		m_isProxyDirty = false;
	}

	return m_proxy;
}

const Guid& WaveRenderer::GetWaveTextureAssetId() const
{
	if (m_waveTextureId.IsValid())
	{
		return m_waveTextureId;
	}

	return m_pendingWaveTextureId.value_or(Guid{});
}

void WaveRenderer::RebuildRenderProxy()
{
	auto owner = GetOwner();

	if (owner)
	{
		auto transform = owner->GetComponentByClass<Transform>();

		if (transform)
		{
			m_proxy.common.position = transform->GetWorldPosition();
			m_proxy.common.worldMatrix = BuildWorldMatrix(transform);
			m_proxy.common.renderSpace = GetRenderSpace();

			Canvas* governingCanvas = GetGoverningCanvas();
			m_proxy.common.color = m_color;
			m_proxy.common.visible = m_isVisible;
		}
	}

	m_proxy.textureOverrideHandle = m_waveTextureHandle;
	m_proxy.vertexDivisions = m_vertexDivisions;
	m_proxy.time = m_time;
	m_proxy.waveAmplitude = m_waveAmplitude;
	m_proxy.waveFrequency = m_waveFrequency;
	m_proxy.waveDirection = m_waveDirection.Normalized();
}

Matrix4x4 WaveRenderer::BuildWorldMatrix(Transform* transform) const
{
	if (!transform)
	{
		return Matrix4x4::Identity();
	}

	RectTransform* rectTransform = dynamic_cast<RectTransform*>(transform);

	if (rectTransform && GetGoverningCanvas())
	{
		const MeshDesc& meshDesc = m_template.meshDesc;
		float modelRadius = meshDesc.boundsCenter.Length() + meshDesc.boundsRadius;

		Transform3D renderTransform = rectTransform->GetWorldTransform();

		const Vector2 rectSize = rectTransform->GetSize();

		const float rectShortSide = (std::min)(rectSize.x, rectSize.y);

		const float modelDiameter = modelRadius * 2.0f;

		if (modelDiameter > 0.0001f && std::isfinite(modelDiameter))
		{
			const float fitScale = rectShortSide / modelDiameter;

			renderTransform.scale *= fitScale;
		}

		return renderTransform.GetMatrix();
	}
	else
	{
		return transform->GetWorldMatrix();
	}
}

AssetPrepareResult WaveRenderer::PrepareWaveTextureAssetState(
	PreparedTextureAssetState& outPreparedState,
	const Guid& textureId) const
{
	if (!textureId.IsValid())
	{
		outPreparedState = {};
		return AssetPrepareResult::Ready;
	}

	const EngineContext* context = GetEngineContext();

	if (!context || !context->pAssetManager || !context->pTextureManager)
	{
		return AssetPrepareResult::Failed;
	}

	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(textureId);

	if (!assetEntry)
	{
		return AssetPrepareResult::MissingAsset;
	}

	if (assetEntry->type != AssetType::Texture)
	{
		return AssetPrepareResult::Failed;
	}

	const TextureHandle textureHandle = context->pAssetManager->GetTextureHandle(textureId);

	if (textureHandle == InvalidTextureHandle)
	{
		return AssetPrepareResult::Failed;
	}

	outPreparedState.assetId = textureId;
	outPreparedState.textureHandle = textureHandle;
	return AssetPrepareResult::Ready;
}

bool WaveRenderer::SetPendingWaveTextureAssetReference(const AssetReference<TextureAsset>& value)
{
	m_waveTextureId = {};
	m_pendingWaveTextureId.reset();
	m_template.materialDesc.textureHandle = InvalidTextureHandle;

	if (value.HasValue())
	{
		m_pendingWaveTextureId = value.GetGuid();
	}

	m_isProxyDirty = true;
	return true;
}

void WaveRenderer::CommitWaveTextureAssetState(PreparedTextureAssetState&& state)
{
	m_waveTextureId = state.assetId;
	m_waveTextureHandle = state.textureHandle;
	m_pendingWaveTextureId.reset();
	m_isProxyDirty = true;
}

void WaveRenderer::ApplyBlendModeToRenderTemplates()
{
	m_template.materialDesc.psoKey.blend = GetBlendMode();
}

bool WaveRenderer::ResolveReferences(SceneBase& scene)
{
	if (m_pendingWaveTextureId.has_value())
	{
		const Guid pendingId = m_pendingWaveTextureId.value();
		PreparedTextureAssetState prepared;

		if (PrepareWaveTextureAssetState(prepared, pendingId) == AssetPrepareResult::Ready)
		{
			CommitWaveTextureAssetState(std::move(prepared));
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}