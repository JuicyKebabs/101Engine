#include "Engine/Graphics/RenderSystem.h"
#include "Engine/Resource/MeshGPU.h"

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

void RenderSystem::Register(UIRenderer* renderer)
{
	if (std::find(m_uiRenderers.begin(), m_uiRenderers.end(), renderer) == m_uiRenderers.end())
	{
		m_uiRenderers.push_back(renderer);
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

void RenderSystem::Unregister(UIRenderer* renderer)
{
	m_uiRenderers.erase(std::remove(m_uiRenderers.begin(), m_uiRenderers.end(), renderer), m_uiRenderers.end());
}

void RenderSystem::FlushRegisters()
{
	for(auto& renderer : m_meshRenderers)
	{
		renderer->Flush();
	}

	for(auto& renderer : m_spriteRenderers)
	{
		renderer->Flush();
	}

	for(auto& renderer : m_uiRenderers)
	{
		renderer->Flush();
	}
}

void RenderSystem::BuildFrameRenderData(const CameraInfo& cameraInfo)
{
	m_cameraInfo = cameraInfo;	// Update cached camera info for this frame

	m_frameRenderData.Clear();	// Clear previous frame's render data
	m_frameSortData.Clear();	// Clear previous frame's sort data

	// Build draw packets for mesh renderers
	for (const auto& renderer : m_meshRenderers){
		if (renderer->IsVisible() && renderer->IsConfigured())	// Skip invisible or unconfigured renderers
		{
			const auto& renderTemplates = renderer->GetRenderTemplates();
			const auto& renderProxy = renderer->GetRenderProxy();

			for (const auto& renderTemplate : renderTemplates) 
			{
				auto item = CreateMeshRenderItem(renderTemplate, renderProxy);
				RenderQueue queue = GetRenderQueue(item.common.materialDesc.psoKey);
				NormalizePSOKey(item.common.materialDesc.psoKey, queue);
				auto handle = m_frameRenderData.AddMeshs(item);

				RenderItemRef ref;
				ref.renderType = RenderType::Mesh;
				ref.handle = handle;

				if (queue == RenderQueue::Opaque)
				{
					SortKeyOpaque opaqueKey;
					opaqueKey.psoKey = item.common.materialDesc.psoKey;
					ref.sortKey = m_frameSortData.AddOpaqueKey(opaqueKey);
					m_frameRenderData.AddOpaque(ref);
				}
				else
				{
					SortKeyTransparent transparentKey;
					transparentKey.depth = CalculateDepth(renderProxy.common.position, m_cameraInfo);
					ref.sortKey = m_frameSortData.AddTransparentKey(transparentKey);
					m_frameRenderData.AddTransparent(ref);
				}
			}
		}
	}

	// Build draw packets for sprite renderers
	for(const auto& renderer : m_spriteRenderers)
	{
		if (renderer->IsVisible() && renderer->IsConfigured())	// Skip invisible or unconfigured renderers
		{
			const auto& renderTemplate = renderer->GetRenderTemplate();
			const auto& renderProxy = renderer->GetRenderProxy(m_cameraInfo);
			auto item = CreateSpriteRenderItem(renderTemplate, renderProxy);
			RenderQueue queue = GetRenderQueue(item.common.materialDesc.psoKey);
			NormalizePSOKey(item.common.materialDesc.psoKey, queue);
			auto handle = m_frameRenderData.AddSprites(item);

			RenderItemRef ref;
			ref.renderType = RenderType::Sprite;
			ref.handle = handle;

			if (queue == RenderQueue::Opaque)
			{
				SortKeyOpaque opaqueKey;
				opaqueKey.psoKey = item.common.materialDesc.psoKey;
				ref.sortKey = m_frameSortData.AddOpaqueKey(opaqueKey);
				m_frameRenderData.AddOpaque(ref);
			}
			else
			{
				SortKeyTransparent transparentKey;
				transparentKey.psoKey = item.common.materialDesc.psoKey;
				transparentKey.depth = CalculateDepth(renderProxy.common.position, m_cameraInfo);
				ref.sortKey = m_frameSortData.AddTransparentKey(transparentKey);
				m_frameRenderData.AddTransparent(ref);
			}
		}
	}

	// Build draw packets for UI renderers
	for(const auto& renderer : m_uiRenderers)
	{
		if (renderer->IsVisible() && renderer->IsConfigured())	// Skip invisible or unconfigured renderers
		{
			const auto& renderTemplate = renderer->GetRenderTemplate();
			const auto& renderProxy = renderer->GetRenderProxy();

			for (const auto& element : renderTemplate)
			{
				auto item = CreateUIRenderItem(element, renderProxy);
				auto handle = m_frameRenderData.AddUI(item);
				RenderItemRef ref;
				ref.renderType = RenderType::UI;
				ref.handle = handle;
				SortKeyScreenSpace ssKey;
				ssKey.psoKey = item.common.materialDesc.psoKey;
				ssKey.canvasOrder = renderProxy.canvasOrder;
				ssKey.order = renderProxy.order;
				ref.sortKey = m_frameSortData.AddScreenSpaceKey(ssKey);
				m_frameRenderData.AddScreenSpace(ref);
			}
		}
	}

	SortOpaque();		// Sort opaque draw packets
	SortTransparent();	// Sort transparent draw packets
	SortScreenSpace();	// Sort screen-space draw packets (e.g., UI)
}

MeshRenderItem RenderSystem::CreateMeshRenderItem(const SubmeshRenderTemplate& renderTemplate, const MeshRendererProxy& renderProxy)
{
	MeshRenderItem item;
	item.meshDesc = renderTemplate.meshDesc;
	item.common.materialDesc = renderTemplate.materialDesc;
	item.common.worldMatrix = renderProxy.common.worldMatrix;
	item.common.color = renderProxy.common.color * renderTemplate.materialDesc.baseColor;
	return item;
}

SpriteRenderItem RenderSystem::CreateSpriteRenderItem(const SpriteRenderTemplate& renderTemplate, const SpriteRendererProxy& renderProxy)
{
	SpriteRenderItem item;
	item.common.materialDesc = renderTemplate.materialDesc;
	item.common.worldMatrix = renderProxy.common.worldMatrix;
	item.common.color = renderProxy.common.color * renderTemplate.materialDesc.baseColor;
	item.uvScale = renderProxy.uvScale;
	item.uvOffset = renderProxy.uvOffset;
	item.pivot = renderProxy.pivot;
	item.flip = renderProxy.flip;
	return item;
}

UIRenderItem RenderSystem::CreateUIRenderItem(const UIRenderElement& renderTemplate, const UIRendererProxy& renderProxy)
{
	UIRenderItem item;
	item.common.materialDesc = renderTemplate.materialDesc;
	item.common.worldMatrix = renderProxy.common.worldMatrix;
	item.common.color = renderProxy.common.color * renderTemplate.materialDesc.baseColor;
	item.uvScale = renderProxy.uvScale;
	item.uvOffset = renderProxy.uvOffset;
	item.flip = renderProxy.flip;
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

void RenderSystem::SortScreenSpace()
{
	std::stable_sort(m_frameRenderData.screenspace.begin(), m_frameRenderData.screenspace.end(),
		[this](const RenderItemRef& a, const RenderItemRef& b)
		{
			const auto& ka = m_frameSortData.GetScreenSpaceKey(a.sortKey);
			const auto& kb = m_frameSortData.GetScreenSpaceKey(b.sortKey);
			if (ka.canvasOrder != kb.canvasOrder) return ka.canvasOrder < kb.canvasOrder;
			if (ka.order != kb.order)       return ka.order < kb.order;
			return PSOKeyLess(ka.psoKey, kb.psoKey);		
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