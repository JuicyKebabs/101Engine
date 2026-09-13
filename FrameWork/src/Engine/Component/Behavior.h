#pragma once
#include "Component.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Resource/AssetReference.h"
#include <vector>
#include <memory>

// BehaviorComponent Class
class Behavior : public Component
{
public:
	Behavior() = default;
	virtual ~Behavior() = default;

	void OnStartOverride() override { Start(); }
	void PreUpdateOverride(float deltaTime) override { PreUpdate(); }
	void UpdateOverride(float deltaTime) override { Update(); }
	void LateUpdateOverride(float deltaTime) override { LateUpdate(); }
	void OnDestroyOverride() override { Destroy(); }

	virtual void Start() {};
	virtual void PreUpdate() {};
	virtual void Update() {};
	virtual void LateUpdate() {};
	virtual void Destroy() {};

protected:

	// Helper function to change the scene from within a behavior component
	bool ChangeScene(const std::string& sceneName);
	bool ChangeScene(const Guid& sceneAssetGuid);
	bool ChangeScene(const AssetReference<SceneAsset>& sceneAsset);
};
