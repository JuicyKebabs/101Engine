#pragma once

#include "SceneBase.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetReference.h"

struct SceneElement
{
	enum class Source { Runtime, Asset };
	std::string name;
	Source source = Source::Runtime;
	std::unique_ptr<SceneBase> pSceneBase;
	Guid sceneAssetGuid;
};

class SceneManager
{
public:
	SceneManager() = default;
	~SceneManager() = default;
	void Initialize(EngineContext& context);
	void PreUpdate(float deltaTime);
	void Update(float deltaTime);
	void LateUpdate(float deltaTime);
	void OnRender();
	void Finalize();
	void RegisterScene(const std::string& name, std::unique_ptr<SceneBase> scene);
	bool RegisterSceneAsset(const std::string& name, const Guid& sceneAssetGuid);
	// Compatibility entry point. Resolves the file's sidecar once and stores only its GUID.
	bool RegisterSceneFile(const std::string& name, const std::string& scenePath);
	void SetInitialScene(const std::string& name);
	bool SetInitialScene(const Guid& sceneAssetGuid);
	bool ReserveChangeScene(const std::string& name);
	bool ReserveChangeScene(const Guid& sceneAssetGuid);
	bool ReserveChangeScene(const AssetReference<SceneAsset>& sceneAsset);
	const CameraInfo* GetCameraInfo();
	const std::string& GetCurrentSceneName() const { return m_currentSceneName; }
	const Guid& GetCurrentSceneAssetGuid() const { return m_currentSceneAssetGuid; }
	SceneBase* GetCurrentScene() const { return m_pCurrentScene; }
	void SetViewportSize(UINT width, UINT height);

private:
	EngineContext m_context;
	std::vector<SceneElement> m_sceneElements;
	SceneBase* m_pCurrentScene = nullptr;
	std::string m_currentSceneName;
	Guid m_currentSceneAssetGuid;
	bool m_sceneChangeReserved = false;
	std::string m_reservedSceneName;
	Guid m_reservedSceneAssetGuid;
	UINT m_viewportWidth = 1;
	UINT m_viewportHeight = 1;
	void ChangeScene(const std::string& name);
	void ChangeScene(const Guid& sceneAssetGuid);
	void ChangeScene(SceneElement& element);
	void ClearReservation();
	bool ValidateSceneAsset(const Guid& sceneAssetGuid) const;
	SceneElement* GetSceneElement(const std::string& name);
	SceneElement* GetSceneElement(const Guid& sceneAssetGuid);
};
