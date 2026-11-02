#pragma once

#include "Backend/DrawPacket.h"
#include "Graph/RenderPass.h"

#include <cstdint>

namespace rendering::graph
{
class RenderPassContext;
}
namespace rendering::detail
{
class DrawResources;
}

namespace rendering
{

class DrawCommand
{
public:
	DrawCommand() = default;

private:
	backend::DrawPacket m_packet;
	std::vector<graph::PassTextureUse> m_reads;

	friend class detail::DrawResources;
	friend class graph::RenderPassContext;
};

} // namespace rendering