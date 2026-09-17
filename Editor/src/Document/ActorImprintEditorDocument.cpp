#include "ActorImprintEditorDocument.h"

#include "ActorImprint/ActorImprintEditingCommand.h"
#include "ActorImprint/ActorImprintEditingContext.h"
#include "Engine/Scene/SceneBase.h"

#include <filesystem>

ActorImprintEditorDocument::ActorImprintEditorDocument(
	std::unique_ptr<ActorImprintEditingContext> context,
	uint32_t viewportWidth,
	uint32_t viewportHeight)
	: IEditorDocument(viewportWidth, viewportHeight), m_context(std::move(context))
{
	if (m_context)
	{
		m_displayName = std::filesystem::path(m_context->GetAssetPath()).filename().string();
	}

	if (m_displayName.empty())
	{
		m_displayName = "ActorImprint";
	}
}

ActorImprintEditorDocument::~ActorImprintEditorDocument() = default;

SceneBase* ActorImprintEditorDocument::GetWorkingScene()
{
	return m_context ? m_context->GetWorkingScene() : nullptr;
}

const SceneBase* ActorImprintEditorDocument::GetWorkingScene() const
{
	return m_context ? m_context->GetWorkingScene() : nullptr;
}

bool ActorImprintEditorDocument::Save()
{
	if (!PrepareSave())
	{
		return false;
	}

	CommitPreparedSave();
	return true;
}

bool ActorImprintEditorDocument::PrepareSave()
{
	return m_context && m_context->Save();
}

void ActorImprintEditorDocument::CommitPreparedSave()
{
	if (!m_context)
	{
		return;
	}

	m_context->CommitPendingSave();
	MarkClean();
}

bool ActorImprintEditorDocument::RollbackPreparedSave()
{
	return m_context && m_context->RollbackPendingSave();
}

Guid ActorImprintEditorDocument::GetSourceAssetGuid() const
{
	return m_context ? m_context->GetAssetGuid() : Guid{};
}

bool ActorImprintEditorDocument::ExecuteCommand(std::unique_ptr<IEditorCommand> command)
{
	SceneBase* scene = GetWorkingScene();

	if (!m_context || !scene || !command)
	{
		return false;
	}

	return IEditorDocument::ExecuteCommand(std::make_unique<ActorImprintEditingCommand>(
		*scene, m_context->GetObjectMap(), std::move(command)));
}

std::unique_ptr<SceneBase>* ActorImprintEditorDocument::GetWorkingSceneOwnerSlot()
{
	return m_context ? m_context->GetWorkingSceneOwnerSlot() : nullptr;
}
