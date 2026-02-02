#include "EffectManager.h"
#include "Renderer.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "EventManager.h"
#include "EffectSprite.h"

using namespace DirectX;

//初期化
void EffectManager::Initialize(TextureManager& textureManager, MeshManager& meshManager)
{
	for (auto& ptr : m_pEffectPool)
	{
		if (ptr) continue;
		ptr = new EffectSprite(); //エフェクトインスタンス生成
		ptr->SetActive(false);
	}
	auto templateSet = GetEffectTemplate();

	//エフェクトテンプレートリストのコピー
	m_effectTemplates.reserve(templateSet.templateCount);

	for (int i = 0; i < templateSet.templateCount; ++i)
	{
		m_effectTemplates.push_back(templateSet.pTemplates[i]);
	}

	//オブジェクト描画情報生成
	PrepareRenderInfo(textureManager, meshManager);


	using args = EffectCommand;
	uint64_t id = EventManager::GetInstance()->Subscribe<args>(
		EventType::ADD_EFFECT_COMMAND,
		[this](std::shared_ptr<args> data)
		{
			AddEffect(*data);
		}
	);
	m_eventDataList.push_back({ EventType::ADD_EFFECT_COMMAND, id });
}

//更新
void EffectManager::Update()
{
	//effect queueからeffect poolへエフェクトを追加
	PushEffectFromQueue();

	//エフェクト更新
	for (auto& effect : m_pEffectPool)
	{
		if (!effect || !effect->IsActive()) continue;

		effect->Update();
	}
}

//描画要求をシーンに提出
void EffectManager::SubmitDraws(Renderer& renderer)
{
	for (auto& effect : m_pEffectPool)
	{
		if (!effect || !effect->IsActive()) continue;

		auto renderInfo = FindRenderInfo(effect->GetType());

		SubmitRenderInfo(renderer, *effect, *renderInfo);
	}
}

//終了
void EffectManager::Finalize()
{
	//unsubscribe events
	for (auto& eventData : m_eventDataList)
	{
		EventManager::GetInstance()->Unsubscribe(eventData.type, eventData.id);
	}

	for (auto& effect : m_pEffectPool)
	{
		if (effect)
		{
			effect->Destroy();
			delete effect;
			effect = nullptr;
		}
	}

	m_effectTemplates.clear();
	m_renderInfos.clear();
	m_pEffectQueue.clear();
}

//add effect command to effect queue
void EffectManager::AddEffect(EffectCommand command)
{
	m_pEffectQueue.push_back(command);
}

//描画情報をシーンに提出
void EffectManager::SubmitRenderInfo(
	Renderer& renderer,					//シーンの参照
	const EffectBase& effects,			//エフェクト配列の参照
	EffectRenderModel& info	//描画情報構造体
)
{
	//アクティブなオブジェクトの描画要求をシーンに提出
	if (effects.IsActive())
	{//アクティブかつ描画フラグが立っている場合
		EffectRenderModel submitInfos;	//Rendererへの提出用描画情報構造体配列
		submitInfos.reserve(info.size());					//容量確保

		//描画情報構造体配列を提出用配列にコピー
		for (auto& i : info)
		{
			i.common.color = effects.GetColor();
			i.common.uvRect = SplitSprite(effects.GetTexSplitInfo());
			i.center = effects.GetCenter();
			i.size = effects.GetSize();

			submitInfos.push_back(i);
		}

		//Rendererに描画情報を提出
		renderer.SubmitToEffectList(submitInfos);
	}
}

//effect queueからeffect poolへエフェクトを追加
void EffectManager::PushEffectFromQueue()
{
	for (auto& command : m_pEffectQueue)
	{
		auto* freeEffect = FindFreeEffectInPool();	//effect poolの空きを探す

		if (freeEffect)
		{
			//エフェクトインスタンスのテンプレートを取得
			auto effectTemplateIt = std::find_if(
				m_effectTemplates.begin(),
				m_effectTemplates.end(),
				[&](const EffectTemplate& templateItem)
				{
					return templateItem.type == command.type;
				}
			);

			//テンプレート情報を元にエフェクトインスタンスを初期化
			freeEffect->SetColor(effectTemplateIt->baseColor);
			freeEffect->SetLifeTime(effectTemplateIt->lifeTime);
			freeEffect->SetTexSplitInfo(effectTemplateIt->texSplitInfo);

			//エフェクトのパラメータを設定
			freeEffect->SetType(command.type);
			freeEffect->SetCenter(command.position);
			XMFLOAT2 size = {
				effectTemplateIt->baseSize.x * command.size.x,
				effectTemplateIt->baseSize.y * command.size.y
			};
			freeEffect->SetSize(size);
			freeEffect->SetActive(true);

			if (command.isChase && command.chaseTarget)
			{
				freeEffect->SetChase(true, command.chaseTarget);
			}
			else
			{
				freeEffect->SetChase(false);
			}
		}
	}

	//effect queueをクリア
	m_pEffectQueue.clear();
}

//effect poolの空きを探す
EffectSprite* EffectManager::FindFreeEffectInPool()
{
	for (auto& effect : m_pEffectPool)
	{
		if (!effect->IsActive())
		{
			return effect;
		}
	}
	return nullptr;
}

//エフェクトタイプから描画情報を探す
std::vector<EffectRenderInfo>* EffectManager::FindRenderInfo(EFFECT_TYPE type)
{
	for (auto& renderInfoSet : m_renderInfos)
	{
		if (renderInfoSet.type == type)
		{
			return &renderInfoSet.renderInfo;
		}
	}
	return nullptr;
}

//オブジェクトの描画情報生成
void EffectManager::PrepareRenderInfo(
	TextureManager& textureManager,	//テクスチャ管理クラスの参照
	MeshManager& meshManager		//メッシュ管理クラスの参照
)
{
	for (auto& effectTemplate : m_effectTemplates)
	{
		std::vector<EffectRenderInfo> renderInfo;	//描画情報構造体

		CreateEffectRenderInfo(
			textureManager,				//テクスチャ管理クラスの参照
			meshManager,				//メッシュ管理クラスの参照
			&renderInfo,				//描画情報構造体配列のポインタ
			effectTemplate.meshType,	//メッシュタイプ
			effectTemplate.blendMode,	//ブレンドモード
			effectTemplate.texPath		//テクスチャパス
		);

		EffectRenderSet renderSet;
		renderSet.renderInfo = renderInfo;
		renderSet.type = effectTemplate.type;

		m_renderInfos.push_back(renderSet);
	}
}