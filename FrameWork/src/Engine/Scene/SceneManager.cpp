#include "SceneManager.h"

#include "SceneLoader.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Core/Path/PathManager.h"

#include <filesystem>

void SceneManager::RegisterScene(const std::string& name, std::unique_ptr<SceneBase> scene)
{
	if (name.empty() || !scene || GetSceneElement(name)) return;
	SceneElement element;
	element.name = name;
	element.pSceneBase = std::move(scene);
	m_sceneElements.push_back(std::move(element));
}

bool SceneManager::RegisterSceneAsset(const std::string& name, const Guid& sceneAssetGuid)
{
	if (name.empty() || !sceneAssetGuid.IsValid() || GetSceneElement(name) || GetSceneElement(sceneAssetGuid)) return false;
	if (m_context.pAssetManager && !ValidateSceneAsset(sceneAssetGuid)) return false;
	SceneElement element;
	element.name = name;
	element.source = SceneElement::Source::Asset;
	element.sceneAssetGuid = sceneAssetGuid;
	m_sceneElements.push_back(std::move(element));
	return true;
}

bool SceneManager::RegisterSceneFile(const std::string& name, const std::string& scenePath)
{
	const std::string resolved = std::filesystem::path(scenePath).is_absolute()
		? scenePath : PathManager::Resolve(scenePath);
	const auto guid = MetaFile::TryLoad(resolved);
	return guid && RegisterSceneAsset(name, *guid);
}

void SceneManager::SetInitialScene(const std::string& name)
{
	m_currentSceneName = name;
	m_currentSceneAssetGuid = {};
}

bool SceneManager::SetInitialScene(const Guid& sceneAssetGuid)
{
	if (!sceneAssetGuid.IsValid()) return false;
	m_currentSceneName.clear();
	m_currentSceneAssetGuid = sceneAssetGuid;
	return true;
}

void SceneManager::Initialize(EngineContext& context)
{
	m_context = context;
	if (m_currentSceneAssetGuid.IsValid()) ChangeScene(m_currentSceneAssetGuid);
	else if (!m_currentSceneName.empty()) ChangeScene(m_currentSceneName);
}

void SceneManager::PreUpdate(float deltaTime) { if (m_pCurrentScene) m_pCurrentScene->PreUpdate(deltaTime); }

void SceneManager::Update(float deltaTime)
{
	if (m_pCurrentScene) m_pCurrentScene->Update(deltaTime);
	if (!m_sceneChangeReserved) return;
	if (m_reservedSceneAssetGuid.IsValid()) ChangeScene(m_reservedSceneAssetGuid);
	else ChangeScene(m_reservedSceneName);
}

void SceneManager::LateUpdate(float deltaTime) { if (m_pCurrentScene) m_pCurrentScene->LateUpdate(deltaTime); }

void SceneManager::Finalize()
{
	if (m_pCurrentScene) m_pCurrentScene->Finalize();
	for (auto& element : m_sceneElements) element.pSceneBase.reset();
	m_pCurrentScene = nullptr;
	m_currentSceneName.clear();
	m_currentSceneAssetGuid = {};
	ClearReservation();
}

bool SceneManager::ReserveChangeScene(const std::string& name)
{
	if (!GetSceneElement(name)) return false;
	m_sceneChangeReserved = true;
	m_reservedSceneName = name;
	m_reservedSceneAssetGuid = {};
	return true;
}

bool SceneManager::ReserveChangeScene(const Guid& sceneAssetGuid)
{
	if (!ValidateSceneAsset(sceneAssetGuid)) return false;
	m_sceneChangeReserved = true;
	m_reservedSceneName.clear();
	m_reservedSceneAssetGuid = sceneAssetGuid;
	return true;
}

bool SceneManager::ReserveChangeScene(const AssetReference<SceneAsset>& sceneAsset)
{
	return sceneAsset.HasValue() && ReserveChangeScene(sceneAsset.GetGuid());
}

void SceneManager::OnRender() { if (m_pCurrentScene) m_pCurrentScene->OnRender(m_context); }

void SceneManager::ChangeScene(const std::string& name)
{
	SceneElement* element = GetSceneElement(name);
	if (!element)
	{
		DBG("SceneManager: Scene '%s' not found", name.c_str());
		ClearReservation();
		return;
	}
	ChangeScene(*element);
}

void SceneManager::ChangeScene(const Guid& sceneAssetGuid)
{
	if (!ValidateSceneAsset(sceneAssetGuid))
	{
		DBG("SceneManager: Scene asset '%s' is unavailable or has the wrong type.", sceneAssetGuid.ToString().c_str());
		ClearReservation();
		return;
	}
	SceneElement* element = GetSceneElement(sceneAssetGuid);
	if (!element)
	{
		const AssetEntry* entry = m_context.pAssetManager->GetAssetEntry(sceneAssetGuid);
		SceneElement candidate;
		candidate.name = entry->relativePath;
		candidate.source = SceneElement::Source::Asset;
		candidate.sceneAssetGuid = sceneAssetGuid;
		m_sceneElements.push_back(std::move(candidate));
		element = &m_sceneElements.back();
	}
	ChangeScene(*element);
}

void SceneManager::ChangeScene(SceneElement& element)
{
	std::unique_ptr<SceneBase> loadedScene;
	if (element.source == SceneElement::Source::Asset)
	{
		if (!ValidateSceneAsset(element.sceneAssetGuid)) { ClearReservation(); return; }
		const std::string path = m_context.pAssetManager->GetAssetPath(element.sceneAssetGuid);
		SceneLoadResult load = SceneLoader::LoadCandidate(path, m_context);
		if (!load)
		{
			DBG("SceneManager: Failed to load scene from '%s' at '%s': %s", path.c_str(), load.error.path.c_str(), load.error.message.c_str());
			ClearReservation();
			return;
		}
		loadedScene = std::move(load.scene);
	}
	else if (!element.pSceneBase)
	{
		DBG("SceneManager: Runtime scene '%s' is not initialized.", element.name.c_str());
		ClearReservation();
		return;
	}

	SceneBase* nextScene = loadedScene ? loadedScene.get() : element.pSceneBase.get();
	nextScene->SetSceneManager(this);
	nextScene->SetViewportSize(m_viewportWidth, m_viewportHeight);
	SceneBase* previousCurrent = m_pCurrentScene;
	std::unique_ptr<SceneBase> replacedTarget;
	if (loadedScene)
	{
		replacedTarget = std::move(element.pSceneBase);
		element.pSceneBase = std::move(loadedScene);
		nextScene = element.pSceneBase.get();
	}
	m_pCurrentScene = nullptr;
	ClearReservation();
	if (previousCurrent && previousCurrent != nextScene) previousCurrent->Finalize();
	if (replacedTarget && replacedTarget.get() != previousCurrent) replacedTarget->Finalize();
	// Finalize hooks must not be able to enqueue an unintended follow-up transition.
	ClearReservation();
	m_pCurrentScene = nextScene;
	m_currentSceneName = element.name;
	m_currentSceneAssetGuid = element.source == SceneElement::Source::Asset ? element.sceneAssetGuid : Guid{};
	DBG("SceneManager: Changed to scene '%s'", element.name.c_str());
}

void SceneManager::ClearReservation()
{
	m_sceneChangeReserved = false;
	m_reservedSceneName.clear();
	m_reservedSceneAssetGuid = {};
}

bool SceneManager::ValidateSceneAsset(const Guid& sceneAssetGuid) const
{
	if (!sceneAssetGuid.IsValid() || !m_context.pAssetManager) return false;
	const AssetEntry* entry = m_context.pAssetManager->GetAssetEntry(sceneAssetGuid);
	return entry && entry->type == AssetType::Scene;
}

SceneElement* SceneManager::GetSceneElement(const std::string& name)
{
	for (auto& element : m_sceneElements) if (element.name == name) return &element;
	return nullptr;
}

SceneElement* SceneManager::GetSceneElement(const Guid& sceneAssetGuid)
{
	for (auto& element : m_sceneElements)
		if (element.source == SceneElement::Source::Asset && element.sceneAssetGuid == sceneAssetGuid) return &element;
	return nullptr;
}

const CameraInfo* SceneManager::GetCameraInfo()
{
	if (!m_pCurrentScene) return nullptr;
	auto cameraSystem = m_pCurrentScene->GetCameraSystem();
	if (!cameraSystem)
	{
		DBG("Error: Current scene does not have a camera system. Unable to retrieve camera information.");
		return nullptr;
	}
	return cameraSystem->GetCameraInfo();
}

void SceneManager::SetViewportSize(UINT width, UINT height)
{
	if (width == 0 || height == 0) return;
	m_viewportWidth = width;
	m_viewportHeight = height;
	if (m_pCurrentScene) m_pCurrentScene->SetViewportSize(width, height);
}
