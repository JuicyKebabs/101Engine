#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <utility>
#include "Engine/Core/Utility/SharedStruct.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Graphics/RenderData.h"

// UI基底クラス
class UIBase
{
public:	//公開関数
	UIBase() {};	//コンストラクタ
	UIBase(
		Vector3 position = { 0,0,0 },
		Vector3 scale = { 1,1,1 },
		Vector3 rotation = { 0,0,0 },
		UINT order = 0,
		PSOKey key = PSOKey{}
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
	//void CollectRenderInfos(std::vector<WorldRenderInfo>& out) const;	//描画情報構造体配列収集

	//ゲッター
	const Transform3D& GetWorldTransform() const;	//ワールド変換情報の取得
	const Transform3D& GetLocalTransform() const;	//ローカル変換情報の取得
	const Vector4 GetColor() const;					//色RGBAの取得
	const bool IsActive() const;					//アクティブかどうかを取得
	int GetOrder() const;							//描画順の取得
	UVRect GetUVRect() const;						//UV矩形の取得
	TexSplitInfo& GetTexSplitInfo();				//テクスチャ分割情報構造体の取得
	Vector3 GetWorldPosition() const { return m_worldPosition; }	//ワールド位置の取得
	Vector3 GetWorldScale() const { return m_worldScale; }		//ワールドスケールの取得
	Vector3 GetWorldRotation() const { return m_worldRotation; }	//ワールド回転の取得
	Vector3 GetLocalPosition() const { return m_localPosition; }	//ローカル位置の取得
	Vector3 GetLocalScale() const { return m_localScale; }		//ローカルスケールの取得
	Vector3 GetLocalRotation() const { return m_localRotation; }	//ローカル回転の取得

	//セッター
	void SetLocalTransform(const Transform3D& local); //ローカル変換情報の設定
	void SetColor(Vector4 color);	//色RGBAの設定
	void SetActive(bool isActive);			//アクティブフラグの設定
	void SetOrder(int order);				//描画順の設定
	void SetUVRect(const UVRect& uvRect);	//UV矩形の設定
	void SetTexSplitInfo(const TexSplitInfo& info);	//テクスチャ分割情報構造体の設定
	void SetWorldPosition(Vector3 position) { m_worldPosition = position; }	//ワールド位置の設定
	void SetWorldScale(Vector3 scale) { m_worldScale = scale; }				//ワールドスケールの設定
	void SetWorldRotation(Vector3 rotation) { m_worldRotation = rotation; }	//ワールド回転の設定
	void SetLocalPosition(Vector3 position) { m_localPosition = position; }	//ローカル位置の設定
	void SetLocalScale(Vector3 scale) { m_localScale = scale; }				//ローカルスケールの設定
	void SetLocalRotation(Vector3 rotation) { m_localRotation = rotation; }	//ローカル回転の設定

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

	void SortChildrenByOrder();	//子UIの描画順の更新(昇順)

protected:
	Vector3 m_localPosition{};
	Vector3 m_localScale{};	
	Vector3 m_localRotation{};
	Vector3 m_worldPosition{};
	Vector3 m_worldScale{};
	Vector3 m_worldRotation{};

	Transform3D m_world{};	//ワールド変換情報
	Transform3D m_local{};	//ローカル変換情報

	Vector4 m_color{ 1.0f, 1.0f, 1.0f, 1.0f };	//色
	bool m_isActive = true;								//アクティブフラグ

	//UI親子関係
	UIBase* m_parent = nullptr;							//親UIオブジェクトポインタ
	std::vector<std::unique_ptr<UIBase>> m_children;	//子UIオブジェクト配列

	UINT m_order = 0;									//描画順
	//std::vector<WorldRenderInfo> m_renderInfos;			//描画情報構造体配列
	PSOKey m_psoKey{};									//パイプラインステートオブジェクトキー

	UVRect m_uvRect{};				//UV矩形
	TexSplitInfo m_texSplitInfo{};	//テクスチャ分割情報構造体

private:
	void UpdateTexSplitInfo();	//テクスチャ分割情報構造体の更新
	void UpdateTransform();	//ローカル変換情報からワールド変換情報を更新
};