#pragma once

#include <memory>
#include <string>

#include "Document/IEditorDocument.h"

class SceneEditorDocument final : public IEditorDocument
{
public:
	SceneEditorDocument(std::unique_ptr<SceneBase> scene, std::string filePath,
		uint32_t viewportWidth, uint32_t viewportHeight, Guid sourceAssetGuid = {});
	~SceneEditorDocument() override;

	SceneBase* GetWorkingScene() override { return m_scene.get(); }
	const SceneBase* GetWorkingScene() const override { return m_scene.get(); }
	bool Save() override;
	bool CanEnterPlay() const override { return m_scene != nullptr; }
	EditorDocumentType GetType() const override { return EditorDocumentType::Scene; }
	std::string_view GetDisplayName() const override { return m_displayName; }
	Guid GetSourceAssetGuid() const override { return m_sourceAssetGuid; }

	void ReplaceScene(std::unique_ptr<SceneBase> scene, std::string filePath,
		bool markDirty = false, Guid sourceAssetGuid = {});
	bool UpdateAssetPath(const Guid& expectedGuid, std::string filePath);
	void ReleaseScene();
	const std::string& GetFilePath() const { return m_filePath; }

private:
	std::unique_ptr<SceneBase>* GetWorkingSceneOwnerSlot() override { return &m_scene; }
	void UpdateDisplayName();

	std::unique_ptr<SceneBase> m_scene;
	std::string m_filePath;
	std::string m_displayName;
	Guid m_sourceAssetGuid;
};
