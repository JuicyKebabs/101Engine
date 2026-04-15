#pragma once
#include <cstdint>
#include <vector>
#include "Engine/Core/Math/Math.h"
#include "Engine/Graphics/RenderTemplateFactory.h"

// Enumeration to specify the type of render item
enum class RenderType {
	None,
	Mesh,
	Sprite,
};

// Render item structure for rendering a mesh
struct MeshRenderItem {
	MeshDesc meshDesc;
	MaterialDesc materialDesc;
	Matrix4x4 worldMatrix = {};
	Vector4 color{ 1,1,1,1 };
};

// Render item structure for rendering a sprite
struct SpriteRenderItem {
	MaterialDesc materialDesc;
	Matrix4x4 worldMatrix = {};
	Vector4 color{ 1,1,1,1 };
	Vector2 uvScale{ 1,1 };
	Vector2 uvOffset{ 0,0 };
};

using ItemHandle = uint32_t;	// Unique identifier for render item in queue

// Renference structure for render items in each render queue
struct RenderItemRef {
	RenderType renderType = RenderType::None;
	ItemHandle handle = UINT32_MAX;
	uint64_t sortKey = 0;
};

// Frame render data structure to hold all render items in a single frame
struct FrameRenderData {
	std::vector<MeshRenderItem> meshs;
	std::vector<SpriteRenderItem> sprites;

	std::vector<RenderItemRef> opaque;
	std::vector<RenderItemRef> transparent;

	ItemHandle AddMeshs(MeshRenderItem item) {
		meshs.push_back(std::move(item));
		return static_cast<ItemHandle>(meshs.size() - 1);
	}

	MeshRenderItem& GetMesh(ItemHandle handle) {
		return meshs[handle];
	}

	SpriteRenderItem& GetSprite(ItemHandle handle) {
		return sprites[handle];
	}

	ItemHandle AddSprites(SpriteRenderItem item) {
		sprites.push_back(std::move(item));
		return static_cast<ItemHandle>(sprites.size() - 1);
	}

	void AddOpaque(RenderItemRef item) {
		opaque.push_back(std::move(item));
	}

	void AddTransparent(RenderItemRef item) {
		transparent.push_back(std::move(item));
	}

	void Clear() {
		meshs.clear();
		sprites.clear();
		opaque.clear();
		transparent.clear();
	}
};