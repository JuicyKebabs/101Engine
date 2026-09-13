#pragma once

#include "Document/IEditorDocument.h"

#include <memory>
#include <string>

class ActorImprintEditingContext;

class ActorImprintEditorDocument final : public IEditorDocument
{
public:
	ActorImprintEditorDocument(std::unique_ptr<ActorImprintEditingContext> context,
		uint32_t viewportWidth, uint32_t viewportHeight);
	~ActorImprintEditorDocument() override;

	SceneBase* GetWorkingScene() override;
	const SceneBase* GetWorkingScene() const override;
	bool Save() override;
	bool PrepareSave();
	void CommitPreparedSave();
	bool RollbackPreparedSave();
	bool CanEnterPlay() const override { return false; }
	EditorDocumentType GetType() const override { return EditorDocumentType::ActorImprint; }
	std::string_view GetDisplayName() const override { return m_displayName; }
	Guid GetSourceAssetGuid() const override;
	bool ExecuteCommand(std::unique_ptr<IEditorCommand> command) override;

	ActorImprintEditingContext* GetEditingContext() { return m_context.get(); }
	const ActorImprintEditingContext* GetEditingContext() const { return m_context.get(); }

private:
	std::unique_ptr<SceneBase>* GetWorkingSceneOwnerSlot() override;

	std::unique_ptr<ActorImprintEditingContext> m_context;
	std::string m_displayName;
};
