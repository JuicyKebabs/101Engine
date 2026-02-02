#pragma once
#include <d3d12.h>
#include "d3dx12.h"
#include <memory>
#include <vector>
#include <utility>
#include "SharedStruct.h"
#include "RenderData.h"

// UI基底クラス
class UIBase
{
public:	//公開関数
	UIBase() {};	//コンストラクタ
	UIBase(
		DirectX::XMFLOAT3 position = { 0,0,0 },
		DirectX::XMFLOAT3 scale = { 1,1,1 },
		DirectX::XMFLOAT3 rotation = { 0,0,0 },
		UINT order = 0,
		BLEND_MODE blendMode = BLEND_MODE::BLEND_TRANSPARENT
	);
	virtual ~UIBase() = default;	//デストラクタ
	//メイン処理関数
	void Initialize(		//初期化
		TextureManager& textureManager,
		MeshManager& meshManager
	);
	void Update();		//更新
	void Finalize();	//終了

	void UpdateWorldTransform(const Transform3D& parent);	//ワールド変換情報更新

	//描画関連関数
	void PrepareRenderInfo(	//オブジェクトの描画情報生成
		TextureManager& textureManager,
		MeshManager& meshManager
	);
	void CollectRenderInfos(std::vector<WorldRenderInfo>& out) const;	//描画情報構造体配列収集

	//ゲッター
	const Transform3D& GetWorldTransform() const;	//ワールド変換情報の取得
	const Transform3D& GetLocalTransform() const;	//ローカル変換情報の取得
	const DirectX::XMFLOAT4 GetColor() const;		//色RGBAの取得
	const bool IsActive() const;					//アクティブかどうかを取得
	int GetOrder() const;							//描画順の取得
	UVRect GetUVRect() const;						//UV矩形の取得
	TexSplitInfo& GetTexSplitInfo();				//テクスチャ分割情報構造体の取得

	//セッター
	void SetLocalTransform(const Transform3D& local); //ローカル変換情報の設定
	void SetColor(DirectX::XMFLOAT4 color);	//色RGBAの設定
	void SetActive(bool isActive);			//アクティブフラグの設定
	void SetUVRect(const UVRect& uvRect);	//UV矩形の設定
	void SetTexSplitInfo(const TexSplitInfo& info);	//テクスチャ分割情報構造体の設定

	//UI親子関係関数
	//子UIオブジェクト追加関数(テンプレート)
	template<class T, class... Args>
	T* AddChild(Args&&... args)
	{
		static_assert(std::is_base_of_v<UIBase, T>, "AddChild<T>: T must derive from UIBase");
		auto c = std::make_unique<T>(std::forward<Args>(args)...);
		c->m_parent = this;
		T* ptr = c.get();
		m_children.emplace_back(std::move(c));
		return ptr;
	}

protected:
	//仮想関数群
	virtual void InitializeOverride(		//初期化(派生クラスでオーバーライド)
		TextureManager& textureManager,
		MeshManager& meshManager
	) = 0;
	virtual void UpdateOverride() = 0;		//更新(派生クラスでオーバーライド)
	virtual void FinalizeOverride() = 0;	//終了(派生クラスでオーバーライド)
	virtual void PrepareRenderInfoOverride(	//オブジェクトの描画情報生成(派生クラスでオーバーライド)
		TextureManager& textureManager,
		MeshManager& meshManager
	) = 0;

protected:
	Transform3D m_world{};	//ワールド変換情報
	Transform3D m_local{};	//ローカル変換情報

	DirectX::XMFLOAT4 m_color{ 1.0f, 1.0f, 1.0f, 1.0f };	//色
	bool m_isActive = true;								//アクティブフラグ

	//UI親子関係
	UIBase* m_parent = nullptr;							//親UIオブジェクトポインタ
	std::vector<std::unique_ptr<UIBase>> m_children;	//子UIオブジェクト配列

	UINT m_order = 0;										//描画順
	std::vector<WorldRenderInfo> m_renderInfos;				//描画情報構造体配列
	BLEND_MODE m_blendMode = BLEND_MODE::BLEND_TRANSPARENT;	//ブレンドモード

	UVRect m_uvRect{};				//UV矩形
	TexSplitInfo m_texSplitInfo{};	//テクスチャ分割情報構造体
};