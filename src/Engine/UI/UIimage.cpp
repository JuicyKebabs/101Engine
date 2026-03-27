#include "UIimage.h"

//コンストラクタ
UIImage::UIImage(
	Vector3 position,
	Vector3 scale,
	Vector3 rotation,
	UINT order,
	const std::wstring& texturePath,
	PSOKey psoKey
)
	: UIBase(position, scale, rotation, order, psoKey),
	m_texturePath(texturePath)
{
}

//初期化
void UIImage::InitializeOverride(TextureManager& textureManager, MeshManager& meshManager)
{
}

//更新
void UIImage::UpdateOverride()
{
}

//終了
void UIImage::FinalizeOverride()
{
}

//
void UIImage::PrepareRenderInfoOverride(TextureManager& textureManager, MeshManager& meshManager)
{
	//m_renderInfos.clear();	//既存の描画情報構造体配列をクリア

	////描画情報生成関数を呼び出し、描画情報を作成
	//CreateRenderInfo(
	//	textureManager,			//テクスチャマネージャへの参照
	//	meshManager,			//メッシュマネージャへの参照
	//	&m_renderInfos,			//描画情報構造体配列へのポインタ
	//	DEFAULT_MESH::QUAD,		//メッシュタイプ
	//	m_psoKey,				//ブレンドモード
	//	m_texturePath.c_str()	//テクスチャのファイル名
	//);
}