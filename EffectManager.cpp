#include "EffectManager.h"
#include "Renderer.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "EventManager.h"

using namespace DirectX;

//初期化
void EffectManager::Initialize(TextureManager& textureManager, MeshManager& meshManager)
{
	for(auto& ptr : m_pEffectPool)
	{
		if (ptr) continue;
		ptr = new EffectBase(); //エフェクトインスタンス生成
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

	//エフェクトコマンド追加
	using args = EffectCommand;
	EventManager::GetInstance()->Subscribe<args>(
		EventType::ADD_EFFECT_COMMAND,
		[this](std::shared_ptr<args> data)
		{
			AddEffectCommand(*data);
		}
	);
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
	for(auto& effect : m_pEffectPool)
	{
		if (!effect || !effect->IsActive()) continue;

		auto renderInfo = FindRenderInfo(effect->GetType());

		for(auto& info : *renderInfo)
		{
			info.billboardType = BILLBOARD_TYPE::BILLBOARD_SPHERICAL;
		}

		SubmitRenderInfo(renderer, *effect, *renderInfo);
	}
}

//終了
void EffectManager::Finalize()
{
	for(auto& effect : m_pEffectPool)
	{
		if(effect)
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

//エフェクトコマンド追加
void EffectManager::AddEffectCommand(EffectCommand command)
{
	m_pEffectQueue.push_back(command);
}

//描画情報をシーンに提出
void EffectManager::SubmitRenderInfo(
	Renderer& renderer,					//シーンの参照
	const EffectBase& effects,			//エフェクト配列の参照
	std::vector<WorldRenderInfo>& info	//描画情報構造体
)
{
	//アクティブなオブジェクトの描画要求をシーンに提出
	if (effects.IsActive())
	{//アクティブかつ描画フラグが立っている場合
		std::vector<WorldRenderInfo> submitInfos;	//Rendererへの提出用描画情報構造体配列
		submitInfos.reserve(info.size());					//容量確保

		//描画情報構造体配列を提出用配列にコピー
		for (auto& i : info)
		{
			i.common.color = effects.GetColor();
			i.common.uvRect = SplitSprite(effects.GetTexSplitInfo());
			i.position = effects.GetCenter();
			i.scale = effects.GetSize();

			submitInfos.push_back(i);
		}

		//Rendererに描画情報を提出
		renderer.SubmitToWorldList(submitInfos);
	}
}

//effect queueからeffect poolへエフェクトを追加
void EffectManager::PushEffectFromQueue()
{
	for(auto& command : m_pEffectQueue)
	{
		auto* freeEffect = FindFreeEffectInPool();	//effect poolの空きを探す
		
		if(freeEffect)
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
			XMFLOAT3 size = {
				effectTemplateIt->baseSize.x * command.size.x,
				effectTemplateIt->baseSize.y * command.size.y
				,effectTemplateIt->baseSize.z* command.size.z
			};
			freeEffect->SetSize(size);
			freeEffect->SetActive(true);

			if(command.isChase && command.chaseTarget)
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
EffectBase* EffectManager::FindFreeEffectInPool()
{
	for(auto& effect : m_pEffectPool)
	{
		if(!effect->IsActive())
		{
			return effect;
		}
	}
	return nullptr;
}

//effect poolからエフェクトを探す
EffectBase* EffectManager::FindEffectInPool(EFFECT_TYPE type)
{
	for (auto& effect : m_pEffectPool)
	{
		if (effect->GetType() == type)
		{
			return effect;
		}
	}

	return nullptr;
}

//エフェクトタイプから描画情報を探す
std::vector<WorldRenderInfo>* EffectManager::FindRenderInfo(EFFECT_TYPE type)
{
	for(auto& renderInfoSet : m_renderInfos)
	{
		if(renderInfoSet.type == type)
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
	for(auto& effectTemplate : m_effectTemplates)
	{
		std::vector<WorldRenderInfo> renderInfo;	//描画情報構造体


		CreateRenderInfo(
			textureManager,				//テクスチャ管理クラスの参照
			meshManager,				//メッシュ管理クラスの参照
			&renderInfo,				//描画情報構造体配列のポインタ
			effectTemplate.meshType,	//メッシュタイプ
			effectTemplate.psoKey,	//ブレンドモード
			effectTemplate.texPath		//テクスチャパス
		);

		EffectRenderSet renderSet;
		renderSet.renderInfo = renderInfo;
		renderSet.type = effectTemplate.type;

		m_renderInfos.push_back(renderSet);
	}
}