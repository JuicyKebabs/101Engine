#pragma once
#include "EffectBase.h"
#include "EffectSprite.h"
#include "EffectData.h"
#include "EventType.h"

//前方宣言
class Renderer;
class InputManager;
class TextureManager;
class MeshManager;

//effect render set structure
struct EffectRenderSet
{
	std::vector<EffectRenderInfo> renderInfo;	//描画情報
	EFFECT_TYPE type;							//エフェクトタイプ
};

//effect manager class
class EffectManager
{
public:
	const wchar_t* texPath = L"asset/texture/white.png";
	static constexpr int maxEffectNum = 5000; //エフェクト最大数

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

	void AddEffect(EffectCommand command);		//エフェクトコマンド追加

private:
	std::vector<EffectCommand> m_pEffectQueue;			//effect queue
	std::vector<EffectTemplate> m_effectTemplates;		//effect template list
	EffectSprite* m_pEffectPool[maxEffectNum]{ nullptr };	//active effect pool
	std::vector<EffectRenderSet> m_renderInfos;			//effect render info

	std::vector<EventData> m_eventDataList; // Subscribed event data list

private:
	void PrepareRenderInfo(	//オブジェクトの描画情報生成
		TextureManager& textureManager,	//テクスチャ管理クラスの参照
		MeshManager& meshManager		//メッシュ管理クラスの参照
	);

	void SubmitRenderInfo(	//描画情報をシーンに提出
		Renderer& renderer,							//シーンの参照
		const EffectBase& effects,					//エフェクト
		EffectRenderModel& info	//描画情報構造体
	);

	void PushEffectFromQueue();		//push effect from queue to pool
	EffectSprite* FindFreeEffectInPool(); //effect poolの空きを探す

	std::vector<EffectRenderInfo>* FindRenderInfo(EFFECT_TYPE type); //エフェクトタイプから描画情報を探す
};