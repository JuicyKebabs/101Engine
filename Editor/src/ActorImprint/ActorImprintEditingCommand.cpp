#include "ActorImprintEditingCommand.h"

#include "Engine/Scene/SceneBase.h"

ActorImprintEditingCommand::ActorImprintEditingCommand(SceneBase& scene,
	ActorImprintEditingObjectMap& objectMap, std::unique_ptr<IEditorCommand> command)
	: m_scene(&scene), m_objectMap(&objectMap), m_command(std::move(command))
{}

bool ActorImprintEditingCommand::Execute()
{
	if (!m_scene || !m_objectMap || !m_command) return false;
	if (!m_hasExecuted)
	{
		if (!m_objectMap->CaptureSnapshot(*m_scene, m_before)) return false;
		if (!m_command->Execute())
		{
			m_structuralResult = m_command->GetStructuralResult();
			return false;
		}
		m_structuralResult = m_command->GetStructuralResult();
		if (!m_objectMap->Reconcile(*m_scene) || !m_objectMap->CaptureSnapshot(*m_scene, m_after))
			return false;
		m_hasExecuted = true;
		return true;
	}

	if (!m_command->Execute())
	{
		m_structuralResult = m_command->GetStructuralResult();
		return false;
	}
	m_structuralResult = m_command->GetStructuralResult();
	return m_objectMap->RestoreSnapshot(*m_scene, m_after);
}

bool ActorImprintEditingCommand::Undo()
{
	if (!m_scene || !m_objectMap || !m_command || !m_hasExecuted) return false;
	if (!m_command->Undo())
	{
		m_structuralResult = m_command->GetStructuralResult();
		return false;
	}
	m_structuralResult = m_command->GetStructuralResult();
	return m_objectMap->RestoreSnapshot(*m_scene, m_before);
}
