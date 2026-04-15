#include "Engine/Graphics/RenderSystem.h"

void RenderSystem::Register(MeshRenderer* renderer)
{
	if (std::find(m_meshRenderers.begin(), m_meshRenderers.end(), renderer) == m_meshRenderers.end())
	{
		m_meshRenderers.push_back(renderer);
	}
}

void RenderSystem::Register(SpriteRenderer* renderer)
{
	if (std::find(m_spriteRenderers.begin(), m_spriteRenderers.end(), renderer) == m_spriteRenderers.end())
	{
		m_spriteRenderers.push_back(renderer);
	}
}

void RenderSystem::Unregister(MeshRenderer* renderer)
{
	m_meshRenderers.erase(std::remove(m_meshRenderers.begin(), m_meshRenderers.end(), renderer), m_meshRenderers.end());
}

void RenderSystem::Unregister(SpriteRenderer* renderer)
{
	m_spriteRenderers.erase(std::remove(m_spriteRenderers.begin(), m_spriteRenderers.end(), renderer), m_spriteRenderers.end());
}

void RenderSystem::BuildFrameRenderData(const CameraInfo& cameraInfo)
{
	m_cameraInfo = cameraInfo;	// Update cached camera info for this frame

	m_frameRenderData.Clear();	// Clear previous frame's render data
	m_frameSortData.Clear();	// Clear previous frame's sort data

	for (const auto& renderer : m_meshRenderers){
		if (renderer->IsVisible() && renderer->IsConfigured())	// Skip invisible or unconfigured renderers
		{
			const auto& renderTemplates = renderer->GetRenderTemplates();
			const auto& renderProxy = renderer->GetRenderProxy();

			for (const auto& renderTemplate : renderTemplates) 
			{
				auto handle = m_frameRenderData.AddMeshs(CreateMeshRenderItem(renderTemplate, renderProxy));

				RenderQueue queue = GetRenderQueue(renderTemplate.materialDesc.psoKey);
				NormalizePSOKey(m_frameRenderData.GetMesh(handle).materialDesc.psoKey, queue);

				RenderItemRef ref;
				ref.renderType = RenderType::Mesh;
				ref.handle = handle;

				if (queue == RenderQueue::Opaque)
				{
					SortKeyOpaque opaqueKey;
					opaqueKey.psoKey = renderTemplate.materialDesc.psoKey;
					ref.sortKey = m_frameSortData.AddOpaqueKey(opaqueKey);
					m_frameRenderData.AddOpaque(ref);
				}
				else
				{
					SortKeyTransparent transparentKey;
					transparentKey.psoKey = renderTemplate.materialDesc.psoKey;
					transparentKey.depth = CalculateDepth(renderProxy.position, m_cameraInfo);
					ref.sortKey = m_frameSortData.AddTransparentKey(transparentKey);
					m_frameRenderData.AddTransparent(ref);
				}
			}
		}
	}

	for(const auto& renderer : m_spriteRenderers)
	{
		if (renderer->IsVisible() && renderer->IsConfigured())	// Skip invisible or unconfigured renderers
		{
			const auto& renderTemplate = renderer->GetRenderTemplate();
			const auto& renderProxy = renderer->GetRenderProxy();
			auto handle = m_frameRenderData.AddSprites(CreateSpriteRenderItem(renderTemplate, renderProxy));

			RenderQueue queue = GetRenderQueue(renderTemplate.materialDesc.psoKey);
			NormalizePSOKey(m_frameRenderData.GetSprite(handle).materialDesc.psoKey, queue);

			RenderItemRef ref;
			ref.renderType = RenderType::Sprite;
			ref.handle = handle;

			if (queue == RenderQueue::Opaque)
			{
				SortKeyOpaque opaqueKey;
				opaqueKey.psoKey = renderTemplate.materialDesc.psoKey;
				ref.sortKey = m_frameSortData.AddOpaqueKey(opaqueKey);
				m_frameRenderData.AddOpaque(ref);
			}
			else
			{
				SortKeyTransparent transparentKey;
				transparentKey.psoKey = renderTemplate.materialDesc.psoKey;
				transparentKey.depth = CalculateDepth(renderProxy.position, m_cameraInfo);
				ref.sortKey = m_frameSortData.AddTransparentKey(transparentKey);
				m_frameRenderData.AddTransparent(ref);
			}
		}
	}

	SortOpaque();		// Sort opaque draw packets
	SortTransparent();	// Sort transparent draw packets
}

MeshRenderItem RenderSystem::CreateMeshRenderItem(const SubmeshRenderTemplate& renderTemplate, const MeshRendererProxy& renderProxy)
{
	MeshRenderItem item;
	item.meshDesc = renderTemplate.meshDesc;
	item.materialDesc = renderTemplate.materialDesc;
	item.worldMatrix = renderProxy.worldMatrix;
	item.color = renderProxy.color;
	return item;
}

SpriteRenderItem RenderSystem::CreateSpriteRenderItem(const SpriteRenderTemplate& renderTemplate, const SpriteRendererProxy& renderProxy)
{
	SpriteRenderItem item;
	item.materialDesc = renderTemplate.materialDesc;
	item.worldMatrix = renderProxy.worldMatrix;
	item.color = renderProxy.color;
	item.uvScale = renderProxy.uvScale;
	item.uvOffset = renderProxy.uvOffset;
	return item;
}

void RenderSystem::SortOpaque()
{
	std::stable_sort(m_frameRenderData.opaque.begin(), m_frameRenderData.opaque.end(),
		[this](const RenderItemRef& a, const RenderItemRef& b)
		{
			return OpaqueLess(
				m_frameSortData.GetOpaqueKey(a.sortKey),
				m_frameSortData.GetOpaqueKey(b.sortKey)
			);
		}
	);
}

void RenderSystem::SortTransparent()
{
	std::stable_sort(m_frameRenderData.transparent.begin(), m_frameRenderData.transparent.end(),
		[this](const RenderItemRef& a, const RenderItemRef& b)
		{
			return TransparentLess(
				m_frameSortData.GetTransparentKey(a.sortKey),
				m_frameSortData.GetTransparentKey(b.sortKey)
			);
		}
	);
}

RenderQueue RenderSystem::GetRenderQueue(const PSOKey& psoKey)
{
	RenderQueue queue = RenderQueue::Invalied;

	// Determine render queue based on blend mode
	queue = GetRenderQueueFromBlendMode(psoKey.blend);

	return queue;
}

void RenderSystem::NormalizePSOKey(PSOKey& psoKey, RenderQueue queue)
{
	// Force transparent objects to disable depth
	if (queue == RenderQueue::Transparent
		&& psoKey.depth != DepthMode::Disable)
	{
		psoKey.depth = DepthMode::TestNoWrite;
	}
}

//void RenderSystem::SortEntries(std::vector<RenderSystem::SortEntry>& entries)
//{
//	// Separate opaque and transparent materials
//	std::vector<RenderSystem::SortEntry> opaque;
//	std::vector<RenderSystem::SortEntry> transparent;
//
//	for (auto& entry : entries)
//	{
//		switch (entry.sortData.renderQueue)
//		{
//		case RenderQueue::Opaque:
//			opaque.push_back(entry);
//			break;
//		case RenderQueue::Transparent:
//			transparent.push_back(entry);
//			break;
//		default:
//			break;
//		}
//	}
//
//	// Sort opaque objects
//	std::stable_sort(opaque.begin(), opaque.end(),
//		[](const RenderSystem::SortEntry& a, const RenderSystem::SortEntry& b)
//		{
//			return OpaqueLess(a.sortData, b.sortData);
//		});
//
//	// Sort transparent objects
//	std::stable_sort(transparent.begin(), transparent.end(),
//		[](const RenderSystem::SortEntry& a, const RenderSystem::SortEntry& b)
//		{
//			return TransparentLess(a.sortData, b.sortData);
//		});
//
//
//	// Combine opaque and transparent materials back into the original list
//	entries.clear();
//	entries.insert(entries.end(), opaque.begin(), opaque.end());
//	entries.insert(entries.end(), transparent.begin(), transparent.end());
//}
