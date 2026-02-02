#pragma once
#include <d3d12.h>
#include "d3dx12.h"
#include "UIBase.h"

class UIImage : public UIBase
{
public:
	UIImage(
		DirectX::XMFLOAT3 position = { 0,0,0 },
		DirectX::XMFLOAT3 scale = { 1,1,1 },
		DirectX::XMFLOAT3 rotation = { 0,0,0 },
		UINT order = 0,
		const std::wstring& texturePath = L"",
		BLEND_MODE blendMode = BLEND_MODE::BLEND_TRANSPARENT
	);
	~UIImage() {};
	void InitializeOverride(
		TextureManager& textureManager,
		MeshManager& meshManager
	) override;
	void UpdateOverride() override;
	void FinalizeOverride() override;

	void SetTexturePath(const std::wstring& texturePath) { m_texturePath = texturePath; }

private:
	std::wstring m_texturePath;							//テクスチャパス

protected:
	void PrepareRenderInfoOverride(	//オブジェクトの描画情報生成
		TextureManager& textureManager,	//テクスチャ管理クラスの参照
		MeshManager& meshManager		//メッシュ管理クラスの参照
	) override;
};
