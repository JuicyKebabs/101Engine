#pragma once
#include "Engine/Component/Behavior.h"
#include "Engine/Resource/AssetReference.h"

class TitleManager : public Behavior
{
public:
    void Start() override;
    void PreUpdate() override;
    void Update() override;
    void LateUpdate() override;
    void Destroy() override;
    static std::optional<TypeMetadata> BuildMetadata();

private:
	AssetReference<SceneAsset> m_nextScene; // 次のシーンへの参照
	ActorReference m_fadeImageActor;        // フェード用のUIImageアクターへの参照
	UIImage* m_fadeImage = nullptr;         // フェード用のUIImageコンポーネントへのポインタ
	bool m_isFadingOut = false;            // フェードアウト中かどうかのフラグ
	float m_fadeDuration = 1.0f;            // フェードアウトの時間（秒）
};

