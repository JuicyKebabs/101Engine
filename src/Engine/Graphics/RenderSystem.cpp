#include "Engine/Graphics/RenderSystem.h"

void RenderSystem::Register(MeshRenderer* renderer)
{
	if (std::find(m_meshRenderers.begin(), m_meshRenderers.end(), renderer) == m_meshRenderers.end())
	{
		m_meshRenderers.push_back(renderer);
	}
}

void RenderSystem::Unregister(MeshRenderer* renderer)
{
	m_meshRenderers.erase(std::remove(m_meshRenderers.begin(), m_meshRenderers.end(), renderer), m_meshRenderers.end());
}

void RenderSystem::BuildDrawPackets(const CameraInfo& cameraInfo)
{
	std::vector<RenderSystem::SortEntry> sortEntries;	// Temporary list of sort entries to be sorted

	// Create sort entries from registered mesh renderers
	for (const auto& renderer : m_meshRenderers){
		if (renderer->IsVisible() && renderer->IsConfigured())	// Skip invisible or unconfigured renderers
		{
			auto entries = CreateSortEntriesFromRenderer(*renderer, cameraInfo);
			for (auto& entry : entries)
			{
				sortEntries.push_back(entry);
			}
		}
	}

	// Sort draw packets
	SortEntries(sortEntries);

	// Create draw packets from sorted materials
	CreateDrawPacketsFromSortEntries(sortEntries);
}

std::vector<RenderSystem::SortEntry> RenderSystem::CreateSortEntriesFromRenderer(MeshRenderer& renderer, const CameraInfo& cameraInfo)
{
	std::vector<RenderSystem::SortEntry> sortEntries;
	auto& renderTemplate = renderer.GetRenderTemplates();
	auto& proxy = renderer.GetRenderProxy();

	for(auto& submeshTemplate : renderTemplate)
	{
		SortEntry entry;
		entry.renderTemplate = submeshTemplate;
		entry.renderProxy = proxy;
		NormalizeSortEntry(entry);
		entry.sortData = CreateSortData(submeshTemplate, proxy, cameraInfo);
		sortEntries.push_back(entry);
	}

	return sortEntries;
}

RenderQueue RenderSystem::GetRenderQueue(const PSOKey& psoKey)
{
	RenderQueue queue = RenderQueue::Invalied;

	// Determine render queue based on blend mode
	queue = GetRenderQueueFromBlendMode(psoKey.blend);

	return queue;
}

void RenderSystem::NormalizeSortEntry(RenderSystem::SortEntry& entry)
{
	// Force transparent objects to disable depth
	if (entry.sortData.renderQueue == RenderQueue::Transparent
		&& entry.renderTemplate.materialDesc.psoKey.depth != DepthMode::Disable)
	{
		entry.renderTemplate.materialDesc.psoKey.depth = DepthMode::TestNoWrite;
	}
}

void RenderSystem::SortEntries(std::vector<RenderSystem::SortEntry>& entries)
{
	// Separate opaque and transparent materials
	std::vector<RenderSystem::SortEntry> opaque;
	std::vector<RenderSystem::SortEntry> transparent;

	for (auto& entry : entries)
	{
		switch (entry.sortData.renderQueue)
		{
		case RenderQueue::Opaque:
			opaque.push_back(entry);
			break;
		case RenderQueue::Transparent:
			transparent.push_back(entry);
			break;
		default:
			break;
		}
	}

	// Sort opaque objects
	std::stable_sort(opaque.begin(), opaque.end(),
		[](const RenderSystem::SortEntry& a, const RenderSystem::SortEntry& b)
		{
			return OpaqueLess(a.sortData, b.sortData);
		});

	// Sort transparent objects
	std::stable_sort(transparent.begin(), transparent.end(),
		[](const RenderSystem::SortEntry& a, const RenderSystem::SortEntry& b)
		{
			return TransparentLess(a.sortData, b.sortData);
		});


	// Combine opaque and transparent materials back into the original list
	entries.clear();
	entries.insert(entries.end(), opaque.begin(), opaque.end());
	entries.insert(entries.end(), transparent.begin(), transparent.end());
}

RenderSystem::SortData RenderSystem::CreateSortData(const SubmeshRenderTemplate& submeshTemplate, const MeshRenderProxy& renderProxy, const CameraInfo& cameraInfo)
{
	SortData sortData;
	sortData.gpuHandle = submeshTemplate.meshDesc.gpuHandle;
	sortData.startIndex = submeshTemplate.meshDesc.startIndex;
	sortData.baseVertex = submeshTemplate.meshDesc.baseVertex;
	sortData.srvIndex = submeshTemplate.materialDesc.srvIndex;
	sortData.psoKey = submeshTemplate.materialDesc.psoKey;
	sortData.sortDepth = CalculateDepth(renderProxy.position, cameraInfo);
	sortData.renderQueue = GetRenderQueue(sortData.psoKey);
	return sortData;
}

void RenderSystem::CreateDrawPacketsFromSortEntries(std::vector<RenderSystem::SortEntry>& entries)
{
	m_drawPackets.clear();	// Clear previous draw packets

	for (const auto& entry : entries)
	{
		DrawPacket packet;
		packet.meshDesc = entry.renderTemplate.meshDesc;
		packet.materialDesc = entry.renderTemplate.materialDesc;
		packet.worldMatrix = entry.renderProxy.worldMatrix;
		packet.color = entry.renderTemplate.materialDesc.baseColor * entry.renderProxy.color;
		m_drawPackets.push_back(packet);
	}
}

