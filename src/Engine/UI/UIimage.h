#pragma once
#include "UIRenderer.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Core/Math/Math.h"

class UIImage : public UIRenderer
{
public:
	struct InitDesc : public UIRenderer::InitDesc
	{
		InitDesc(const std::string& name = "UIImage") : UIRenderer::InitDesc(name) {}
		InitDesc(const UIRenderTemplate& renderTemplate, UINT order, const Vector4& color, const Vector2& uvScale, const Vector2& uvOffset, const Vector2& pivot, bool flipX, bool flipY, const std::string& name = "UIImage")
			: UIRenderer::InitDesc(renderTemplate, order, color, uvScale, uvOffset, pivot, flipX, flipY, name) {}
	};

public:
	UIImage() = default;
	~UIImage() = default;
	void Init(const InitDesc& desc) { UIRenderer::Init(desc); }

private:
	// Override functions for component lifecycle
	void OnStartOverride() override {};
	void PreUpdateOverride(float deltaTime) override {};
	void UpdateOverride(float deltaTime) override {};
	void LateUpdateOverride(float deltaTime) override {};
	void OnDestroyOverride() override {};
};
