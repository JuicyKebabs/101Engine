#pragma once
#include "EffectBase.h"
#include "EffectData.h"

//前方宣言
class Renderer;
class InputManager;
class TextureManager;
class MeshManager;

//エフェクト描画情報構造体セット
struct EffectRenderSet
{
	std::vector<WorldRenderInfo> renderInfo;	//描画情報
	EFFECT_TYPE type;				//エフェクトタイプ
};

//エフェクト管理クラス
class EffectManager
{
public:
	const wchar_t* texPath = L"asset/texture/white.png";
	static constexpr int maxEffectNum = 500; //エフェクト最大数

public:
	EffectManager() {};		//コンストラクタ
	~EffectManager() {};	//デストラクタ

	void Initialize(	//初期化
		TextureManager& textureManager,		//テクスチャ管理クラスの参照
		MeshManager& meshManager			//メッシュ管理クラスの参照
	);
	void Update();							//更新
	void SubmitDraws(Renderer& renderer);	//描画要求をシーンに提出
	void Finalize();						//終了

	void AddEffectCommand(EffectCommand command);	//エフェクトコマンド追加

private:
	std::vector<EffectCommand> m_pEffectQueue;			//effect queue
	std::vector<EffectTemplate> m_effectTemplates;		//effect template list
	EffectBase* m_pEffectPool[maxEffectNum]{nullptr};	//active effect pool
	std::vector<EffectRenderSet> m_renderInfos;			//effect render info

private:
	void PrepareRenderInfo(	//オブジェクトの描画情報生成
		TextureManager& textureManager,	//テクスチャ管理クラスの参照
		MeshManager& meshManager		//メッシュ管理クラスの参照
	);

	void SubmitRenderInfo(	//描画情報をシーンに提出
		Renderer& renderer,							//シーンの参照
		const EffectBase& effects,					//エフェクト
		std::vector <WorldRenderInfo>& info	//描画情報構造体
	);

	void PushEffectFromQueue(); //effect queueからeffect poolへエフェクトを追加

	EffectBase* FindFreeEffectInPool(); //effect poolの空きを探す

	EffectBase* FindEffectInPool(EFFECT_TYPE type); //effect poolからエフェクトを探す

	std::vector<WorldRenderInfo>* FindRenderInfo(EFFECT_TYPE type); //エフェクトタイプから描画情報を探す
};