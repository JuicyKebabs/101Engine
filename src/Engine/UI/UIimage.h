#pragma once
#include <d3d12.h>
#include "UIBase.h"

class UIImage : public UIBase
{
public:
	UIImage(
		Vector3 position = { 0,0,0 },
		Vector3 scale = { 1,1,1 },
		Vector3 rotation = { 0,0,0 },
		UINT order = 0,
		const std::wstring& texturePath = L"",
		PSOKey psoKey = PSO_KEY_DEFAULT::SPRITE_TRANSPARENT
	);
	~UIImage() {};
	void InitializeOverride(
		TextureManager& textureManager,
		MeshManager& meshManager
	) override;
	void UpdateOverride() override;
	void FinalizeOverride() override;

	void SetTexturePath(const std::wstring& texturePath) { m_texturePath = texturePath; }

protected:
	void PrepareRenderInfoOverride(	//オブジェクトの描画情報生成
		TextureManager& textureManager,	//テクスチャ管理クラスの参照
		MeshManager& meshManager		//メッシュ管理クラスの参照
	) override;

private:
	std::wstring m_texturePath;							//テクスチャパス
};
