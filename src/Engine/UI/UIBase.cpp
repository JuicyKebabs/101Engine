#include "UIBase.h"

using namespace DirectX;

//コンストラクタ
UIBase::UIBase(
	Vector3 position,
	Vector3 scale,
	Vector3 rotation,
	UINT order,
	PSOKey psoKey
)
	: 
	m_localPosition(position),
	m_localScale(scale),
	m_localRotation(rotation),
	m_order(order),
	m_psoKey(psoKey)
{
	m_local.position = position;
	m_local.scale = scale;
	XMVECTOR qRotation = XMQuaternionRotationRollPitchYaw(
		XMConvertToRadians(rotation.x),
		XMConvertToRadians(rotation.y),
		XMConvertToRadians(rotation.z)
	);
	XMStoreFloat4(&m_local.rotation, qRotation);
}

// 初期化
void UIBase::Initialize(
	TextureManager& textureManager,
	MeshManager& meshManager
)
{
	InitializeOverride(textureManager, meshManager);
	for (auto& child : m_children)
	{
		child->Initialize(textureManager, meshManager);
	}
}

// 更新
void UIBase::Update()
{
	UpdateOverride();
	UpdateTexSplitInfo();
	UpdateTransform();
	for (auto& child : m_children)
	{
		child->Update();
	}
	SortChildrenByOrder();
}

// 終了
void UIBase::Finalize()
{
	FinalizeOverride();
	for (auto& child : m_children)
	{
		child->Finalize();
	}
}

//オブジェクトの描画情報生成
void UIBase::PrepareRenderInfo(
	TextureManager& textureManager,
	MeshManager& meshManager
)
{
	//自身の描画情報生成
	PrepareRenderInfoOverride(textureManager, meshManager);

	//子UIオブジェクトの描画情報生成
	for (auto& child : m_children)
	{
		child->PrepareRenderInfo(textureManager, meshManager);
	}
}

//描画情報構造体配列収集
//void UIBase::CollectRenderInfos(std::vector<WorldRenderInfo>& out) const
//{
//	if (!m_isActive) return;	//非アクティブなら何もしない
//	//自身の描画情報構造体配列を収集
//
//	auto worldMatrix = GetMatrixFromTransform3D(m_world);	//ワールド行列を取得
//
//	for (const auto& renderInfo : m_renderInfos) {
//		WorldRenderInfo renderInfoCopy = renderInfo;				//描画情報構造体をコピー
//		renderInfoCopy.worldMatrix = worldMatrix;							//ワールド行列を設定
//		renderInfoCopy.common.color = m_color;						//色RGBAを設定
//		renderInfoCopy.common.uvRect = SplitSprite(m_texSplitInfo);	//UV矩形を設定
//		out.push_back(renderInfoCopy);								//配列に追加
//	}
//
//	//子UIオブジェクトの描画情報構造体配列を収集
//	for (const auto& child : m_children) {
//		child->CollectRenderInfos(out);
//	}
//}

// ワールド変換情報の取得
const Transform3D& UIBase::GetWorldTransform() const
{
	return m_world;
}

// ローカル変換情報の取得
const Transform3D& UIBase::GetLocalTransform() const
{
	return m_local;
}

// 色RGBAの取得
const Vector4 UIBase::GetColor() const
{
	return m_color;
}

// アクティブかどうかを取得
const bool UIBase::IsActive() const
{
	return m_isActive;
}

//描画順の取得
int UIBase::GetOrder() const
{
	return m_order;
}

//UV矩形の取得
UVRect UIBase::GetUVRect() const
{
	return m_uvRect;
}

//テクスチャ分割情報構造体の取得
TexSplitInfo& UIBase::GetTexSplitInfo()
{
	return m_texSplitInfo;
}

//ローカル変換情報の設定
void UIBase::SetLocalTransform(const Transform3D& local)
{
	m_local = local;
}

// 色RGBAの設定
void UIBase::SetColor(Vector4 color) {
	m_color = color;
}

// アクティブフラグの設定
void UIBase::SetActive(bool isActive) {
	m_isActive = isActive;
}

//描画順の設定
void UIBase::SetOrder(int order)
{
	m_order = order;
}

//UV矩形の設定
void UIBase::SetUVRect(const UVRect& uvRect)
{
	m_uvRect.u = uvRect.u;
	m_uvRect.v = uvRect.v;
	m_uvRect.su = uvRect.su;
	m_uvRect.sv = uvRect.sv;
}

//テクスチャ分割情報構造体の設定
void UIBase::SetTexSplitInfo(const TexSplitInfo& info)
{
	m_texSplitInfo = info;
}

void UIBase::SortChildrenByOrder()
{
	std::stable_sort(
		m_children.begin(),
		m_children.end(),
		[](const std::unique_ptr<UIBase>& a, const std::unique_ptr<UIBase>& b) {
			return a->GetOrder() < b->GetOrder();
		}
	);

	for(auto& child : m_children) {
		child->SortChildrenByOrder();
	}
}

void UIBase::UpdateTexSplitInfo()
{
	if (m_texSplitInfo.total <= 1 || m_texSplitInfo.updateRate <= 0) return;

	m_texSplitInfo.frameCount++;	//フレームカウントをインクリメント

	//更新頻度に達したらインデックスを更新
	if (m_texSplitInfo.frameCount >= m_texSplitInfo.updateRate)
	{
		m_texSplitInfo.frameCount = 0;	//フレームカウントリセット
		m_texSplitInfo.index++;			//インデックスをインクリメント

		//インデックスが総数を超えたらリセット
		if (m_texSplitInfo.index >= m_texSplitInfo.total)
		{
			m_texSplitInfo.index = 0;
		}
	}
}

void UIBase::UpdateTransform()
{
	m_local.position = m_localPosition;
	m_local.scale = m_localScale;
	XMVECTOR qRotation = XMQuaternionRotationRollPitchYaw(
		XMConvertToRadians(m_localRotation.x),
		XMConvertToRadians(m_localRotation.y),
		XMConvertToRadians(m_localRotation.z)
	);
	XMStoreFloat4(&m_local.rotation, qRotation);
}

// ワールド変換情報更新
void UIBase::UpdateWorldTransform(const Transform3D& parent)
{
	m_world = CombineTransform3D(parent, m_local);	//親の変換情報とローカル変換情報を合成してワールド変換情報を更新
	for (auto& child : m_children) {
		child->UpdateWorldTransform(m_world);	//子のワールド変換情報も更新
	}
}
